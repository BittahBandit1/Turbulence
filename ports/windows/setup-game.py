#!/usr/bin/env python3
"""First-run setup for the Windows launcher. All game files stay on this PC."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


PACKAGE = Path(__file__).resolve().parent
GAME_ID = "GXEE8P"
MODULE = "gGXEE8P_recomp.dll"


def run(args, **kwargs):
    print("+", subprocess.list2cmdline([str(arg) for arg in args]), flush=True)
    subprocess.run(args, check=True, **kwargs)


def compiler_environment(work):
    installer = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"))
    vswhere = installer / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    error = ("Install Visual Studio Build Tools with Desktop development with C++, "
             "C++ Clang tools for Windows, and C++ CMake tools for Windows.")
    if not vswhere.is_file():
        raise ValueError(error)
    installations = subprocess.check_output([
        vswhere, "-latest", "-products", "*", "-requires",
        "Microsoft.VisualStudio.Component.VC.Llvm.Clang",
        "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
        "-property", "installationPath", "-utf8",
    ], encoding="utf-8-sig").splitlines()
    if not installations:
        raise ValueError(error)
    visual_studio = Path(installations[0])
    clang = visual_studio / "VC/Tools/Llvm/x64/bin"
    integrations = visual_studio / "Common7/IDE/CommonExtensions/Microsoft/CMake"
    batch = work / "compiler-env.cmd"
    # Delayed expansion is off and the VS path is passed in the environment, so
    # percent signs or spaces in the installation path survive cmd parsing.
    batch.write_text('@echo off\ncall "%TURBULENCE_VCVARS%" >nul\n'
                     'if errorlevel 1 exit /b 1\nset\n', encoding="ascii")
    inherited = dict(os.environ)
    inherited["TURBULENCE_VCVARS"] = str(visual_studio / "VC/Auxiliary/Build/vcvars64.bat")
    shell = os.environ.get("COMSPEC", r"C:\Windows\System32\cmd.exe")
    # Run from the short build directory so cmd never has to parse its path.
    variables = subprocess.check_output(
        [shell, "/d", "/u", "/v:off", "/c", batch.name], cwd=work,
        env=inherited, encoding="utf-16-le")
    env = {}
    for line in variables.splitlines():
        key, sep, value = line.partition("=")
        if sep and key:
            env[key.upper()] = value
    env["PATH"] = os.pathsep.join(map(str, [
        clang, integrations / "CMake/bin", integrations / "Ninja", env.get("PATH", ""),
    ]))
    for tool in ("clang-cl.exe", "lld-link.exe", "cmake.exe", "ninja.exe"):
        if not shutil.which(tool, path=env["PATH"]):
            raise ValueError(f"{tool} was not found. {error}")
    env["CMAKE_NINJA_FORCE_RESPONSE_FILE"] = "1"
    return env


def recompile(game, work, env):
    recompiler = PACKAGE / "dolrecomp.exe"
    jobs = min(8, os.cpu_count() or 1)
    common = [recompiler, f"-j{jobs}", "--backend=c", "--cpu", "gekko", "--gamecube"]
    run([*common, game / "sys/main.dol", work / "dol"], cwd=work)
    run([*common, "--rel-base", "0x803FBD00", game / "files/_Main.rel", work / "rel"],
        cwd=work, env=dict(os.environ, DOLRECOMP_REL_BSS="0x805FB080"))

    generated = work / "dol/generated"
    rel_generated = work / "rel/generated"
    shutil.copyfile(game / "sys/main.dol", generated / "main.dol")
    for folder in (generated, rel_generated):
        (folder / "generated_smc.txt").touch(exist_ok=True)

    template = PACKAGE / "toolchain/module-template"
    build = work / "b"
    cmake = shutil.which("cmake.exe", path=env["PATH"])
    run([cmake, "-S", template, "-B", build, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
         f"-DCMAKE_C_COMPILER={shutil.which('clang-cl.exe', path=env['PATH'])}",
         f"-DCMAKE_LINKER={shutil.which('lld-link.exe', path=env['PATH'])}",
         "-DCMAKE_C_FLAGS=-march=x86-64-v2 -maes -msha -mpclmul",
         "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded", "-DCMAKE_POLICY_DEFAULT_CMP0091=NEW",
         f"-DGAME_ID={GAME_ID}", f"-DPython3_EXECUTABLE={sys.executable}",
         f"-DGENERATED_DIR={generated}", f"-DREL_GENERATED_DIR={rel_generated}"], env=env)
    run([cmake, "--build", build, "--parallel", str(jobs)], env=env)
    return build / MODULE


def setup(library, image):
    game = library / "game"
    library.mkdir(parents=True, exist_ok=True)
    package_id = (PACKAGE / "build-id.txt").read_text(encoding="ascii").strip()
    if len(package_id) != 64 or any(c not in "0123456789abcdef" for c in package_id):
        raise ValueError("Invalid launcher package. Extract a fresh copy of the ZIP.")

    with tempfile.TemporaryDirectory(prefix="build-", dir=library) as temp:
        work = Path(temp)
        env = compiler_environment(work)
        if not (game / "sys/main.dol").is_file():
            if image is None:
                raise ValueError("Choose a Sonic Riders USA revision 0 disc image first.")
            print("Importing your Sonic Riders disc image.", flush=True)
            staging = work / "game"
            run([sys.executable, PACKAGE / "extract-sonic-riders.py", image, staging])
        else:
            staging = game
        print("Building Sonic Riders. This can take several minutes.", flush=True)
        module = recompile(staging, work, env)
        destination = library / "modules" / package_id
        destination.mkdir(parents=True, exist_ok=True)
        if staging != game:
            staging.rename(game)
        module.replace(destination / MODULE)

    config = library / "user/Config"
    config.mkdir(parents=True, exist_ok=True)
    pad = config / "GCPadNew.ini"
    if not pad.exists():
        shutil.copyfile(PACKAGE / "GCPadNew.ini", pad)
    print("Ready to play.", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("library", type=Path)
    parser.add_argument("image", type=Path, nargs="?")
    args = parser.parse_args()
    try:
        setup(args.library, args.image)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        sys.exit(f"Setup failed: {error}")
