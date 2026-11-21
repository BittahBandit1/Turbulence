#!/usr/bin/env python3
"""Run scalar and generated-PSQ differential tests on a connected ARM64 Android device."""
import argparse
import os
from pathlib import Path
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def run(*args, **kwargs):
    subprocess.run([str(arg) for arg in args], check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ndk', required=True, type=Path)
    parser.add_argument('--serial', required=True)
    args = parser.parse_args()
    compiler = next((args.ndk/'toolchains/llvm/prebuilt').glob('*/bin/clang'))
    recompiler = ROOT/'lib/DolRecomp/build-llvm/dolrecomp'
    runtime = ROOT/'lib/ModernGekko/vendor/dolphin/GXRuntime'
    sources = [runtime/'src/core'/name for name in (
        'cpu.c', 'cpu_exception.c', 'cpu_interpreter.c', 'cpu_interpreter_table.c',
        'cpu_interpreter_float.c', 'cpu_interpreter_integer.c')]
    adb = ['adb', '-s', args.serial]
    with tempfile.TemporaryDirectory(prefix='turbulence-cpu-tests-') as temp:
        temp = Path(temp)
        # Adjacent functions provide distinct native entry points. Each loads
        # f1 from r4, stores it eight bytes later, and returns; the second uses W=1.
        words = [0xe0240000, 0xf0240008, 0x4e800020,
                 0xe0248000, 0xf0248008, 0x4e800020,
                 0xf0240008, 0x4e800020, 0xf0248008, 0x4e800020]
        # Three scalar operations with every destination/source alias, plus
        # a dependent mixed chain to exercise fast/slow joins and FPSCR state.
        for op in range(3):
            for destination in range(3):
                operands = (2 << 6) if op == 2 else (2 << 11)
                xo = (21, 20, 25)[op]
                words += [(59 << 26) | (destination << 21) | (1 << 16) |
                          operands | (xo << 1), 0x4e800020]
        words += [(59 << 26) | (1 << 16) | (2 << 11) | (21 << 1),
                  (59 << 26) | (1 << 21) | (2 << 11) | (20 << 1),
                  (59 << 26) | (2 << 21) | (1 << 16) | (2 << 6) | (25 << 1),
                  0x4e800020]
        # Fused variants, aliasing the destination with a, b and c in turn.
        for xo in (29, 28, 31, 30):
            for destination in range(4):
                words += [(59 << 26) | (destination << 21) | (1 << 16) |
                          (2 << 11) | (3 << 6) | (xo << 1), 0x4e800020]
        header = bytearray(256)
        for offset, value in ((0, 256), (0x48, 0x80003100),
                              (0x90, len(words)*4), (0xe0, 0x80003100)):
            struct.pack_into('>I', header, offset, value)
        dol = temp/'main.dol'
        dol.write_bytes(header + struct.pack('>' + 'I'*len(words), *words))
        run(recompiler, '--backend=llvm', '--targets', 'aarch64', '--cpu', 'gekko',
            '--gamecube', dol, temp/'dol', env=dict(
                os.environ, DOLRECOMP_LLVM_TARGET='aarch64-unknown-linux-android26',
                DOLRECOMP_LLVM_CACHE='off'))
        objects = list((temp/'dol/generated/chunks').glob('*.o'))
        for name, extra in (('scalar-fp', []), ('psq-dynamic', objects), ('scalar-generated', objects)):
            output = temp/name
            run(compiler, '--target=aarch64-linux-android26', '-O2', '-std=c11',
                '-ffp-contract=off', '-I'+str(runtime/'include'),
                '-I'+str(runtime/'src/core'), ROOT/'tests'/f'{name}.c',
                *sources, *extra, '-lm', '-o', output)
            device_path = '/data/local/tmp/turbulence-'+name+'-test'
            run(*adb, 'push', output, device_path)
            run(*adb, 'shell', 'chmod', '755', device_path)
            run(*adb, 'shell', device_path)


if __name__ == '__main__':
    main()
