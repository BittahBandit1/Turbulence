# Turbulence on Windows: handoff for a Claude session on the laptop

Goal: Sonic Riders (GXEE8P) recomp running at a locked 60 FPS on this Windows
laptop. It boots, but the character select screen runs at about 7 FPS.
Work unattended, with the game muted (`TURBULENCE_MUTE=1`).

Project rules:
- Never put the owner's real name in files, commits or git identities. Use
  the handle BittahBandit1.
- Commit only when the owner asks. Commit identity:
  `git -c user.name=Claude -c user.email=noreply@anthropic.com commit ...`
- Edits inside the submodules (lib/ModernGekko, its vendor/dolphin and
  DolRecomp, lib/DolRecomp, libspng, watcher) are saved as patches/*.patch by
  `bash scripts/save-patches.sh` (Git Bash). Run it after changing them.

## What is in this package

- The full source, including submodules, with patches already applied.
- `build/llvm-windows-x86_64/dol/generated`: main.dol compiled by DolRecomp's
  LLVM backend to COFF objects, in x86-64-v2 and x86-64-v3 (AVX2) copies.
  The module picks one at startup; `TURBULENCE_X86_64_V2=1` forces v2.
- `build/modules/GXEE8P/*-5f8c8e46802d6f00/dolrecomp-output/rel/generated`:
  the REL (_Main.rel) recompiled to C.
- The prebuilt Windows test folder is `build/release/Turbulence-Windows-x64.zip`
  on the Mac. Copy the extracted game into `game\` (use
  `ports/extract-sonic-riders.py` on the ISO).
- These are derived from the owner's disc. Don't share them.

These are cross-built on macOS with clang-cl + lld-link + xwin
(`scripts/windows.sh`, `ports/windows/clang-cl-xwin.cmake`). Building
natively here needs:

    winget install Kitware.CMake Ninja-build.Ninja LLVM.LLVM Python.Python.3.12
    winget install Microsoft.VisualStudio.2022.BuildTools  (C++ workload + Windows SDK)

Then, from an "x64 Native Tools" prompt (or after vcvars64.bat):

    cmake -S lib/ModernGekko -B build/win/runtime -G Ninja -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_LINKER=lld-link ^
      -DCMAKE_C_FLAGS="-march=x86-64-v2 -maes -msha -mpclmul" -DCMAKE_CXX_FLAGS="-march=x86-64-v2 -maes -msha -mpclmul" ^
      -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded -DBUILD_TESTING=OFF -DUSE_SYSTEM_LIBS=OFF ^
      -DTURBULENCE_OPTIONS_SOURCE_DIR=%CD%/src
    cmake --build build/win/runtime --target moderngekko-run
    cmake -S lib/ModernGekko/vendor/dolphin/module-template -B build/win/module -G Ninja ^
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl ^
      -DGAME_ID=GXEE8P -DGENERATED_DIR=%CD%/build/llvm-windows-x86_64/dol/generated ^
      -DREL_GENERATED_DIR=<the rel/generated path above> ^
      -DGXRUNTIME_DIR=%CD%/lib/ModernGekko/vendor/dolphin/GXRuntime ^
      -DCHASSIS_ABI_DIR=%CD%/lib/ModernGekko/vendor/dolphin/Source/Core/Core/PowerPC/StaticRecomp
    cmake --build build/win/module

The module build takes a while: the REL is about 120 big C files. The
runtime build is about 1,700 steps. Sys\ comes from build/win/runtime/Sys.

## Running and measuring

    set TURBULENCE_MUTE=1
    set TURBULENCE_PERF_LOG=1        (prints "[turbulence] Perf fps= vps= speed=" every 5 s and the video backend)
    set TURBULENCE_FPS_COUNTER=2     (on-screen counter with frame intervals)
    moderngekko-run.exe --game game --module module\gGXEE8P_recomp.dll --user-dir user [--graphics D3D|D3D12|Vulkan|OGL|Null] [--headless]

On exit the runtime prints
`[staticrecomp] shutdown: native= fallback= hook_fb= smc_failed= ...`.
fallback should be 0 and hook_fb small (macOS: 3.75M per run).
The attract demo needs no input. Character select needs input: drive it with
SendInput/keyboard, or look in lib/ModernGekko/tools/automation_protocol.cpp,
which accepts scripted input and reports status (vps, frame counts).
On crash the exe prints `[crash]` with the exception, the C++ type/what()
and a stack of module+offset entries. Link with `/MAP` (scripts/windows.sh
does) to turn the offsets into function names.

## Reference numbers

macOS (M3 Pro, Metal, LLVM main.dol + C REL): locked 60, about 50% of one
core. Android (Retroid Pocket Nova, Vulkan): locked 60. Linux aarch64 and
x86_64 (under Rosetta) boot headless with fallback=0.

## Windows bugs fixed so far (Nov 11, 2026)

1. clang-cl build fixes in Dolphin: no /WX or PCH, optional PowerShell,
   D3D narrowing warnings, CPUCull target attributes, <functional>,
   _putenv_s, no libm (GXRuntime, libspng), ASSERT_MSG comma elision under
   the MS preprocessor.
2. module_export.c: the x86-64-v3 path never fell through to the REL
   dispatcher. That broke every AVX2 machine, Linux included.
3. src/turbulence_prompts.cpp: dynamic_cast on a controller device. Dolphin
   builds with /GR- on Windows, so it threw "no RTTI data" on the first
   frame. Now checks GetSource()=="SDL" and uses static_cast.
4. module-template: clang-cl got /fp:strict (= -ffp-model=strict). It now
   gets /fp:precise /clang:-ffp-contract=off /clang:-fno-fast-math, the same
   as other platforms. This did NOT change the 7 FPS.

## Open problem: about 7 FPS at character select

Not yet measured: whether the CPU thread or the GPU is the bottleneck, or the
fallback/hook_fb counts on Windows. Start with:
- The speed log on each backend (D3D, D3D12, Vulkan, OGL). If one backend is
  fine, it's a video issue. If all are slow, it's the CPU side.
- The shutdown counters versus macOS. A large fallback or hook_fb points at the
  module/REL path. smc_failed or reverify_events growing points at SMC
  invalidation.
- A CPU profile (Windows Performance Recorder, or `perf`-like sampling via
  the Visual Studio profiler) of moderngekko-run during character select.
  Check whether time goes to the module, Dolphin's video code, or
  ntdll/kernel (page faults, timers).
- Windows-only code paths in the runtime: the fastmem / exception handler
  (Dolphin's MemArena / VEH on Windows), the Windows timer resolution (CPU
  thread pacing uses sleeps), thread priority/affinity code in
  moderngekko_run.cpp (_WIN32 blocks), and Dolphin's D3D11 EFB access or
  bounding-box paths (character select may read back the EFB).

## Resolved (Nov 12, 2026): locked 60 at character select

Measured on the Ryzen 7 8845HS / Radeon 780M laptop, built natively with
VS 2022's clang-cl 19 (runtime + module), muted, automation-driven:

| Scene | Before | After |
| --- | --- | --- |
| Character select, Vulkan | ~8 fps | 59.8 |
| Character select, D3D11 | — | 59.9 |
| Menus (title to Player Entry), D3D11 | — | 59.9 |
| Intro movie, D3D11 | ~35 | 54-57 |

The Null video backend was as slow as the others, and a sampling profile
put 85% of the CPU-GPU thread in the module, then (on Vulkan) 82% in a
12-byte memcpy. Two bugs:

1. module_export.c: `host_has_x86_64_v3()` ran CPUID x3 + XGETBV on every
   dispatch. The MSVC/clang-cl branch had no caching (the GCC/Clang branch
   did); CPUID serializes and traps to Windows' hypervisor (VBS). Now
   detected once, and the v2/v3 choice is cached in `selected_dispatch`.
2. VertexManagerBase::MeasureExtent (2D/widescreen classification) measured
   from `m_base_buffer_pointer`. On Vulkan that is the start of the whole
   mapped stream buffer, so each 2D draw re-read up to 4096 stale vertices
   of earlier draws from uncached GPU memory (and mixed them into the
   extent). It now measures from `m_draw_start_pointer`, set after each
   buffer reset. D3D/Metal were unaffected (their base is the draw start).

Also fixed: crash on exit (PlatformWin32 WndProc ran Config::SetCurrent from
WM_KILLFOCUS after Config shutdown); implot passed GCC `-include` to clang-cl
(VS's clang-cl 19 rejects it; now /FI whenever MSVC).

New: Options menu "Fullscreen" row (desktop only), saved as [Video] Fullscreen
in Turbulence.ini and applied at the next launch. Platform::RequestFullscreen
is applied by each platform's own loop (Win32, macOS, X11, Wayland). Win32
now really starts fullscreen when MAIN_FULLSCREEN is set (was a TODO) and
reports it through Host_RendererIsFullscreen; macOS no longer shadows
Platform::m_window_fullscreen.

Still open: D3D12 starts but never renders; boot takes 35-60 s; the intro
movie is not fully locked. Native-build notes: the copied tree lacked tracked
`build/` folders in several Externals (restore with `git ls-files -d`); after
VS Build Tools updated the Windows SDK, the runtime needed --clean-first
(C++/WinRT /failifmismatch). scripts/export-patches.py misbehaves on Windows
(file-mode noise, drops untracked files), so this commit's patch entries were
spliced in from per-file `git diff HEAD`.
