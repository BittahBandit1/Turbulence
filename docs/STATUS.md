# Turbulence status

Sonic Riders (USA, GXEE8P) static recompilation on ModernGekko + DolRecomp.

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

Working sources recovered from `/Users/piestudios/.gemini/users/user1/Turbulence`
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
