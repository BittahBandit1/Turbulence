Turbulence for Windows x64
=========================

1. Extract the entire ZIP and open Turbulence.exe.
2. Choose or drop your Sonic Riders USA revision 0 ISO, GCM or GameCube CISO.
3. Let Turbulence finish the first build, then click Play.

First-time setup requires Python 3 and Visual Studio Build Tools with the
Desktop development with C++ workload. In the Visual Studio Installer,
include C++ Clang tools for Windows, C++ CMake tools for Windows and a
Windows SDK. Install Python with its launcher (py.exe) or Add Python to PATH.
Close and reopen Turbulence after installing prerequisites.

Python: https://www.python.org/downloads/windows/
Visual Studio Build Tools: https://visualstudio.microsoft.com/downloads/

The first build can take several minutes. Use Open Log for progress or
errors. If the compiler is missing, install it and click Build Game.
Future launches reuse the local build. A launcher update may ask you to
rebuild from the imported files, so keep the local library.

The game, compiled module, settings, saves and current log are in
%LOCALAPPDATA%\Turbulence. The disc image and game code stay on this PC.
The ZIP contains no original disc image, game assets or compiled game module.

Requires Windows 10 or later on an x64 CPU with SSE4.2. Vulkan is the
default graphics backend. The game also offers Direct3D 11 in Recomp Settings.
