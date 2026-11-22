# Turbulence status

Sonic Riders (USA, GXEE8P) static recompilation on ModernGekko + DolRecomp.

## Native Recomp Settings (2026-11-18)

Options now contains a fifth native **Recomp Settings** button. The original
Options artwork, font, navigation and other four entries remain in place.
The new page reuses the original Language Setting frames and disc bitmap
lettering, with the green preview panel cleared of the map and globe for
future custom icons. Left/Right changes settings and saves automatically;
Back returns through the native menu lifecycle. Shoulder-button opening is
removed. Earlier R1 quick-menu notes below describe the superseded UI.

The native integration targets the USA GXEE8P front-end. Menu hooks are active
only while the host settings UI is available and the front-end task is active;
racing retains its normal dispatch path. Sanitized hook tests cover the extra
item, stack restoration, focus mapping, native Back lifecycle and preserving
the real language preference. macOS live checks cover entry, saving and Back.

## What is recompiled
- `sys/main.dol`: 57 chunks.
- `files/_Main.rel` (459,831 instructions, about two thirds of the game's
  code): 113 chunks, compiled at the address the game links it to (base
  `0x803FBD00`, `.bss` `0x805FB080`, read from RAM and identical across
  boots). Set in the Makefile; `TURBULENCE_REL_DISABLE=1` turns it off.
- The runtime hash-checks every chunk against RAM before running it, so a
  layout change falls back to the JIT instead of running wrong code.

## Earlier measurements (M3 Pro, Metal, 150-420 s unattended runs)

These inherited results are historical. The foreground captures and FMV
checks below show remaining slow frames; they supersede any blanket 60 fps claim.
| | Gemini's state | Now |
|---|---|---|
| REL code | Dolphin JIT | native |
| Interpreter steps (SDK cache routines) | ~1 billion per 3 min | 0 |
| Speed | 0.95x (with upstream JIT fix restored) | 1.00x, 60 fps |
| Chunks verified / failed | 57 / 0 | 170 / 0 |

Played from the title screen through save creation, Player Entry and into a
Metal City race on the recompiled REL at 60 fps.

## Fixed along the way
- DolRecomp: REL `.text` at an unaligned offset was rejected; 16-bit REL
  relocations were written 2 bytes late (68,761 corrupted instructions).
- ModernGekko: full-RAM REL scans on every lookup / icache line (boot
  stalled); OSLinked RELs never matched (absolute sectionInfoOffset).
- Gemini had reverted upstream's JitArm64 fix, which left the static recomp
  mostly inert on Apple Silicon.

## Active FMV follow-up

User requested FMV playback work on November 5. Source-frame comparisons
and native quantization optimizations are documented in the latest section
below. The original roughly 3/4-speed report is not considered resolved solely
from a short full-speed sample.

## Recording follow-up (2026-11-05)

The title's rotating circular artwork is now identified by its original C4
atlas hashes (`306cc14cb2da5cdf`, `0a503a7ee2456e29`) and marked to preserve
its aspect, independently of the full-width cloud/backdrop passes. This also
wins over the stretched-HUD option.

The drift recipe now replaces the entire octagonal GC stick cell with the
modern rubber thumbstick already supplied in the original atlas, for both
PlayStation and Xbox. The race HUD's PS Circle cell is covered by the shared
recipe test. Recipe definitions are shared between runtime and tests in
`src/turbulence_prompt_recipes.hpp`.

Found and fixed stale prompts on controller changes: invalidation previously
ran inside GetTexture, after LoadImpl's bound/TMEM fast path. Prompt entries
are now cleared from BOTH bound stages and address/hash caches before that
fast path runs. Automatic detection now prefers SDL's identified PS3/PS4/PS5
or Xbox type and supports native XInput, with device-name fallback.

Pacing change: removed timestamp-only frame skipping entirely; the prior
bounded recovery policy still discarded visible frames without proving a
render backlog. Metal now explicitly limits CAMetalLayer to two drawables
(the API defaults to three). Frame capture additionally records drawable
acquisition wait, submission work, availability and display-callback counts.
The report separates CPU submission intervals from actual display timing.

Build and sanitized controller/viewport tests pass. A muted isolated run
recognized PlayStation automatically. Its 600-frame capture had zero
unsubmitted frames, but the layer reported every callback as not displayed;
it cannot validate visible pacing. `scripts/capture-pacing.sh` launches normal
play and requests three 600-frame captures at about 20/40/60 seconds. A
foreground user capture has been requested before claiming a pacing gain.
The earlier recovery header/test were removed, and previous pacing notes
below describe historical behavior superseded by this follow-up.

## Results UI, controller prompts, culling and cursor follow-up

- Edge-touching foreground UI now shares the same centered w/h transform as
  its portraits and text. Width alone no longer stretches/extends a results
  row or its portrait socket. Only a single full-screen background quad,
  frame-copy effect, or offscreen pass fills the expanded viewport. The
  classification and transforms are independent of a particular target AR.
- The previously omitted race drift atlas is CI4
  `tex1_256x128_b59d2162deab7746`. It now receives Xbox LT/RT and PlayStation
  L2/R2 captions in the original rectangular keycaps, matching the current
  analog-trigger defaults. The separate race HUD boost atlas
  `tex1_512x512_6e2a2924575e9af0` also swaps GC B for Xbox B / PS circle.
  Adjacent atlas cells and palette variants remain supported.
- GXEE8P's verified perspective wrapper at 0x800645DC now receives the same
  Hor+/Vert+ scaling used by presentation, putting the expanded projection
  into guest RAM for frustum calculations. Backend projection recognizes
  these matrices and avoids applying the expansion twice. This targets
  frustum-edge disappearance, not distance-based streaming/LOD policies.
- Shared cursor inactivity policy: show on movement/click/entry, hide after
  three seconds in the focused render area, restore on leaving/focus loss or
  pause. Integrated in Cocoa, Win32, X11 and Wayland. Mobile was explicitly
  deferred by the user.

Validation: macOS runtime builds; sanitized recovery, viewport/cursor and
caption tests pass. The viewport tests cover 4:3, 16:9, 16:10, 21:9, 32:9 and
portrait Hor+/Vert+ math, plus narrow result rows and batched row geometry.
The caption tests exercise both styles and check untouched neighboring cells.
Native caption output preview: `build/drift-prompts-preview.png`.
Win32 passed MinGW compilation checks; X11 passed compilation checks. Wayland
was reviewed but its SDK/runtime is unavailable on this host.

A separate muted 32:9 Metal session confirmed widened guest camera construction
and live replacement of both race boost and drift textures (see
`build/ui-bundle.log`). The desktop inspection call hung and was interrupted
when ChatGPT restarted, so exhaustive visual checks of results and every track
at all ratios are not claimed. Player save data/settings were not used for
these tests; isolated profiles live under build/.

## Intro aspect ratio (2026-11-04)

The inherited full-width 2D classification stretched the intro movie along
with menu backgrounds. `OPENING.SFD` and `TITLE_G.SFD` are 640x480 video.
A live rendering trace identified decoded playback as a single 640x480
RGBA8 RAM texture (not an EFB/frame copy). Full-width orthographic draws
using that texture now use `Movie4x3`, centering the video even if the user
chooses stretched HUDs. EFB effects, other menu textures and perspective
rendering retain their existing handling. `TURBULENCE_WS_TRACE=1` now also
logs distinct texture format/dimension combinations for follow-up checks.
Rebuilt successfully and captured the intro with centered video and black
side bars in `build/codex-intro-fixed-auto/intro-fixed.png`.

## Frame pacing follow-up (2026-11-04)

Working sources recovered from `~/.gemini/users/user1/Turbulence`
(parent revision `0ecf0d4`), including the existing uncommitted runtime and
pacing-report changes. The older working copy and play folder were left intact.

The old recovery loop dropped up to twelve frames even if the lateness stayed
constant. It now stops after the first ineffective skip (less than 1 ms of
progress), or 50 ms of recovery wall time, while retaining the twelve-frame
hard cap. State belongs to the presenter rather than process-wide statics.
A fresh schedule rearms recovery; missing/backward deadlines reset it.
This addresses recovery-induced blank intervals, not shader compilation cost.

The pacing report now counts presentations without a submission timestamp
separately from Metal-reported dropped drawables, including the longest run.
It also accepts captures with only one interval or lateness sample.

Build in this workspace (the copied old build caches refer to the old path):

```sh
cmake -S lib/ModernGekko -B build/codex-runtime -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
  -DTURBULENCE_OPTIONS_SOURCE_DIR="$PWD/src"
CCACHE_DIR="$PWD/build/codex-ccache" cmake --build build/codex-runtime \
  --target moderngekko-run -j 8
c++ -std=c++20 -Wall -Wextra -Werror \
  -Ilib/ModernGekko/vendor/dolphin/Source/Core \
  tests/frame-recovery.cpp -o build/test-frame-recovery
build/test-frame-recovery
```

`launch_sonic_riders.sh` uses this runtime and the existing active game module
when launched from the source workspace. The Dolphin patch includes the changes.

Validation: full `moderngekko-run` build passed, recovery scenarios passed
with AddressSanitizer and UndefinedBehaviorSanitizer, launcher syntax checked,
and the short-capture report was exercised. The build also needed `VERBATIM`
on its Sys copy step because the new workspace path contains parentheses;
that fix is included in `patches/moderngekko.patch`.

A Metal smoke run loaded the native module, ran, and exited cleanly with no
failed SMC verifications. `build/codex-pacing-3/pacing-0.csv` contains 600
presentation events (3 without a submission timestamp, longest run 2), but
no Metal on-glass callbacks, even with caffeinate keeping the display awake.
This validates runtime integration, **not** an improvement in measured display
cadence. A visible race/loading A/B capture remains needed; the historical
measurements below belong to Claude's earlier build.

## Frame pacing (on-glass times, 120 Hz MacBook Pro display, windowed)
Measured with the pacing recorder (`TURBULENCE_PACING_DIR`, touch
`pacing-request`; `scripts/pacing-report.py`), 600 frames each:
| | Gemini's pacing | ReFare's pacing |
|---|---|---|
| Race: frames on the 16.67 ms cadence | 593/598 (5 at 25 ms) | 598/598, sd 0.001 ms |
| Race: present vs. schedule (p50) | ~3,560 ms behind | 15 ms |
| Loading into a race | not measured | 576/591, 14 at 25 ms, no drops or 2-frame hitches |
| Loading into a race, shader cache saved | | 585/593, 8 at 25 ms, one 50 ms frame ~5 s in; p50 present vs. schedule 2 ms |

Changes: minimum present duration matched to the display (16.17 ms on
60/120 Hz) instead of a fixed 14 ms; no 3-drawable queue; no
present-before-wait reorder; stall catch-up; time-drift correction no longer
forced on; user-interactive QoS for CPU and render threads; no App Nap.
Remaining: one-refresh-late frames while a race loads (file loading and
shader compilation).

## Metal HUD
Now as in ReFare: MTL_HUD_ENABLED set before Metal starts, the menu only flips
the layer's HUD mode. Gemini's version set the variable after Metal had read
it and replaced the layer's HUD properties, so the HUD never appeared.

## Dynamic button prompts
Sonic Riders' prompt textures already contain the GameCube, Xbox and PS2
icons side by side; the GameCube build only draws the GameCube cells.
`TurbulencePromptTextures` (VideoCommon) patches the chosen style's cells over
them when a prompt texture loads; `src/turbulence_prompts.cpp` follows the
last-used controller (DualShock / DualSense: PlayStation, Xbox pads: Xbox,
anything else: GameCube). Options menu: Button prompts Automatic / GameCube /
Xbox / PlayStation. Verified on Player Entry (Select / Back bar, Information,
"Press" slots) with both styles. Covered textures: the 256x128 icon sheet and
the 512x128 Select / Back bar -- the only prompt art on the title, save,
main menu, Player Entry and race screens. Story mode, shop and missions not
yet checked.

## Shader cache
Dolphin's `Cache/` folder was never created, so the pipeline UID cache never
saved and every launch compiled shaders during play. Now created at startup.

## Options menu
Ported from ReFare (`src/turbulence_options.cpp`), Sonic Riders palette.
R1 / Tab by default, rebindable. Not yet checked visually: automation
screenshots capture the game image without overlays.

## Open
- Widescreen: 3D is drawn at 16:9 through Dolphin's widescreen hack. Every 2D
  draw is classified by its measured on-screen extent (VertexManagerBase):
  full-width backgrounds and frame-copy effects stretch, bars touching one edge
  extend to that edge, everything else (race HUD, menu panels, loading art)
  stays centered at 4:3. Checked on the title, Player Entry, loading and race
  screens. `TURBULENCE_WS_CLASSIFY=0` centers all 2D (diagnostic);
  `TURBULENCE_WS_TRACE=1` logs extents. The game-side perspective hook described above now updates the
  projection used for frustum checks at nonstandard aspect ratios.
- Gemini's `presentDrawable:afterMinimumDuration:0.014` and texture-cache
  changes are still in place, unreviewed.

## Testing
`caffeinate -dims scripts/turbulence_agent.sh` runs jobs from
`build/claude-jobs/`. `scripts/package-play-folder.sh <folder>` refreshes a
play folder from the current build.

### Lap-3 HUD capture and scheduler wait — November 5

The live capture `build/pacing-live/20261105-002430-45594` exposed two additional HUD cells: the turbulence shake cue is `tex1_128x128_4ee3d7b790310d80` (48-pixel stick cell), and the boost HUD samples the B alias at x424, not only x376. Both aliases now use Circle for PlayStation and the native red B for Xbox; the separate shake atlas uses its modern stick for both. Native GameCube remains unchanged. Tests exercise the real recipe factory, both boost aliases, the complete shake cell and surrounding arrows.

The live CPU sample placed 266/1238 CPU-GPU samples in the SDK ready-queue idle poll at 0x80010574. Enabled scheduled-event advancement for this game only, guarded by exact original DOL instructions, empty ready queue, enabled interrupts and no pending exceptions. Unit checks cover each rejection condition. The muted smoke run booted/exited normally, advanced 487812 idle waits, and reported zero SMC failures. Its opening capture measured 19.123 ms mean CPU submissions and 73.2% display intervals on cadence; this is a different scene from the user's race capture and does not establish a race improvement. Race pacing is still unverified and should not be called fixed. FMV speed remains queued for later.

Validation: macOS runtime rebuilt; sanitized controller-atlas and idle-guard checks passed; maintained patch reverse dry-runs passed.

### Opening logo proportions — November 5

Confirmed the opening SEGA and Sonic Team logos share the native 512x512 CMPR atlas `bcbaa36c9296931c` in the existing texture dump. Added this exact atlas to aspect-locked artwork, using the same centered aspect-preserving transform as the title circles, including with stretched HUD selected. This avoids treating the logo quad as a fill background at ultrawide or portrait ratios.

### Boost cell correction and FMV investigation — November 5

User confirmed the remaining mixed Xbox/GameCube artwork was the top-left Boost prompt. Inspection of the original atlas showed the actual race HUD cells are 25x25 at x376 (GC B), x401 (Circle), and x426 (Xbox B). Replaced the earlier 24x24 / x400 / x424 assumption. Both drawn B aliases now receive the entire Xbox B or Circle cell, copied from immutable original pixels. Sanitized checks cover both styles, both destinations, surrounding pixels, and native GC. Native-image preview is `build/verified-boost-alignment.png`.

FMV tests: compare screenshots with decoded TITLE_G source frames rather than relying solely on the FPS counter. With the existing runtime, source positions were 3.6 / 31.7 / 59.2 / 86.7 seconds at test elapsed 10 / 40 / 70 / 100 seconds (the initial logos account for startup offset). Extra Metal hold disabled did not solve sustained slowdown and substantially worsened interval cadence; retain existing presentation behavior. Dual-core rendering also failed to solve it; do not enable it as a fix. These settings were tested only in the isolated review user profile.

The CPU profile during movie playback exposed repeated PSQ pixel quantization calls through ldexpf. Added exact IEEE float power-of-two factors for the signed six-bit GQR scale range, retaining f32 multiplication, clamp and truncation behavior. Dequantization uses the same exact factors. Sanitized comparison covers all signed/unsigned 16-bit lane values at all 64 scales, plus floating-point boundary cases, infinities and NaN. Playback validation of the relinked native module is pending.

First optimized module playback comparison: source frame positions were 3.9 / 33.0 / 61.3 / 89.8 seconds at elapsed 10 / 40 / 70 / 100 seconds, compared with baseline 3.6 / 31.7 / 59.2 / 86.7. Early display cadence improved from 84.6% to 90.5% within +/-1 ms of 16.68 ms. This is a modest improvement, not a full-speed fix. Added a second optimization dispatching the common unsigned-byte quantization type once per pair, while preserving normal byte write hooks/journals/reservations. The GXRuntime test suite passes. Final module linkage and playback validation remain pending.

Final validation and launch selection: restored Metal's three-image drawable pool, retaining the existing minimum-duration presentation and normal emulation speed limit. Compared with two images, it improves sustained movie throughput while preserving cadence. Final source-image matches at elapsed 10 / 40 / 70 / 100 seconds were 3.875 / 33.750 / 63.083 / 92.792 source seconds. Between elapsed 40 and 100, source playback advanced at 98.4% real time versus 91.7% for the baseline. The final 600-frame samples measured 95.5% and 97.2% on cadence; the later sample had no >=2-frame hitch and no dropped frames. The source comparison identifies the actual movie frames, independent of the emulator FPS display. Small pacing dips remain; do not describe it as perfectly locked.

The complete movie finished and returned to the title screen. The paired-byte module also ran into the attract race: zero interpreter fallback steps and zero SMC failures; the modern turbulence stick appeared in the demo. Final artifacts are `build/fmv-validation/final/`, including report.json and runtime.log. `active-module.txt` now selects the new, separately stored native module; the previous selection is retained in `active-module-before-fmv.txt`. Normal sound remains at volume 100; all validation runs used session-only mute. The unsigned-byte PSQ pair sanitizer test also checks every GQR scale, single/pair writes, NaN/infinities, saturation and disabled-LSQE rejection. GXRuntime's existing test suite passes.

## Mobile ports and pacing polish — November 8, 2026

Adapted Crazy Taxi ReFare's local Android/iOS hosts and mobile runtime patches
into Turbulence. The source reference was the neighboring Crazy Taxi
Recompilation (ReFare) workspace; that project was not edited. Mobile source,
GPL attribution and build instructions live in `ports/README.md` and
`ports/COPYING`.

Both personal gameplay apps statically link the matching GXEE8P main.dol and
_Main.rel generated code. Runtime compilation/dynamic game-module loading is
disabled. iOS disables executable JIT fallback and uses interpreter fallback,
Metal, native AVAudioEngine audio and retained UIKit surfaces. Android uses
Vulkan, AAudio, JNI and its activity lifecycle. Both have ISO/GCM/GameCube CISO
importers with DOL/REL revision checks, touch/controller input, automatic
PlayStation/Xbox prompts respecting manual preferences, pause/resume and the
existing options menu. Touch input is Sonic Riders specific: A jump, B boost,
independent L/R drift, analog movement (Android) or direction pad (iOS), Start.
R1 opens options. Mobile hides the unsupported desktop button-rebinding row.

Built and validated:

- `build/release/Turbulence-Android-arm64-test.apk`: debug signed, signatures
  verified by apksigner; ARM64 native libraries have verified 16 KiB ELF segment alignment.
- `build/release/Turbulence-Gameplay-0.1.0-iOS15-arm64-unsigned.ipa`: physical
  ARM64 iPhone/iPad target, iOS 15 minimum, executable/load-command checks pass.
  Requires personal signing before installation.
- Both contain system resources and native code, with no raw main.dol,
  _Main.rel or extracted game assets. Import a personal matching USA revision 0
  disc on the device. No physical-device gameplay, audio, thermal or pacing
  performance has been verified in this session.

Android SDK/NDK tools were reused read-only from ReFare's build/android-sdk;
all Turbulence outputs and signing keys are in this workspace's ignored build
folder. iOS uses installed Xcode. SwiftUI macro compilation requires its own
protected compiler subprocess, so the final iOS build ran outside the restricted
execution environment. Normal desktop playback remains audible.

Validation: Android importer tests pass with synthetic ISO/CISO data; Swift
importer tests pass including file coordination; Swift touch-layout/input
checks pass for landscape phones, portrait and tablets; generated-source
selection rejects missing/wrong game output. Both device targets compile and
package. `scripts/validate-mobile.py` checks architecture, iOS deployment,
Android ELF page alignment, resources and absence of raw game executables.
Vendor patches reverse-apply cleanly, including the separate watcher iOS shim.
Patch export no longer stages vendor files and now includes new files.

Pacing changes:

- Bound GXEE8P's obsolete host-deadline allowance to 20 ms (previous default
  100 ms) instead of dropping rendered frames after a stall.
- Metal can commit rendering before a host presentation wait longer than
  1 ms, keeping GPU work out of the timer's critical path. Normal submission is
  unchanged when there is no useful wait. `TURBULENCE_EARLY_SUBMIT=0` disables
  this path; `2` forces it solely for command-buffer validation.
- Convert Core Animation presentation timestamps to the recorder's Clock
  epoch. Serialize callback writers, bound overwritten-ring reads, and fix the
  duplicate presentation-counter increment. Reports distinguish valid negative
  submission offsets from unknown timestamps and count early submissions.

A/B validation uses the same saved attract-race state in
`build/pacing-polish-20261108`, with mobile compilers paused for measurement.
Both baseline and candidate ran around 59.9 FPS with 16.683 ms mean CPU
submission intervals, no unsubmitted frames, no native code verification
failures and zero interpreter fallback steps. The normal race samples had no
host waits over 1 ms, so the early-commit hook did not activate there. Forced
validation exercised 599 early submissions in a 600-frame sample and exited
normally with zero SMC failures.

All automated Metal callbacks were reported not displayed in this session.
These results establish submission stability and regression coverage, not
improved visible pacing. A user foreground capture was requested; displayed
frame cadence and the effect after actual display stalls remain pending.
Do not describe pacing as perfect or assert an on-glass gain from these tests.

Presentation timestamp conversion now takes a fixed epoch calibration at the
first valid display callback in each capture. Native timestamps remain intact,
so callback scheduling delays cannot introduce artificial interval jitter.

## RP Nova Android handoff — 2026-11-08

Installed the debug-signed Android APK on a Retroid Pocket Nova, Android 13/API 33, QCS8550, landscape 1280x960. The fresh install
opened successfully. Reconstructed a private test ISO from the existing
extracted USA revision 0 assets and exercised the on-device importer: 1669
files imported and verified. Imported assets remain in the app sandbox.
The temporary device ISO was removed and the app force-stopped for handoff.

Native ARM64 runtime and Vulkan booted; AAudio opened at 48 kHz stereo and
reported non-silent output. Screenshots show the intro playing. Intro frame
counters were around 43–50 FPS after startup, below full speed. Actual
SurfaceFlinger timestamps were available (display period 8,333,333 ns), but
no sustained gameplay/pacing measurement was completed. Do not claim full
speed or complete feature parity from this test.

Investigate next: gameplay.log reports `idle wait PC=0x00000000` although
files/Sys/GameSettings/GXEE8P.ini includes StaticRecompIdlePC=0x80010574.
Determine why that game configuration is not effective at core initialization.
Start events reached the native pad (1000 pressed, 0000 released), but the
intro remained visible; racing, physical controls, prompts, options, audio
quality, suspend/resume and thermals still need device verification. Local
screenshots and private reconstructed disc are under ignored build/rp-nova.
Android work was stopped at the user's request for Claude to continue.

## Android performance and FPS counter — November 8-9, 2026 (Claude)

Measured on a Retroid Pocket Nova (Snapdragon 8 Gen 2, Android 13, 120 Hz
panel) over the same attract loop (title, intro movie, demo race), with
`adb logcat -s Turbulence` "Perf" lines (5 s FPS/VPS samples) and simpleperf.

Causes found, in order of impact:

1. Sys resolved to `/system/bin/Sys/`: Android builds also set LINUX_LOCAL_DEV,
   whose branch in `CreateSysDirectoryPath` won over the Android one, so
   GameSettings/GXEE8P.ini never loaded (no idle-loop skip, MaxFallback 100).
   `File::CreateSysDirectoryPath` now prefers the directory set by
   `SetSysDirectory`. Log line: `[turbulence] Sys directory: ...`.
2. The CPU thread was scheduled on Cortex-A510 efficiency cores (47 -> 60 fps
   on the title when moved). `Common::PreferPerformanceCores` (Android only)
   keeps the CPU and Video threads on cores above the lowest `cpu_capacity`.
3. Paired-single helpers were ~45% of the CPU thread. Android modules now build
   with thin LTO (as desktop does), and cpu.h has exact inline fast paths for
   ps_add/ps_sub/ps_mul (finite results, NI flush included) and quantized
   psq_l/psq_st (types 4-7). Equivalence: 3M randomized/edge cases against the
   out-of-line ops, 0 mismatches (NaN, inf, denormal, NI, LSQE, all GQRs).
4. 120 Hz FIFO alternated 1/3 refreshes per frame. The activity requests a
   60 Hz display mode and `Surface.setFrameRate(60)` while playing.

Also: Android uses the ARM64 vertex loader (iOS keeps the software one); the
fallback JIT stays off on Android (zero guest blocks, only invalidation cost).
The thread labelled "GC Adapter Scan" in top/simpleperf is the CPU thread
(cosmetic).

FPS counter: Options -> "FPS counter": Off / On / On + frame times
([Video] FPSCounter in Turbulence.ini; `TURBULENCE_FPS_COUNTER=0|1|2` forces
it). Shows FPS, emulation speed, VPS, mean/min/max new-frame interval over the
last second, intervals over 25 ms in the last 4 s, and a 4 s interval graph
with a 16.7 ms line. All platforms.

Attract-loop VPS through the movie and demo race (title is 60 throughout):
before ~44-48; after Sys + cores ~46-49; + LTO ~50; + PS fast paths ~57
(dips 50-54). See the end of this section for the latest numbers.

Latest (Nov 10): heavy-stretch average 56.8 VPS (repeat runs agree within
0.1); title/menus a locked 60. Remaining dips: ~50 at the busiest point of the
intro movie, 52-58 in the demo race. Tried and dropped (no gain): -O3 for the
module, forced inlining of the fast paths (-2 fps: code growth), pinning the
CPU thread to the prime core (the scheduler already keeps it there at
3.19 GHz). Top remaining costs (simpleperf, CPU thread): the game's own
generated code (~45%), quantized stores/loads (~18%), dispatcher and cache-
flush bookkeeping (~10%).

## Android: LLVM backend for main.dol, custom GPU drivers — November 11, 2026 (Claude)

**LLVM backend.** `python3 scripts/android.py --llvm` builds main.dol with
DolRecomp's LLVM backend (native ARM64 objects for
`aarch64-unknown-linux-android26`, generated into `build/llvm-android/` on
first use) while _Main.rel stays C-backend output. Both run natively; only the
code generator differs. Needs DolRecomp built with LLVM 19/20 in
`lib/DolRecomp/build-llvm` (the script prints the cmake line).

What made mixing possible:
- module-template no longer rejects an LLVM main.dol with a C REL; the two
  dispatchers only share the CPUState module ABI (module_export.c tries the
  DOL's, then the REL's).
- GXRuntime gains `ppc_native_region_available`, which LLVM regions call on
  entry while a host_call hook is installed; StaticRecompCore answers the
  `PPC_HOST_CALL_NATIVE_REGION_QUERY` from `ModManager::HandlesRange`.
- The runtime installs the mod host_call hook only when mods are loaded. With
  no mods (the normal case) the recompiled code skips a ModManager round trip
  on every dispatch (~2.5% of the CPU thread) and LLVM regions skip the query.
- CPUState layouts of DolRecomp's and GXRuntime's headers were checked
  identical (only union field aliases differ).
- `scripts/mobile_common.py` accepts LLVM objects for the main executable.

Attract loop, heavy stretch (movie + demo races), Retroid Pocket Nova:

| Build | Average | Worst 5 s sample |
|---|---|---|
| C backend (Nov 10) | 56.8 | 49.6 |
| C backend + no-mods host_call | 58.5 | 52.5 |
| LLVM main.dol + C REL | 59.9 | 59.0 |

The LLVM build held 59-60 through the movie and three demo races (including
2-player split screen), 0 hitches over 25 ms, correct rendering. iOS is not
built with it yet (needs an `arm64-apple-ios15` triple and checking that the
sideload restrictions allow the objects as-is; they contain no JIT).

**Custom GPU drivers (Android).** Home screen: "Install GPU driver (.zip)" /
"Use system driver". Takes standard adrenotools packages (meta.json +
Vulkan library, e.g. Mesa Turnip builds); validates meta.json, libraryName,
minApi and rejects zip path traversal. The package is extracted to
`files/GpuDrivers/Extracted`; libadrenotools (already in Dolphin) loads it via
the hook libraries, which the APK now ships (libhook_impl, libmain_hook,
libfile_redirect_hook, libgsl_alloc_hook). Only offered when
`Vulkan::SupportsCustomDriver()` (API 28+, Adreno/KGSL). gameplay.log and
logcat print `Vulkan device: ...` on the first frame. Test hooks: intent
extras `--es driver <path to zip>` and `--ez systemdriver true`.

Verified with MrPurple Turnip T30 (26.3.0-devel): device reported as
"PurpleVK-public Adreno (TM) 740", game runs. The first launch on a new driver
waits ~50 s while every cached shader compiles for it (WaitForShadersBefore
Starting); later launches are normal.

Follow-up: Dolphin saves the Vulkan pipeline cache only on a clean shutdown,
which a killed Android app never gets, so Turnip recompiled its startup
shaders on every launch (46.8 s). The Android host now also saves it after the
first frame and when the app is backgrounded: relaunch with Turnip 0.55 s.
(The stock Qualcomm driver hid this with its own internal cache.)

## LLVM backend on every platform — November 11, 2026 (Claude)

main.dol now uses DolRecomp's LLVM backend by default everywhere; _Main.rel
stays C. `--c-backend` (mobile scripts) or `BACKEND=c` (Makefile) selects C.

- macOS: `make recompile GAME=Sonic-Riders-USA-En-Ja-Fr-De-Es-It` builds the
  runtime (build/codex-runtime, DOLRECOMP_ENABLE_LLVM=ON, Homebrew LLVM 20
  found automatically) and the LLVM module, and makes it active
  (previous: active-module-before-llvm.txt). moderngekko-port no longer
  rejects LLVM with a recompiled REL.
  Attract loop on the M3 Pro: 59.9 FPS throughout, whole process ~50% of one
  core; shutdown counters native=213M fallback=0 smc_failed=0, interpreted
  hook-fallback instructions 3.75M (C module: 261M per run).
- iOS: `python3 scripts/ios.py gameplay-app --ipa` (objects for
  arm64-apple-ios15.0, generated into build/llvm-ios/).
- Android: `python3 scripts/android.py` (LLVM default).

Fixes found on the way:
- DolRecomp's LLVM backend put a cold helper in an ELF-only section
  (".text.unlikely.*"), which Mach-O rejects: every Apple build failed. Now
  ELF-only (fp_fixups.cpp, both DolRecomp copies).
- The project path contains parentheses: ModernGekko's dolrecomp copy step
  lacked VERBATIM, and moderngekko-port forced Ninja response files for every
  compile, whose unquoted "@path.rsp" broke under sh. Response files are now
  left to CMake (it still uses one for the module link) except on Windows.
- Makefile: tool builds moved to build/dolrecomp and build/codex-runtime
  (lib/*/build held caches from the old Gemini workspace path).

Windows x64 and Linux Flatpaks (cross-built on macOS, untested on target
hardware yet; commands in ports/README.md):
- Windows: clang-cl + lld-link + xwin CRT/SDK (ports/windows/clang-cl-xwin.cmake),
  static CRT, imports only system DLLs. Dolphin needed: no /WX or PCH under
  clang-cl, optional PowerShell, -Wno-c++11-narrowing on the D3D backends,
  the CPUCull target attribute for clang-cl, <functional> in FZeroAX.cpp,
  _putenv_s in moderngekko-run, no libm on Windows (GXRuntime, libspng —
  saved as patches/libspng-windows.patch), and ASSERT_MSG reordered:
  clang-cl's MS-compatible preprocessor deletes the comma before an empty
  __VA_ARGS__, which glued the condition string onto the format string.
- Linux: Fedora 41 container + flatpak-builder on freedesktop 25.08
  (ports/linux), flatpak-builder always runs natively and builds x86_64 with
  Rosetta-run tools (an x86_64 bwrap under Rosetta can't install seccomp).
  Headless smoke test in a container with --graphics Null: both bundles load
  the module and boot with fallback=0, smc_failed=0 (aarch64 45 s,
  5.36G guest cycles; x86_64 under Rosetta on the v2 path).

## Original controller artwork — 2026-11-17

Inspected owner-provided Xbox and PS2 USA discs. Xbox drift/shake now uses
the original Xbox L-stick artwork and both triggers use its original trigger
silhouette, replacing PS2-derived generic keycaps. Original PlayStation L/R
lettering and keycap shading are preserved; only the binding digit becomes 2
for Turbulence's existing analog-trigger controls. Full 64px/48px cells are
replaced, including transparency. Shared renderer changes apply when each
platform is rebuilt. See CONTROLLER-PROMPTS.md for provenance and extraction.

Mac runtime build and sanitized atlas/artwork tests pass. Xbox extraction
reproduces the artwork header byte for byte. Live Metal captures verify Xbox
Boost B, PlayStation Boost Circle and both shake atlases. Drift/trigger
coverage was verified through cell tests; not all situational cues were
triggered live. No Android/iOS/Windows release packages rebuilt in this pass.
Existing unrelated Windows changes were preserved; no commit requested.

### Boost identification correction — 2026-11-17

The earlier claim that the orange B was Xbox was wrong: the shared atlas was
misidentified. GameCube uses x=426 (flat orange-red B); Xbox uses x=376
(glossy red B). Finding both in the Xbox disc was not evidence of which cell
Xbox displays. Corrected both Boost aliases to x=376 in Xbox mode, updated the
regression expectation and rebuilt macOS. The captured live Xbox texture now
matches x=376 exactly and differs from x=426. Replaced the mislabeled local
comparison with boost-button-comparison-corrected.png. PlayStation remains
x=401. Original controller stick/trigger artwork changes are unaffected.

## Centered HUD background and results edges — 2026-11-17

Full-width solid menu panels now fill the viewport even when the background
is split into horizontal quads. Track select's side gaps become blue while
its textured circular design and foreground remain centered. Results silver
wedge vertices at the original left boundary extend to the physical edge;
other vertices, portrait circles, ranks and text retain the centered transform.
The silver extension is disabled in stretched HUD mode and at native 4:3.
The results atlas is selected by content hash (46f9f97b69e89d02), not size alone.

Validation: rebuilt macOS runtime; sanitized viewport/cursor tests pass with
full-width panel exclusion checks and silver edge/interior/stretched cases.
Offscreen Metal captures pass for track select at centered 16:9, centered
32:9 and stretched 16:9 (build/hud-fill). The owner verified the live results
fix in the test game. Shader waiting was disabled only in isolated offscreen
test profiles because its progress UI requires an ImGui context. The normal
launcher settings were not changed. Vendor patches exported and reverse-apply
checks pass. No commit requested.

### Menu scanline coverage — 2026-11-17

Extended the three original scanline strips across the full viewport. Only
the identified menu-atlas strips (0a503a7ee2456e29, original half-width x
[-1,0], 128px high) are widened. Vertical positions/UVs are unchanged, so the
scanline pitch is unchanged; their original horizontal fade spans the screen
without gutter seams. Circular art and foreground draws remain independent.
The treatment also works with stretched HUDs; native 4:3 is unchanged.

Mac runtime rebuilt. Offscreen Metal captures verified centered 16:9,
centered 32:9 and stretched 16:9 in build/hud-fill/*-scanlines. Pixel-column
analysis measured 12px line spacing in both the original and extended strip.
Sanitized viewport tests cover strip classification and horizontal projected
movement preserving Y, including rotated matrices and singular rejection.
Vendor patch export and reverse-apply checks pass. Restart the game to load
the rebuilt runtime; no commit requested.

### Menu gray dividers and footer coverage — 2026-11-17

Extended the flat outer columns of the gray divider atlas
(8ef9802115e76654), top/bottom gray trim (549fadeb773fc748), and footer
bevels (0406bb54a29b5554) to the viewport edges in centered HUD mode.
The footer's half-width solid gradient draws continue their edge colors as
well. Added quads preserve the original artwork, curves, vertical UVs,
scanline pitch and text placement. Atlas samples are slightly inset to
avoid transparent boundary texels, with a small overlap only for textured
trim; translucent gradient joins do not overlap. Native 4:3 and stretched
HUD mode bypass this padding.

Mac runtime rebuilt; sanitized viewport/cursor tests pass, including outer
endpoint, internal join and already-offscreen exclusion cases. Offscreen
Metal captures checked centered 16:9, centered 32:9 and stretched 16:9 in
build/hud-fill/*-bars. Final 32:9 capture in debug-32-9-bars confirms footer
joins without dark seams. Vendor patches exported; reverse-apply and
whitespace checks pass. Restart the game for the rebuilt runtime. No commit
requested.
