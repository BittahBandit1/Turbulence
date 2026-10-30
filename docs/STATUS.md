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

## Measured (M3 Pro, Metal, 150-420 s unattended runs)
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

## Frame pacing (on-glass times, 120 Hz MacBook Pro display, windowed)
Measured with the pacing recorder (`TURBULENCE_PACING_DIR`, touch
`pacing-request`; `scripts/pacing-report.py`), 600 frames each:
| | Gemini's pacing | ReFare's pacing |
|---|---|---|
| Race: frames on the 16.67 ms cadence | 593/598 (5 at 25 ms) | 598/598, sd 0.001 ms |
| Race: present vs. schedule (p50) | ~3,560 ms behind | 15 ms |
| Loading into a race | not measured | 576/591, 14 at 25 ms, no drops or 2-frame hitches |

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

## Options menu
Ported from ReFare (`src/turbulence_options.cpp`), Sonic Riders palette.
R1 / Tab by default, rebindable. Not yet checked visually: automation
screenshots capture the game image without overlays.

## Open
- Widescreen: still Gemini's heuristics in Dolphin's video code. Races look
  right; title / menu screens show stretched edge bars. To be redone as
  game-side fixes (like ReFare's culling hook).
- Gemini's `presentDrawable:afterMinimumDuration:0.014` and texture-cache
  changes are still in place, unreviewed.

## Testing
`caffeinate -dims scripts/turbulence_agent.sh` runs jobs from
`build/claude-jobs/`. `scripts/package-play-folder.sh <folder>` refreshes a
play folder from the current build.
