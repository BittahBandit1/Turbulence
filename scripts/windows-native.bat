@echo off
rem Native Windows build of Turbulence (runtime + game module) with Visual Studio
rem 2022's clang-cl, lld-link, CMake and Ninja. Output: build\win\Turbulence (a
rem play folder; put the extracted game in its "game" folder, or pass --game).
rem
rem Needs: Visual Studio 2022 (or Build Tools) with "Desktop development with
rem C++" and "C++ Clang tools for Windows"; Python 3 (module tables); a full
rem submodule checkout (if Externals lost tracked build\ folders in a copy,
rem restore them with: git submodule foreach --recursive "git ls-files -d -z | xargs -0 git checkout --").
rem
rem Flags match ports/windows/clang-cl-xwin.cmake (the macOS cross-build):
rem x86-64-v2 for the runtime AND the module. Without -march the REL chunks are
rem built for baseline x86-64 (no SSE4.1) and races lose ~10% speed.
rem
rem   scripts\windows-native.bat          incremental build
rem   scripts\windows-native.bat clean    delete build\win\runtime and \module, then
rem                                       rebuild (needed if the system clock jumped
rem                                       back: objects then look newer than sources)
setlocal
cd /d "%~dp0.."
set "ROOT=%CD%"
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Llvm.Clang -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR (echo Visual Studio 2022 with the C++ Clang tools was not found. & exit /b 1)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set "LLVM=%VSDIR%\VC\Tools\Llvm\x64\bin"
set "PATH=%LLVM%;%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
set CMAKE="%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "FL=-march=x86-64-v2 -maes -msha -mpclmul"
rem "clean" resets future-dated files and deletes both build folders: after
rem the clock jumps back, objects and CMake's files look newer than sources.
if /i "%~1"=="clean" (
  rem Files written while the clock was ahead stay "in the future"; ninja then
  rem regenerates build.ninja forever. Reset those timestamps to now.
  powershell -NoProfile -Command "$now = Get-Date; Get-ChildItem -LiteralPath '%ROOT%' -Recurse -File -Force -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -gt $now } | ForEach-Object { $_.LastWriteTime = $now }"
  if exist build\win\runtime rmdir /s /q build\win\runtime
  if exist build\win\module rmdir /s /q build\win\module
)
set "CLEAN="

rem Keep CMake's MSVC defaults (/DWIN32 /D_WINDOWS /GR /EHsc): replacing
rem CMAKE_CXX_FLAGS outright drops /EHsc and moderngekko_run.cpp fails to build.
%CMAKE% -S lib/ModernGekko -B build/win/runtime -G Ninja -DCMAKE_BUILD_TYPE=Release ^
  "-DCMAKE_C_COMPILER=%LLVM%/clang-cl.exe" "-DCMAKE_CXX_COMPILER=%LLVM%/clang-cl.exe" "-DCMAKE_LINKER=%LLVM%/lld-link.exe" ^
  "-DCMAKE_C_FLAGS=/DWIN32 /D_WINDOWS %FL%" "-DCMAKE_CXX_FLAGS=/DWIN32 /D_WINDOWS /GR /EHsc %FL%" ^
  "-DCMAKE_EXE_LINKER_FLAGS_RELEASE=/INCREMENTAL:NO /MAP" ^
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded -DBUILD_TESTING=OFF -DUSE_SYSTEM_LIBS=OFF ^
  "-DTURBULENCE_OPTIONS_SOURCE_DIR=%ROOT%/src" || exit /b 2
%CMAKE% --build build/win/runtime --target moderngekko-run %CLEAN% || exit /b 3

set "REL="
for /d %%d in (build\modules\GXEE8P\*-5f8c8e46802d6f00) do set "REL=%%~fd\dolrecomp-output\rel\generated"
if not defined REL (echo The C REL output is missing under build\modules\GXEE8P. & exit /b 4)
rem Module C flags carry only -march: CMake's IPO check reuses them with a
rem GNU-style clang driver, and MSVC-style flags there silently disable LTO.
%CMAKE% -S lib/ModernGekko/vendor/dolphin/module-template -B build/win/module -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release "-DCMAKE_C_COMPILER=%LLVM%/clang-cl.exe" "-DCMAKE_LINKER=%LLVM%/lld-link.exe" ^
  "-DCMAKE_C_FLAGS=%FL%" "-DCMAKE_SHARED_LINKER_FLAGS=/MAP" ^
  -DGAME_ID=GXEE8P "-DGENERATED_DIR=%ROOT%/build/llvm-windows-x86_64/dol/generated" ^
  "-DREL_GENERATED_DIR=%REL%" ^
  "-DGXRUNTIME_DIR=%ROOT%/lib/ModernGekko/vendor/dolphin/GXRuntime" ^
  "-DCHASSIS_ABI_DIR=%ROOT%/lib/ModernGekko/vendor/dolphin/Source/Core/Core/PowerPC/StaticRecomp" || exit /b 5
rem -j 8: 16 parallel compiles of the big REL chunks ran out of memory on a 16 GB laptop.
%CMAKE% --build build/win/module -j 8 %CLEAN% || exit /b 6

set "P=build\win\Turbulence"
if not exist %P%\module mkdir %P%\module
copy /y build\win\runtime\moderngekko-run.exe %P%\ >nul || exit /b 7
robocopy build\win\runtime\Sys %P%\Sys /MIR /NFL /NDL /NJH /NJS >nul
copy /y build\win\module\gGXEE8P_recomp.dll %P%\module\ >nul || exit /b 7
copy /y ports\windows\Turbulence.bat %P%\ >nul
copy /y config\GCPadNew.ini %P%\ >nul
copy /y ports\extract-sonic-riders.py %P%\ >nul
echo Built %ROOT%\%P%
