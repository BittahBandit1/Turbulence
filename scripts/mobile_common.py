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
        # Each executable may be C or LLVM-backend output (precompiled
        # objects), provided it was generated for the target platform.
        llvm = 'DOLRECOMP_BACKEND_LLVM' in (candidate/'generated.h').read_text(errors='ignore')
        main_chunks = (candidate/'chunks').glob('*.o' if llvm else '*.c')
        rel_llvm = 'DOLRECOMP_BACKEND_LLVM' in (rel/'generated.h').read_text(errors='ignore')
        rel_chunks = (rel/'chunks').glob('*.o' if rel_llvm else '*.c')
        if not any(main_chunks) or not any(rel_chunks):
            continue
        return candidate, rel
    raise ValueError('Generate Sonic Riders USA code with the project build first, or supply '
                     '--generated and --rel-generated pointing to matching main.dol and REL output.')


def llvm_main(root, c_generated, triple, name):
    """LLVM-backend objects for the C output's main.dol, for one target triple
    (e.g. aarch64-unknown-linux-android26, arm64-apple-ios15.0). Generated
    into build/llvm-<name>/ on first use; the separately supplied REL output
    keeps its existing backend.
    Needs DolRecomp built with LLVM 19/20 in lib/DolRecomp/build-llvm."""
    root = Path(root)
    # Keep hot guest functions/loops together on Android. Fixed 128-instruction
    # partitions force repeated native dispatch and register-state transfers.
    # Use a distinct cache so old fixed-size objects cannot silently be reused.
    android_functions = name == 'android'
    dolrecomp = root/'lib/DolRecomp/build-llvm/dolrecomp'
    missing_compiler = ('Build DolRecomp with LLVM first: cmake -S lib/DolRecomp -B lib/DolRecomp/build-llvm '
                        '-G Ninja -DCMAKE_BUILD_TYPE=Release -DDOLRECOMP_ENABLE_LLVM=ON '
                        '-DLLVM_DIR=<llvm 19/20>/lib/cmake/llvm && cmake --build lib/DolRecomp/build-llvm --target dolrecomp')
    if android_functions and not dolrecomp.is_file():
        raise SystemExit(missing_compiler)
    # A changed compiler must not silently reuse objects from an older emitter.
    compiler_key = sha256(dolrecomp.read_bytes()).hexdigest()[:12] if android_functions else ''
    cache_name = f'{name}-functions-v2-{compiler_key}' if android_functions else name
    generated = root/f'build/llvm-{cache_name}/dol/generated'
    complete = generated/'.generation-complete'
    if not (generated/'generated.h').is_file() or (android_functions and not complete.is_file()):
        if not dolrecomp.is_file():
            raise SystemExit(missing_compiler)
        generated.parent.mkdir(parents=True, exist_ok=True)
        generation_env = dict(os.environ, DOLRECOMP_LLVM_TARGET=triple)
        if android_functions:
            generation_env.update(DOLRECOMP_LLVM_FUNCTION_CHUNKS='1',
                                  DOLRECOMP_LLVM_FUNCTION_CHUNK_MAX='4096',
                                  DOLRECOMP_LLVM_CHUNK_INSTRUCTIONS='128',
                                  DOLRECOMP_LLVM_CACHE=str(generated.parent.parent/'object-cache'))
        subprocess.run([str(dolrecomp), '-j', str(os.cpu_count() or 4), '--backend=llvm', '--targets', 'aarch64',
                        '--cpu', 'gekko', '--gamecube', str(Path(c_generated)/'main.dol'), str(generated.parent)],
                       cwd=root, check=True, env=generation_env)
    shutil.copy2(Path(c_generated)/'main.dol', generated/'main.dol')
    if android_functions:
        complete.touch()
    return generated


def _find_rel_source(root, c_rel_generated):
    """Locate the original files/_Main.rel, falling back to the linked
    rel_image.bin that the C-backend REL output already carries."""
    c_rel_generated = Path(c_rel_generated)
    for rel in sorted((Path(root) / 'extracted').glob('*/files/_Main.rel')):
        return rel.resolve()
    candidate = c_rel_generated / 'rel_image.bin'
    if candidate.is_file():
        return candidate.resolve()
    raise ValueError('Cannot find files/_Main.rel (or a rel_image.bin fallback).')


def llvm_rel(root, c_rel_generated, triple, name):
    """LLVM-backend objects for the C output's _Main.rel, for one target triple
    (e.g. aarch64-unknown-linux-android26, arm64-apple-ios15.0). Generated into
    build/llvm-<name>-rel/ on first use. Mirrors llvm_main(), plus the REL's
    --rel-base and DOLRECOMP_REL_BSS. The REL is about two thirds of the game's
    code, so compiling it with the LLVM backend (instead of C) is the largest
    single win for CPU-bound devices like the Snapdragon 7s Gen 2 and A13."""
    root = Path(root)
    c_rel_generated = Path(c_rel_generated)
    android_functions = name == 'android'
    dolrecomp = root / 'lib/DolRecomp/build-llvm/dolrecomp'
    missing_compiler = ('Build DolRecomp with LLVM first: cmake -S lib/DolRecomp -B lib/DolRecomp/build-llvm '
                        '-G Ninja -DCMAKE_BUILD_TYPE=Release -DDOLRECOMP_ENABLE_LLVM=ON '
                        '-DLLVM_DIR=<llvm 19/20>/lib/cmake/llvm && cmake --build lib/DolRecomp/build-llvm --target dolrecomp')
    if not dolrecomp.is_file():
        raise SystemExit(missing_compiler)
    compiler_key = sha256(dolrecomp.read_bytes()).hexdigest()[:12] if android_functions else ''
    cache_name = f'{name}-rel-functions-v2-{compiler_key}' if android_functions else f'{name}-rel'
    generated = root / f'build/llvm-{cache_name}/rel/generated'
    complete = generated / '.generation-complete'
    if not (generated / 'generated.h').is_file() or (android_functions and not complete.is_file()):
        rel_source = _find_rel_source(root, c_rel_generated)
        generated.parent.mkdir(parents=True, exist_ok=True)
        env = dict(os.environ,
                   DOLRECOMP_LLVM_TARGET=triple,
                   DOLRECOMP_REL_BSS='0x805FB080')
        if android_functions:
            env.update(DOLRECOMP_LLVM_FUNCTION_CHUNKS='1',
                       DOLRECOMP_LLVM_FUNCTION_CHUNK_MAX='4096',
                       DOLRECOMP_LLVM_CHUNK_INSTRUCTIONS='128',
                       DOLRECOMP_LLVM_CACHE=str(generated.parent.parent / 'object-cache'))
        subprocess.run([str(dolrecomp), '-j', str(os.cpu_count() or 4),
                        '--backend=llvm', '--targets', 'aarch64', '--cpu', 'gekko',
                        '--gamecube', '--rel-base', '0x803FBD00',
                        str(rel_source), str(generated.parent)],
                       cwd=root, check=True, env=env)
    shutil.copy2(c_rel_generated / 'rel_image.bin', generated / 'rel_image.bin')
    if android_functions:
        complete.touch()
    return generated
