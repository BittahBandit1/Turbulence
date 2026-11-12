"""Shared checks for personal Sonic Riders mobile builds."""
from hashlib import sha256
from pathlib import Path
import os
import shutil
import subprocess

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


def llvm_main(root, c_generated, triple, name):
    """LLVM-backend objects for the C output's main.dol, for one target triple
    (e.g. aarch64-unknown-linux-android26, arm64-apple-ios15.0). Generated
    into build/llvm-<name>/ on first use; _Main.rel stays C-backend output.
    Needs DolRecomp built with LLVM 19/20 in lib/DolRecomp/build-llvm."""
    root = Path(root)
    generated = root/f'build/llvm-{name}/dol/generated'
    if not (generated/'generated.h').is_file():
        dolrecomp = root/'lib/DolRecomp/build-llvm/dolrecomp'
        if not dolrecomp.is_file():
            raise SystemExit('Build DolRecomp with LLVM first: cmake -S lib/DolRecomp -B lib/DolRecomp/build-llvm '
                             '-G Ninja -DCMAKE_BUILD_TYPE=Release -DDOLRECOMP_ENABLE_LLVM=ON '
                             '-DLLVM_DIR=<llvm 19/20>/lib/cmake/llvm && cmake --build lib/DolRecomp/build-llvm --target dolrecomp')
        generated.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run([str(dolrecomp), '-j', str(os.cpu_count() or 4), '--backend=llvm', '--targets', 'aarch64',
                        '--cpu', 'gekko', '--gamecube', str(Path(c_generated)/'main.dol'), str(generated.parent)],
                       cwd=root, check=True, env=dict(os.environ, DOLRECOMP_LLVM_TARGET=triple))
    shutil.copy2(Path(c_generated)/'main.dol', generated/'main.dol')
    return generated
