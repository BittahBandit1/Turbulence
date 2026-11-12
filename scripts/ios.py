#!/usr/bin/env python3
"""Build the iOS importer, personal gameplay app, or static game module.

Gameplay links personal generated game code into the app before signing.
Imported discs supply assets; unsigned dynamic game modules are not loaded.
"""
import argparse
import os
import shlex
import json
from pathlib import Path
import platform
import re
import subprocess
import struct
import sys
import zipfile
from mobile_common import generated_sources, llvm_main

ROOT = Path(__file__).resolve().parent.parent
CACHE = ROOT / "build/ios-module-cache"
CACHE.mkdir(parents=True, exist_ok=True)
os.environ.setdefault("CCACHE_DIR", str(ROOT / "build/codex-ccache"))
os.environ.setdefault("CLANG_MODULE_CACHE_PATH", str(CACHE))
os.environ.setdefault("SWIFT_MODULECACHE_PATH", str(CACHE))

def run(*args):
    subprocess.run([str(arg) for arg in args], cwd=ROOT, check=True)

def package_ipa(build, gameplay=False):
    app = build / ('turbulence-ios/TurbulenceImport.app' if gameplay else 'Release-iphoneos/TurbulenceImport.app')
    if not (app / 'TurbulenceImport').is_file():
        raise ValueError('The device app was not built; cannot package an IPA')
    # Check the binary as well as the plist; a stale macOS build cannot be sideloaded.
    binary = (app / 'TurbulenceImport').read_bytes()
    if len(binary) < 32 or struct.unpack_from('<II', binary) != (0xfeedfacf, 0x100000c):
        raise ValueError('Expected a thin ARM64 Mach-O device executable')
    offset = 32
    ios15 = False
    for _ in range(struct.unpack_from('<I', binary, 16)[0]):
        command, size = struct.unpack_from('<II', binary, offset)
        if size < 8 or offset + size > len(binary):
            raise ValueError('Invalid executable load commands')
        if command == 0x32 and size >= 24:
            target, minimum = struct.unpack_from('<II', binary, offset + 8)
            ios15 = target == 2 and minimum == 0x0f0000
        offset += size
    if not ios15:
        raise ValueError('Executable does not target physical iOS devices with a 15.0 minimum')
    import plistlib
    with (app / 'Info.plist').open('rb') as source:
        info = plistlib.load(source)
    if info.get('MinimumOSVersion') != '15.0':
        raise ValueError('The app does not declare the required iOS 15.0 minimum')
    version = info['CFBundleShortVersionString']
    if not re.fullmatch(r'\d+(?:\.\d+){0,2}', version):
        raise ValueError('Invalid app version for IPA filename')
    output = ROOT / 'build/release' / f'Turbulence-{"Gameplay" if gameplay else "Import"}-{version}-iOS15-arm64-unsigned.ipa'
    output.parent.mkdir(parents=True, exist_ok=True)
    pending = output.with_suffix('.ipa.tmp')
    with zipfile.ZipFile(pending, 'w', zipfile.ZIP_DEFLATED) as archive:
        for file in sorted(app.rglob('*')):
            if file.is_symlink():
                raise ValueError('Unexpected symlink in app bundle')
            if file.is_file() and file.suffix != '.d':
                archive.write(file, str(Path('Payload') / app.name / file.relative_to(app)))
    pending.replace(output)
    print(f'IPA: {output}\nThis unsigned testing IPA must be signed by your sideloading tool.')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('target', choices=['import-app', 'gameplay-app', 'module'])
    parser.add_argument('--simulator', action='store_true')
    parser.add_argument('--generated', type=Path,
                        help='Personal DolRecomp C output (generated.h, main.dol, chunks/)')
    parser.add_argument('--rel-generated',type=Path,required=False,help='Generated Sonic Riders REL code')
    parser.add_argument('--team', help='Apple development team ID, only for signing the import app')
    parser.add_argument('--jobs', type=int, default=6)
    parser.add_argument('--ipa', action='store_true', help='Package an unsigned device app for sideloading')
    parser.add_argument('--c-backend', action='store_true',
                        help='use the C backend for main.dol instead of LLVM (the REL is always C)')
    args = parser.parse_args()
    if platform.system() != 'Darwin':
        parser.error('The iOS toolchain requires macOS and Xcode')
    if args.target in ('module', 'gameplay-app'):
        args.generated, args.rel_generated = generated_sources(ROOT, args.generated, args.rel_generated)
        if not args.c_backend and not args.simulator:
            args.generated = llvm_main(ROOT, args.generated, 'arm64-apple-ios15.0', 'ios')
    if args.ipa and (args.target not in ('import-app', 'gameplay-app') or args.simulator or args.team):
        parser.error('--ipa requires an unsigned device app build (no --simulator or --team)')
    if args.target == 'gameplay-app':
        if args.simulator or args.team:
            parser.error('The gameplay test builder currently packages unsigned physical-device apps')
        build = ROOT / 'build/ports/ios-runtime-ninja'
        run('cmake', '-S', ROOT/'lib/ModernGekko', '-B', build, '-G', 'Ninja',
            '-DCMAKE_SYSTEM_NAME=iOS', '-DCMAKE_OSX_SYSROOT=iphoneos',
            '-DCMAKE_OSX_ARCHITECTURES=arm64', '-DCMAKE_OSX_DEPLOYMENT_TARGET=15.0',
            '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_Swift_FLAGS=-target arm64-apple-ios15.0 -module-cache-path '+shlex.quote(str(CACHE)),
            '-DBUILD_TESTING=OFF', '-DMODERNGEKKO_ENABLE_DYNAMIC_MODULES=OFF',
            '-DENABLE_VULKAN=OFF', '-DUSE_BUNDLED_MOLTENVK=OFF',
            f'-DTURBULENCE_IOS_APP_SOURCE_DIR={ROOT / "ports/ios"}',
            f'-DTURBULENCE_IOS_GENERATED_DIR={args.generated.resolve()}',
            f'-DTURBULENCE_IOS_REL_GENERATED_DIR={args.rel_generated.resolve()}',
            f'-DTURBULENCE_OPTIONS_SOURCE_DIR={ROOT / "src"}')
        run('cmake', '--build', build, '--target', 'TurbulenceImport', '--parallel', args.jobs)
        if args.ipa: package_ipa(build, gameplay=True)
        return

    sdk = 'iphonesimulator' if args.simulator else 'iphoneos'
    build = ROOT / 'build/ports' / f'ios-{args.target}-{sdk}'
    source = ROOT / 'ports/ios'
    extra = []
    if args.target == 'module':
        source = ROOT / 'lib/ModernGekko/vendor/dolphin/module-template'
        extra += ['-DGAME_ID=GXEE8P', f'-DGENERATED_DIR={args.generated.resolve()}', f'-DREL_GENERATED_DIR={args.rel_generated.resolve()}',
                  '-DRECOMPCORE_MODULE_STATIC=ON', '-DRECOMPCORE_MODULE_ENABLE_IPO=OFF',
                  '-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY']
    if args.team:
        extra += [f'-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM={args.team}']
    run('cmake', '-S', source, '-B', build, '-G', 'Xcode',
        '-DCMAKE_SYSTEM_NAME=iOS', f'-DCMAKE_OSX_SYSROOT={sdk}',
        '-DCMAKE_OSX_ARCHITECTURES=arm64', '-DCMAKE_OSX_DEPLOYMENT_TARGET=15.0',
        '-DCMAKE_SUPPRESS_REGENERATION=ON', *extra)
    run('cmake', '--build', build, '--config', 'Release', '--parallel', args.jobs,
        '--target', 'TurbulenceImport' if args.target == 'import-app' else 'gGXEE8P_recomp',
        '--', *([] if args.team else ['CODE_SIGNING_ALLOWED=NO']))
    print(f'Built {args.target} in {build}. Gameplay runtime integration is still required.')
    if args.ipa:
        package_ipa(build)

if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f'error: {error}', file=sys.stderr)
        sys.exit(1)
