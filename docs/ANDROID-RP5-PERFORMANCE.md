# RP5 Android performance — November 16, 2026

Latest native-module APK: stationary Metal City measures **99.8% median speed**
(with 95.2% dips), and the matched opening sequence averages **97.67%** at the
original 2x resolution. The target of sustained 100% remains unmet. Detailed
results and artifacts for each optimization pass follow.

Measured on Retroid Pocket 5 over USB debugging, system Adreno 650 Vulkan driver,
existing saved settings (2x internal resolution, VSync enabled). App data and
save were preserved. These are short scene samples, not a full-course or
thermal endurance benchmark.

## Changes

- Restored the current-draw vertex pointer already described in dolphin.patch.
  Vulkan's base pointer describes the entire
  stream buffer, not the pending draw. Measuring the old base repeatedly scanned
  stale vertices in slow mapped GPU memory and misclassified backgrounds.
- Reject batches other than four-vertex quads before reading vertex positions.
  The existing fullscreen-background predicate already rejects those batches.
  Four-vertex background classification and its aspect rules are retained.
- Android LLVM generation now aligns object boundaries with guest function
  starts, with a 4096-instruction cap, using a separate versioned cache. Main
  game floating-point semantics remain exact; REL remains the C backend.
  No isolated performance benefit is established for this layout change;
  the large measured UI improvement is attributable to removing the vertex scan.

## Device results

| Scene | Baseline | Final |
| --- | --- | --- |
| Load-game menu | 60.75% median (12 five-second samples) | 99.7% median / about 60 FPS |
| Load confirmation | about 38% | about 100% / 60 FPS |
| Character/Blue Star selection | about 19–23% | about 100% / 60 FPS |
| Metal City, Sonic/Blue Star, starting area | about 68–77%, final 12-sample median 71.8% | roughly 90–100%, with brief lower dips |
| Short moving Metal City sample | no matched moving baseline | high 80s–90s, screenshot 96% |
| Opening movie | CPU-bound, below full speed | still below full speed; observed 65–90% dips |

The menu's memcpy self share fell from 74.74% to 0.27% of sampled CPU time.
Final MeasureExtent self share was 0.05%. Profiles use cpu-clock at 200 Hz,
12-second samples with call stacks. Percentages are sampling shares, not frame
budgets. Movie and moving-gameplay samples are not a proof of a universal 100%
lock; more CPU-side optimization is required. The remaining gameplay profile
shows generated routines, floating-point helpers and CPU/video synchronization.

## Validation and artifacts

- Existing viewport/cursor C++ tests passed.
- Mobile build tests passed (two executed, one personal-source test skipped).
- APK signature verification passed (v2 and v3).
- USB update installation succeeded; final process was launched and sampled.
- Screenshots, speed logs and raw profiles are in build/rp5-performance/.
- Baseline rollback APK: build/release/Turbulence-Android-arm64-baseline.apk.
- Installed final APK: build/release/Turbulence-Android-arm64-test.apk.

First-pass APK SHA-256: `f3775580974b7f0327b29092bb23c4b7c52069fc8503ec1c71b6106ee5fe4091`

## Follow-up CPU optimization

The arithmetic-only APK (`Turbulence-Android-arm64-pass2.apk`) improves the
matched opening sequence, virtual frames 600–3000, from 50.77 FPS / 84.70% to
55.03 FPS / 91.82%. Speed uses the 59.94 Hz guest target and elapsed logcat
frame timestamps; occasional missing exact frame samples are interpolated
between adjacent frame reports. Both runs retain the same saved 2x settings.
These figures describe this sequence, not an entire movie or a gameplay lock.

- Scalar adds/subtracts/multiplies retain their original exceptional paths and
  use a shorter finite-result path. Differential tests compare 4,800,000 cases
  on the RP5, covering rounding modes, NI/VE, aliasing and exceptional operands.
- The successful FPU availability gate avoids reloading guest registers that
  neither runtime changes; fault paths retain the exception exit.
- Dynamic quantized paired loads now have direct paths for supported GQR types.
  The tested full APK includes this change.
- PSQ stores preserve signed zero and match Dolphin's bit conversion, including
  NaN payloads. Memory callback values use their declared access width, and
  reservation invalidation occurs on the correct RAM path.
- 524,288 generated PSQ comparisons pass on the RP5, including load/store,
  quantization, alignment, disabled features, RAM/MMIO and reservation checks.
  All four DolRecomp LLVM regression tests pass.
- Android generation caches include the recompiler binary hash and require a
  completion marker. CMake now recomputes the emitter hash when sources change.

The arithmetic-build CPU sample still spends 8.65% in generic PSQ load helpers,
which the pending direct-load build targets. Profile shares do not establish
an equivalent end-to-end speedup.

### Direct-load APK results

The direct-load APK (`Turbulence-Android-arm64-pass3.apk`) measures **58.07 FPS /
96.88%** over the same opening frames 600–3000, versus 50.77 FPS / 84.70% in
pass 1 and 55.03 FPS / 91.82% in pass 2. It reaches 99–100% in portions of the
sequence but does not sustain that throughout the timed window.

Stationary Metal City remains variable: the last twelve five-second samples
have a median 93.85% (pass 2: 94.75%). These short runs are not evidence of a
gameplay improvement from the direct-load change. A separate affinity experiment
(CPU on core 7, video on cores 4–6) did not yield a clear gain and was reverted
to the original cores 4–7. Saved graphics settings remain unchanged.

A subsequent small runtime patch rejects invalidation searches outside the
sorted compiled-code ranges, while retaining REL discovery invalidation.
The incremental build passed signature verification and USB installation.
The final APK measures 58.17 FPS / 97.05% over the same opening window. Its
settled stationary race sample remains 92.15% median, ranging 82.1–100.2% across
twelve five-second samples. No gameplay benefit is established for this small
runtime change. The target of a sustained 100% gameplay lock remains unmet.

Installed APK: `build/release/Turbulence-Android-arm64-test.apk` (also saved as
`Turbulence-Android-arm64-pass4.apk`). SHA-256:
`ae9f69c8a371d1b2f0911797311fd1c3bf7a374d97abca84845427b639e3b969`.

Further work should focus on generated scalar arithmetic/helper boundaries,
REL C routines and runtime dispatch/address translation, which remain visible
in the gameplay CPU profile. Changing graphics resolution was not part of this
pass.

## Direct scalar arithmetic pass

Generated scalar adds, subtracts, multiplies and single fused multiply-add
variants now keep ordinary normal single results in generated ARM64 code.
The direct path uses constrained operations with dynamic host rounding. It
updates the same guest FPSCR classification and multiply FI/FR bits. Results
near underflow/overflow, exceptional values, subnormal multipliers and fused
halfway-rounding correction cases retain the original exact runtime helper.

The RP5 differential test compares 4,160,000 generated scalar results and FPSCR
values against GXRuntime, covering all four rounding modes, NI/VE, destination
aliasing, dependent mixed chains, random doubles, ordinary game-like values,
NaN/Inf, signed zeros, subnormals and explicit halfway/residual cases. All pass.
The existing 4,800,000 scalar helper and 524,288 PSQ comparisons also pass, as
do all four DolRecomp LLVM regression tests. End-to-end gains remain unmeasured
until the full APK completes.

This pass also enables Android ThinLTO inlining for the scalar helpers used by
the C-generated REL, while keeping their external definitions for native LLVM
fallback calls. The complete RP5 differential suite passes with those
attributes enabled. Runtime REL translation now maintains coarse linked/runtime
bounds and skips the section scan for addresses outside them. Bounds are reset
with the active mapping and recomputed whenever the mapping changes; matching
addresses still use the original section-order lookup.

### Scalar pass measured result

Pass 5 built, signed and installed successfully (SHA-256
`f3243b6d3840fa67f86a727dca3fa9275058850b816e3abe47ea8197fba1f7f3`).
The opening window measures 58.32 FPS / 97.29%. Stationary race frames
10000–13000 measure 54.96 FPS / 91.69%; the last twelve five-second readings
have median 91.15%. The arithmetic helper symbols disappear from the top CPU
costs, but these samples establish no overall race-speed gain. This remains
short of a 100% lock.

### Native REL compilation

The loader incorrectly applied header module alignment to every executable
section, and wrote 16-bit relocations two bytes past their actual offset.
It also lacked the pinned BSS environment setting already expected by the host
port. The fixes retain instruction alignment checks, validate the module base,
patch exactly two bytes for halfword relocations (including at section end),
and support the game's independently allocated BSS at `0x805FB080`.

REL regression tests cover module/section alignment, version-1 headers,
misaligned module bases, and all four halfword relocation forms at normal and
last-halfword offsets. They pass. These rules were checked against the
[reference SDK linker implementation](https://github.com/doldecomp/dolsdk2004/blob/master/src/os/OSLink.c).
Reconstructing the image from the original `_Main.rel` with base `0x803FBD00`
and pinned BSS now exactly matches the existing working image:
`c7aa1527f737c1771311bfd5f4b355219fab977c5b1e59343b184839d42b8fc5`.
The module build now accepts the native LLVM object manifest as well as C output.
Performance is pending compilation and device testing.

### Native REL APK: latest tested result

All 1,308 native REL objects compiled successfully, with zero unknown decoded
instructions and an empty fallback-instruction report. The combined module has
2,206 chunks. The APK built, passed v2/v3 signature checks and was installed on
the RP5, retaining the same saves, 2x resolution and VSync setting.

- Opening frames 600–3000: **58.54 FPS / 97.67%**.
- Stationary Metal City frames 10000–13000: **59.09 FPS / 98.58%** effective speed.
- Last twelve settled five-second race samples: **99.8% median**, range
  **95.2–99.9%**. This is a substantial improvement over the previous ~91–92%
  sample, but remains short of a universal 100% lock.
- A 15-second CPU sample shows 7,334 of 7,336 CPU-thread samples on core 7.
  The remaining dips are not explained by frequent migration off the fastest core.
- A temporary VSync-off diagnostic on pass 5 measured 97.37% in the opening,
  versus 97.29% with VSync on. It provided no meaningful gain. The exact original
  GFX.ini was restored and verified before installing this APK.
- All seven affected compiler/source/loader checks pass. The reorganized compiler
  also passes 4,160,000 generated scalar, 4,800,000 scalar-helper, and 524,288 PSQ
  device comparisons. The native DOL + REL integration fixture passes ABI,
  metadata, linking and 2,000 dispatch checks on the RP5.

Latest installed APK is saved as `Turbulence-Android-arm64-pass6.apk` and as
`Turbulence-Android-arm64-test.apk`. SHA-256:
`4111c4f39965e1ca5ef975108bb2c3c1afdbf0c48d0ee6deb720f9f2b224cb22`.
Main native output: `build/llvm-android-functions-v2-ea7ef9fce477/dol/generated`.
REL native output: `build/android-rel-llvm-pass6/generated`.
The full APK is 99 MiB; the main and REL now both use native LLVM objects.

The RP5 disconnected from USB during subsequent work on the requested iOS HUD
feature. Further moving-gameplay and speed verification requires reconnection.
No full-lap or thermal endurance claim is established by these samples.
