#!/usr/bin/env python3
"""Build a personal ARM64 Android test APK using Google's SDK/NDK tools."""
import argparse
from pathlib import Path
import os,shutil,subprocess,zipfile
from mobile_common import generated_sources
ROOT=Path(__file__).resolve().parents[1]
# libadrenotools hook libraries: loaded from the APK's native library directory
# when a custom GPU driver (e.g. Mesa Turnip) is selected.
HOOKS=('hook_impl','main_hook','file_redirect_hook','gsl_alloc_hook')
os.environ.setdefault("CCACHE_DIR",str(ROOT/"build/codex-ccache"))
def run(*args,**kwargs):subprocess.run([str(a) for a in args],cwd=ROOT,check=True,**kwargs)
def llvm_main(c_generated):
 """LLVM-backend objects for the C output's main.dol, built for Android ARM64.
 Needs lib/DolRecomp built with -DDOLRECOMP_ENABLE_LLVM=ON in build-llvm."""
 out=ROOT/'build/llvm-android/dol';generated=out/'generated'
 if not (generated/'generated.h').is_file():
  dolrecomp=ROOT/'lib/DolRecomp/build-llvm/dolrecomp'
  if not dolrecomp.is_file():raise SystemExit('Build DolRecomp with LLVM first: cmake -S lib/DolRecomp -B lib/DolRecomp/build-llvm -G Ninja -DCMAKE_BUILD_TYPE=Release -DDOLRECOMP_ENABLE_LLVM=ON -DLLVM_DIR=<llvm 19/20>/lib/cmake/llvm && cmake --build lib/DolRecomp/build-llvm --target dolrecomp')
  out.mkdir(parents=True,exist_ok=True)
  run(dolrecomp,'-j',os.cpu_count() or 4,'--backend=llvm','--targets','aarch64','--cpu','gekko','--gamecube',c_generated/'main.dol',out,
      env=dict(os.environ,DOLRECOMP_LLVM_TARGET='aarch64-unknown-linux-android26'))
 shutil.copy2(c_generated/'main.dol',generated/'main.dol')
 return generated
def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--generated',type=Path)
 parser.add_argument('--rel-generated',type=Path)
 parser.add_argument('--sdk',type=Path,default=Path(os.environ.get('ANDROID_HOME',ROOT/'build/android-sdk')))
 parser.add_argument('--jobs',type=int,default=6)
 parser.add_argument('--package-only',action='store_true')
 parser.add_argument('--llvm',action='store_true',help='main.dol from DolRecomp\'s LLVM backend (REL stays C); generated on first use')
 args=parser.parse_args()
 sdk=args.sdk.resolve();build=ROOT/'build/ports/android-arm64';tools=sdk/'build-tools/35.0.0';ndk=sdk/'ndk/27.3.13750724'
 java=Path(os.environ.get('JAVA_HOME') or subprocess.check_output(['/usr/libexec/java_home'],text=True).strip())
 env=dict(os.environ,JAVA_HOME=str(java))
 if not args.package_only:
  args.generated,args.rel_generated=generated_sources(ROOT,args.generated,args.rel_generated)
  if args.llvm:args.generated=llvm_main(args.generated)
  run('cmake','-S',ROOT/'ports/android','-B',build,'-G','Ninja',
      '-DCMAKE_TOOLCHAIN_FILE='+str(ndk/'build/cmake/android.toolchain.cmake'),
      '-DANDROID_ABI=arm64-v8a','-DANDROID_PLATFORM=android-26','-DCMAKE_BUILD_TYPE=Release',
      '-DTURBULENCE_ANDROID_GENERATED_DIR='+str(args.generated.resolve()),
      '-DTURBULENCE_ANDROID_REL_GENERATED_DIR='+str(args.rel_generated.resolve()))
  run('cmake','--build',build,'--target','turbulence',*HOOKS,'-j',args.jobs)
 staging=build/'apk-stage'
 if staging.exists():shutil.rmtree(staging)
 assets=staging/'assets';assets.mkdir(parents=True)
 shutil.copytree(ROOT/'lib/ModernGekko/vendor/dolphin/Data/Sys',assets/'Sys')
 shutil.copy2(ROOT/'ports/COPYING',assets/'COPYING')
 (assets/'GFX.ini').write_text((ROOT/'config/GFX.ini').read_text().replace('InternalResolution = 3','InternalResolution = 1'))
 classes=staging/'classes';classes.mkdir()
 sources=sorted((ROOT/'ports/android/java').rglob('*.java'))
 run(java/'bin/javac','-source','8','-target','8','-classpath',sdk/'platforms/android-35/android.jar','-d',classes,*sources)
 dex=staging/'dex';dex.mkdir()
 run(tools/'d8','--min-api','26','--lib',sdk/'platforms/android-35/android.jar','--output',dex,*sorted(classes.rglob('*.class')),env=env)
 unsigned=staging/'unsigned.apk'
 run(tools/'aapt2','link','--manifest',ROOT/'ports/android/AndroidManifest.xml','-I',sdk/'platforms/android-35/android.jar','-A',assets,'-o',unsigned)
 lib=staging/'libturbulence.so';shutil.copy2(build/'libturbulence.so',lib)
 llvm=ndk/'toolchains/llvm/prebuilt/darwin-x86_64'
 run(llvm/'bin/llvm-strip','--strip-unneeded',lib)
 with zipfile.ZipFile(unsigned,'a',zipfile.ZIP_DEFLATED) as archive:
  archive.write(dex/'classes.dex','classes.dex')
  archive.write(lib,'lib/arm64-v8a/libturbulence.so')
  archive.write(llvm/'sysroot/usr/lib/aarch64-linux-android/libc++_shared.so','lib/arm64-v8a/libc++_shared.so')
  for hook in HOOKS:
   built=build/'moderngekko/vendor/dolphin/Externals/libadrenotools/src/hook'/f'lib{hook}.so'
   staged=staging/built.name;shutil.copy2(built,staged);run(llvm/'bin/llvm-strip','--strip-unneeded',staged)
   archive.write(staged,f'lib/arm64-v8a/{built.name}')
 aligned=staging/'aligned.apk';run(tools/'zipalign','-f','-p','4',unsigned,aligned)
 key=ROOT/'build/android-tools/turbulence-debug.keystore'
 key.parent.mkdir(parents=True,exist_ok=True)
 if not key.exists():run(java/'bin/keytool','-genkeypair','-keystore',key,'-storepass','android','-keypass','android',
                         '-alias','androiddebugkey','-keyalg','RSA','-keysize','2048','-validity','10000','-dname','CN=Turbulence Local Test',env=env)
 output=ROOT/'build/release/Turbulence-Android-arm64-test.apk';output.parent.mkdir(parents=True,exist_ok=True)
 run(tools/'apksigner','sign','--ks',key,'--ks-key-alias','androiddebugkey','--ks-pass','pass:android',
     '--key-pass','pass:android','--out',output,aligned,env=env)
 run(tools/'apksigner','verify','--verbose',output,env=env)
 print('APK:',output)
if __name__=='__main__':main()
