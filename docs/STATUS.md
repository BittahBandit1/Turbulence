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
