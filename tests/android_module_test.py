#!/usr/bin/env python3
"""Build and execute a tiny native DOL + native REL module on Android (no game assets)."""
import argparse
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def run(*args, **kwargs):
    subprocess.run([str(a) for a in args], check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ndk', type=Path, required=True)
    parser.add_argument('--serial', required=True)
    args = parser.parse_args()
    runtime = ROOT/'lib/ModernGekko/vendor/dolphin'
    compiler = ROOT/'lib/DolRecomp/build-llvm/dolrecomp'
    clang = next((args.ndk/'toolchains/llvm/prebuilt').glob('*/bin/clang'))
    env = dict(os.environ, DOLRECOMP_LLVM_TARGET='aarch64-unknown-linux-android26',
               DOLRECOMP_LLVM_CACHE='off')
    with tempfile.TemporaryDirectory(prefix='turbulence-module-test-') as temp:
        temp = Path(temp)
        dol = bytearray(0x108)
        for offset, value in [(0, 0x100), (0x48, 0x80003100), (0x90, 8),
                              (0xe0, 0x80003100), (0x100, 0x38630001), (0x104, 0x4e800020)]:
            struct.pack_into('>I', dol, offset, value)
        rel = bytearray(0x108)
        for offset, value in [(0, 1), (0xc, 3), (0x10, 0x50), (0x1c, 3),
                              (0x40, 32), (0x44, 4), (0x58, 0x101), (0x5c, 8),
                              (0x100, 0x38630007), (0x104, 0x4e800020)]:
            struct.pack_into('>I', rel, offset, value)
        rel[0x30] = 1
        (temp/'test.dol').write_bytes(dol)
        (temp/'test.rel').write_bytes(rel)
        for name, extra in [('dol', []), ('rel', ['--rel-base', '0x80500000'])]:
            run(compiler, '--backend=llvm', '--targets', 'aarch64', '--cpu', 'gekko',
                '--gamecube', *extra, temp/f'test.{name}', temp/name, env=env)
        shutil.copy2(temp/'test.dol', temp/'dol/generated/main.dol')
        shutil.copy2(temp/'test.rel', temp/'rel/generated/rel_image.bin')
        build = temp/'build'
        run('cmake', '-S', runtime/'module-template', '-B', build, '-G', 'Ninja',
            '-DCMAKE_TOOLCHAIN_FILE='+str(args.ndk/'build/cmake/android.toolchain.cmake'),
            '-DANDROID_ABI=arm64-v8a', '-DANDROID_PLATFORM=android-26',
            '-DCMAKE_BUILD_TYPE=Release', '-DGAME_ID=GXEE8P',
            '-DGENERATED_DIR='+str(temp/'dol/generated'),
            '-DREL_GENERATED_DIR='+str(temp/'rel/generated'))
        run('cmake', '--build', build, '-j2')
        executable = temp/'module-test'
        run(clang, '--target=aarch64-linux-android26', '-O2',
            '-I'+str(runtime/'GXRuntime/include'),
            '-I'+str(runtime/'Source/Core/Core/PowerPC/StaticRecomp'),
            ROOT/'tests/android-module.c', '-L'+str(build), '-l:gGXEE8P_recomp.so',
            '-o', executable)
        adb = ['adb', '-s', args.serial]
        destination = '/data/local/tmp/turbulence-native-module-test'
        run(*adb, 'shell', 'mkdir', '-p', destination)
        run(*adb, 'push', build/'gGXEE8P_recomp.so', executable, destination+'/')
        run(*adb, 'shell', 'chmod', '755', destination+'/module-test')
        run(*adb, 'shell', 'env', 'LD_LIBRARY_PATH='+destination, destination+'/module-test')


if __name__ == '__main__':
    main()
