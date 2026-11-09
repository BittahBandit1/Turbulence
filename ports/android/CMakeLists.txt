cmake_minimum_required(VERSION 3.25)
project(TurbulenceAndroid LANGUAGES C CXX)
if(NOT ANDROID)
 message(FATAL_ERROR "Use the Android NDK CMake toolchain")
endif()
set(TURBULENCE_ANDROID ON CACHE BOOL "Turbulence native Android host" FORCE)
set(TURBULENCE_OPTIONS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../src" CACHE PATH "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(MODERNGEKKO_ENABLE_DYNAMIC_MODULES OFF CACHE BOOL "" FORCE)
set(ENABLE_VULKAN ON CACHE BOOL "" FORCE)
set(ENABLE_SDL OFF CACHE BOOL "" FORCE)
set(USE_SYSTEM_LIBS OFF CACHE STRING "" FORCE)
set(USE_BUNDLED_MOLTENVK OFF CACHE BOOL "" FORCE)
set(DOLPHIN "${CMAKE_CURRENT_SOURCE_DIR}/../../lib/ModernGekko/vendor/dolphin")
add_library(androidcommon STATIC AndroidJNI.cpp "${DOLPHIN}/Source/Android/jni/AndroidCommon/AndroidCommon.cpp")
set_target_properties(androidcommon PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_features(androidcommon PUBLIC cxx_std_23)
target_include_directories(androidcommon PRIVATE "${DOLPHIN}/Source/Android" "${DOLPHIN}/Source/Core")
target_link_libraries(androidcommon PRIVATE android log)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../lib/ModernGekko" moderngekko)
target_link_libraries(androidcommon PRIVATE common fmt::fmt)
# NDK r27 ships stop_token/jthread in its experimental C++20 library.
target_compile_definitions(moderngekko PRIVATE _LIBCPP_ENABLE_EXPERIMENTAL)
set(GAME_ID GXEE8P CACHE STRING "" FORCE)
set(GENERATED_DIR "${TURBULENCE_ANDROID_GENERATED_DIR}" CACHE PATH "" FORCE)
set(REL_GENERATED_DIR "${TURBULENCE_ANDROID_REL_GENERATED_DIR}" CACHE PATH "" FORCE)
  set(RECOMPCORE_MODULE_STATIC ON CACHE BOOL "" FORCE)
# Thin LTO inlines the paired-single helpers into the generated code (as the
# desktop module does); without it they were ~45% of the CPU thread.
set(RECOMPCORE_MODULE_ENABLE_IPO ON CACHE BOOL "" FORCE)
add_subdirectory("${DOLPHIN}/module-template" game-module)
add_library(turbulence SHARED GameplayBridge.cpp AAudioStream.cpp
 "${CMAKE_CURRENT_SOURCE_DIR}/../../lib/ModernGekko/tools/dol_patch.cpp"
)
target_compile_features(turbulence PRIVATE cxx_std_23)
target_compile_definitions(turbulence PRIVATE _LIBCPP_ENABLE_EXPERIMENTAL TURBULENCE_ANDROID=1)
target_include_directories(turbulence PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../src"
 "${CMAKE_CURRENT_SOURCE_DIR}/../../lib/ModernGekko/tools" "${DOLPHIN}/Source/Core" "${DOLPHIN}/Source/Android")
target_link_libraries(turbulence PRIVATE ModernGekko::Runtime core imgui gGXEE8P_recomp aaudio android log c++experimental)

# Support devices with 16 KiB host pages as well as older 4 KiB devices.
target_link_options(turbulence PRIVATE "-Wl,-z,max-page-size=16384")
