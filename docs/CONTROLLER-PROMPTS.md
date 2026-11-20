# Controller prompt artwork

The prompt recipes are shared by desktop, Android and iOS. They replace
texture cells at render time; the imported GameCube files remain unchanged.
Rebuild each platform's runtime to include changes. Existing APKs/IPAs and
Windows packages do not update when the source changes.

## Xbox artwork

The former Xbox drift and turbulence mappings reused the PlayStation rubber
stick and drew LT/RT over PlayStation keycaps. Xbox now uses the original
Xbox release's stick marked L and trigger silhouettes. The same silhouette
is present in both trigger positions on the original Xbox sheet.

`src/turbulence_xbox_artwork.hpp` stores lossless RLE RGBA cells, extracted
from the owner-provided Sonic Riders USA Xbox XISO. Source: root file `1`,
decompressed PVRT offset `0x2f8a50`, DXT5, 256x64. The stick is at x=0 and
the triggers at x=64/128. The generated header records the full decoded
sheet's SHA-256. The extractor verifies this digest before generating output:

```sh
python3 scripts/extract-xbox-prompts.py '/path/to/Sonic Riders.xiso.iso'
```

Pillow is required for DXT5 decoding. No disc image, executable, or stage
archive is included in the source tree. Only the three controller cells are
retained in the generated artwork header. Users of a built runtime do not
need to import an additional Xbox disc to use these prompts.

The drift stick cell is replaced at 64x64; the shake cue uses the same source
scaled to 48x48. Every pixel in each destination cell is replaced, including
transparent borders, so the GameCube rim cannot survive behind an Xbox icon.

## PlayStation artwork

The PS2 disc was inspected separately. Its stage-1 controller textures are
in STAGE.DAT, compressed chunk 0x27c000: shared stick/shoulder atlas at
PVRT 0x37e6e0, and auxiliary controller sheet at 0x382740. The original PS2
uses a rubber thumbstick and rectangular L1/R1 keycaps; the GameCube edition
already carries those graphics in its unused platform cells. The 16 palette
index counts of the shared PS2 and GameCube controller sheets match exactly.

Keep those original PlayStation cells rather than substituting Xbox graphics.
Turbulence binds drift to analog L2/R2, so only the original keycap's digit
changes from 1 to 2. The native L/R lettering, shading, frame and transparency
are preserved. Xbox no longer uses this caption path.

## Boost and menus

The GameCube HUD atlas contains all three face-button families. Xbox Boost
uses its original B cell at x=376 (GameCube B is x=426); PlayStation uses Circle at x=401. Both B
aliases are covered by full 25px copies. GameCube mode bypasses replacements.

## Validation (2026-11-17)

- macOS runtime rebuilt successfully.
- Address/undefined sanitizer tests: `tests/prompt-artwork.cpp` and
  `tests/trigger-caption.cpp` pass. These check golden Xbox pixel digests,
  complete cell coverage, transparent rims, smaller shake cells, unchanged
  neighbours, original PlayStation L/R lettering and Boost B/Circle routing.
- Extraction from the Xbox XISO reproduces the generated header byte for byte.
- Live Metal races: Xbox Boost B, PlayStation Boost Circle and the patched shake atlases were captured
  in build/prompt-artwork-test. No GPU or static-code verification errors were
  observed. The drift and trigger replacements were checked through cell
  tests; not every situational prompt was triggered during the live run.
- Mobile and Windows package rebuilds/device testing remain separate.

Correction: the initial Boost comparison mislabeled the two B cells. Merely
finding a cell in the Xbox disc did not establish that Xbox uses it: every
platform carries the shared atlas. The recipe and regression expectation now
select x=376 for Xbox; x=426 is the original GameCube B. The earlier comparison
and its claimed platform identification were incorrect.
