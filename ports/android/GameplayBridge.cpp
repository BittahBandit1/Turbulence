#include "moderngekko/runtime.hpp"
#include "moderngekko/module.h"
#include "turbulence_options.hpp"
#include "turbulence_prompts.hpp"
#include "Common/FileUtil.h"
#include "Common/HookableEvent.h"
#include "Core/Core.h"
#include "Core/HW/GCPad.h"
#include "InputCommon/GCPadStatus.h"
#include "Core/DolphinAnalytics.h"
#include "Core/System.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerInterface/Touch/InputOverrider.h"
#include "VideoCommon/Present.h"
#include "Common/Config/Config.h"
#include "Core/Config/GraphicsSettings.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/VulkanContext.h"
#include "VideoBackends/Vulkan/VulkanLoader.h"
#include "VideoCommon/PerformanceMetrics.h"
#include <chrono>
#include "VideoCommon/VideoEvents.h"
#include "imgui.h"
#include <android/native_window_jni.h>
#include <android/log.h>
#include <jni.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <filesystem>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <vector>
extern "C" const ModernGekkoModuleDesc* staticrecomp_get_module();
namespace {
// Dolphin writes the Vulkan pipeline cache only on a clean shutdown, and an
// Android app is usually killed instead. Drivers without their own disk cache
// (Turnip) then recompile every startup shader on each launch (~45 s), so
// also save after the first frame and whenever the app is backgrounded.
void SavePipelineCache(const char* why){
 if(!Vulkan::g_object_cache||!Vulkan::g_vulkan_context)return;
 Vulkan::g_object_cache->SavePipelineCache();
 std::fprintf(stderr,"[turbulence] saved Vulkan pipeline cache (%s)\n",why);
}
std::mutex lock;
std::jthread worker;
moderngekko::Runtime* active=nullptr;
int state=0;
bool stopping=false;
std::string message;
std::atomic<long long> frames{0};
std::atomic<bool> toggle{false};
std::atomic<unsigned> buttons{0};
struct Pointer{float x,y;bool down;};
std::mutex pointer_lock;
std::vector<Pointer> pointers;
void Log(const char* text){__android_log_print(ANDROID_LOG_INFO,"Turbulence","%s",text);}
void Stop(){std::lock_guard guard(lock);stopping=true;if(active)active->RequestStop();}
turbulence::OptionsInput MenuInput(){
 std::vector<Pointer> events;
 {std::lock_guard guard(pointer_lock);events.swap(pointers);}
 auto& io=ImGui::GetIO();
 for(auto p:events){io.AddMousePosEvent(p.x*io.DisplaySize.x,p.y*io.DisplaySize.y);io.AddMouseButtonEvent(0,p.down);}
 turbulence::OptionsInput input{};auto mask=buttons.load();
 for(int i=0;i<turbulence::Count;++i)input[i]=(mask&(1u<<i))!=0;
 input[turbulence::Toggle]=input[turbulence::Toggle]||toggle.exchange(false);
 return input;
}
std::string Text(JNIEnv* env,jstring s){const char* p=env->GetStringUTFChars(s,nullptr);std::string text(p);env->ReleaseStringUTFChars(s,p);return text;}
}
#define JNI_METHOD(name) Java_io_github_bittahbandit1_turbulence_MainActivity_##name
// Custom Vulkan drivers (Mesa Turnip and other adrenotools packages): the
// activity installs a package's files into <driver_root>/Extracted and passes
// its meta.json libraryName to nativeStart; libadrenotools loads it through the
// hook libraries shipped in the APK's native library directory.
extern "C" JNIEXPORT void JNICALL JNI_METHOD(nativeSetDriverDirectories)(JNIEnv* env,jclass,jstring root,jstring libs){
 static bool set=false;if(set)return;set=true;
 File::SetGpuDriverDirectories(Text(env,root),Text(env,libs));
}
extern "C" JNIEXPORT jboolean JNICALL JNI_METHOD(nativeSupportsCustomDriver)(JNIEnv*,jclass){return Vulkan::SupportsCustomDriver();}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(nativeStart)(JNIEnv* env,jclass,jobject surface,jstring game,jstring user,jstring sys,jfloat scale,jstring driver_lib){
 {std::lock_guard guard(lock);if(state==1)return;}
 if(worker.joinable())worker.join();
 auto* window=ANativeWindow_fromSurface(env,surface);
 if(!window)return;
 std::filesystem::path root=Text(env,game),profile=Text(env,user),system=Text(env,sys);
 const std::string driver=driver_lib?Text(env,driver_lib):std::string();
 {std::lock_guard guard(lock);state=1;stopping=false;message="Starting game";}
 frames=0;toggle=false;buttons=0;
 worker=std::jthread([=]{
  try{
   std::filesystem::create_directories(profile/"Config");
   std::freopen((profile/"gameplay.log").c_str(),"a",stderr);
   std::setvbuf(stderr,nullptr,_IONBF,0);
   Log("Starting native ARM64 Sonic Riders runtime");
   DolphinAnalytics::AndroidSetGetValFunc([](std::string){return std::string("Turbulence Android");});
   static bool sys_set=false;if(!sys_set){File::SetSysDirectory(system.string());sys_set=true;}
   std::fprintf(stderr,"[turbulence] Sys directory: %s\n",File::GetSysDirectory().c_str());
   if(!std::filesystem::exists(profile/"Config/GFX.ini"))std::filesystem::copy_file(system.parent_path()/"GFX.ini",profile/"Config/GFX.ini");
   moderngekko::RuntimeConfig config;
   config.game_root=root;config.user_directory=profile;
   config.module=moderngekko::ModuleSource::AttachedDescriptor(staticrecomp_get_module());
   config.native_window=window;config.native_metal_layer=window;config.native_surface_scale=scale;
   config.graphics.backend="Vulkan";config.audio.backend="AAudio";config.show_fps_in_title=false;
   config.input.blocks_input=[] {return turbulence::OptionsMenuBlocksInput();};
   auto created=moderngekko::Runtime::Create(std::move(config));
   if(!created)throw std::runtime_error(created.error->message);
   Log("Runtime created; launching compiled game");
   // Empty selects the system driver. Session-only: the activity owns the choice.
   Config::SetCurrent(Config::GFX_DRIVER_LIB_NAME,driver);
   if(!driver.empty()){
    const bool supported=Vulkan::SupportsCustomDriver();
    std::fprintf(stderr,"[turbulence] GPU driver requested: %s%s\n",driver.c_str(),supported?"":" (custom drivers unsupported on this device; using system driver)");
    __android_log_print(ANDROID_LOG_INFO,"Turbulence","GPU driver requested: %s (supported=%d)",driver.c_str(),supported);
   }
   {auto guard=ControllerEmu::EmulatedController::GetStateLock();ciface::Touch::RegisterGameCubeInputOverrider(0);}
   {std::lock_guard guard(lock);active=created.runtime.get();if(stopping)active->RequestStop();message="Running";}
   struct Clear{~Clear(){std::lock_guard guard(lock);active=nullptr;}}clear;
   auto present=GetVideoEvents().after_present_event.Register([](const PresentInfo& info){
    frames=info.frame_count;
    static bool driver_reported=false;
    if(!driver_reported&&Vulkan::g_vulkan_context){
     driver_reported=true;SavePipelineCache("startup shaders compiled");const auto& d=Vulkan::g_vulkan_context->GetDeviceInfo();
     std::fprintf(stderr,"[turbulence] Vulkan device: %s (driver 0x%08x)\n",d.deviceName,d.driverVersion);
     __android_log_print(ANDROID_LOG_INFO,"Turbulence","Vulkan device: %s (driver 0x%08x)",d.deviceName,d.driverVersion);
    }
    if(info.frame_count && info.frame_count%300==0)__android_log_print(ANDROID_LOG_INFO,"Turbulence","Presented frame %llu",static_cast<unsigned long long>(info.frame_count));
    // Speed log for on-device measurement (adb logcat -s Turbulence).
    static auto last=std::chrono::steady_clock::now();
    const auto now=std::chrono::steady_clock::now();
    if(now-last>=std::chrono::seconds(5)){
     last=now;const auto& m=Core::System::GetInstance().GetPerfMetrics();
     __android_log_print(ANDROID_LOG_INFO,"Turbulence","Perf fps=%.1f vps=%.1f speed=%.1f%%",m.GetFPS(),m.GetVPS(),m.GetSpeed()*100.0);
    }
   });
   auto menu=std::make_unique<turbulence::OptionsMenu>(profile,nullptr,Stop,MenuInput,[]{return turbulence::MenuButtonCapture{};});
   auto result=created.runtime->Run();
   menu.reset();present.reset();
   {std::lock_guard guard(lock);active=nullptr;}
   {auto guard=ControllerEmu::EmulatedController::GetStateLock();ciface::Touch::UnregisterGameCubeInputOverrider(0);}
   created.runtime.reset();
   {std::lock_guard guard(lock);state=result.error?3:2;message=result.error?result.error->message:"Game stopped";Log(message.c_str());}
  }catch(const std::exception& error){
   std::lock_guard guard(lock);active=nullptr;state=3;message=error.what();Log(message.c_str());std::fprintf(stderr,"Android startup error: %s\n",error.what());
  }
  std::fflush(stderr);ANativeWindow_release(window);
 });
}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(nativeStop)(JNIEnv*,jclass){Stop();}
extern "C" JNIEXPORT jint JNICALL JNI_METHOD(nativeState)(JNIEnv*,jclass){std::lock_guard guard(lock);return state;}
extern "C" JNIEXPORT jstring JNICALL JNI_METHOD(nativeMessage)(JNIEnv* env,jclass){std::lock_guard guard(lock);return env->NewStringUTF(message.c_str());}
extern "C" JNIEXPORT jlong JNICALL JNI_METHOD(nativeFrames)(JNIEnv*,jclass){return frames.load();}
extern "C" JNIEXPORT jboolean JNICALL JNI_METHOD(nativeMenuOpen)(JNIEnv*,jclass){return turbulence::OptionsMenuBlocksInput();}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(nativeToggleMenu)(JNIEnv*,jclass){toggle=true;}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(nativePointer)(JNIEnv*,jclass,jfloat x,jfloat y,jboolean down){std::lock_guard guard(pointer_lock);if(pointers.size()<256)pointers.push_back({x,y,down!=0});}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(nativeInput)(JNIEnv* env, jclass, jdoubleArray input, jint menu_mask) {
  constexpr int control_count = 18;
  if (!input || env->GetArrayLength(input) != control_count) return;
  jdouble values[control_count];
  env->GetDoubleArrayRegion(input, 0, control_count, values);
  if (env->ExceptionCheck()) return;

  buttons.store(menu_mask);
  auto guard = ControllerEmu::EmulatedController::GetStateLock();
  const bool blocked = turbulence::OptionsMenuBlocksInput();
  for (int id = 0; id < control_count; ++id) {
    const double value = !blocked && std::isfinite(values[id]) ? std::clamp(values[id], -1.0, 1.0) : 0.0;
    ciface::Touch::SetControlState(0, static_cast<ciface::Touch::ControlID>(id), value);
  }
}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(nativePause)(JNIEnv*,jclass,jboolean paused){
 std::lock_guard guard(lock);
 if(active&&Core::IsRunning(Core::System::GetInstance())){if(paused){active->Pause();Core::QueueHostJob([](Core::System&){SavePipelineCache("app backgrounded");});}else active->Resume();}
}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(nativeResize)(JNIEnv*,jclass){Core::QueueHostJob([](Core::System&){if(g_presenter)g_presenter->ResizeSurface();});}

extern "C" JNIEXPORT void JNICALL JNI_METHOD(nativePromptStyle)(JNIEnv*,jclass,jint style){turbulence::SetAutomaticControllerStyle(style);}
