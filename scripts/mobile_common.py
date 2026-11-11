"""Shared checks for personal Sonic Riders mobile builds."""
from hashlib import sha256
from pathlib import Path

DOL_SHA256 = '8b36ae0c3c86bc249b1bf0b05ea636478589aad25a5632f21877f60c900a94a1'

def generated_sources(root, generated=None, rel_generated=None):
    candidates = [Path(generated)] if generated else sorted(
        (Path(root)/'build/modules/GXEE8P').glob('*/dolrecomp-output/generated'))
    for candidate in candidates:
        candidate = candidate.resolve()
        rel = Path(rel_generated).resolve() if rel_generated else candidate.parent/'rel/generated'
        required = [candidate/'generated.h', candidate/'generated_smc.txt', candidate/'main.dol',
                    rel/'generated.h', rel/'generated_smc.txt', rel/'rel_image.bin']
        if not all(p.is_file() for p in required):
            continue
        if sha256((candidate/'main.dol').read_bytes()).hexdigest() != DOL_SHA256:
            continue
        # The main executable may be C or LLVM-backend output (precompiled
        # objects); the REL is always C.
        llvm = 'DOLRECOMP_BACKEND_LLVM' in (candidate/'generated.h').read_text(errors='ignore')
        main_chunks = (candidate/'chunks').glob('*.o' if llvm else '*.c')
        if not any(main_chunks) or not any((rel/'chunks').glob('*.c')):
            continue
        return candidate, rel
    raise ValueError('Generate Sonic Riders USA code with the project build first, or supply '
                     '--generated and --rel-generated pointing to matching main.dol and REL C output.')
