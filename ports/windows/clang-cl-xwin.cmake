# Cross-compile for Windows x64 from macOS/Linux with clang-cl + lld-link and
# the MSVC CRT / Windows SDK fetched by xwin:
#   xwin --accept-license --arch x86_64 splat --output ~/.xwin
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_VERSION 10.0)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

set(TURBULENCE_LLVM_BIN "/opt/homebrew/opt/llvm@20/bin" CACHE PATH "LLVM with clang-cl, llvm-lib, llvm-rc, llvm-mt")
set(TURBULENCE_LLD_LINK "/opt/homebrew/opt/lld/bin/lld-link" CACHE FILEPATH "lld-link")
set(XWIN "$ENV{HOME}/.xwin" CACHE PATH "xwin splat output")

set(CMAKE_C_COMPILER "${TURBULENCE_LLVM_BIN}/clang-cl")
set(CMAKE_CXX_COMPILER "${TURBULENCE_LLVM_BIN}/clang-cl")
set(CMAKE_LINKER "${TURBULENCE_LLD_LINK}")
set(CMAKE_AR "${TURBULENCE_LLVM_BIN}/llvm-lib")
set(CMAKE_RC_COMPILER "${TURBULENCE_LLVM_BIN}/llvm-rc")
set(CMAKE_MT "${TURBULENCE_LLVM_BIN}/llvm-mt")

set(_includes "")
foreach(dir crt/include sdk/include/ucrt sdk/include/um sdk/include/shared sdk/include/winrt sdk/include/cppwinrt)
  string(APPEND _includes " /imsvc \"${XWIN}/${dir}\"")
endforeach()
# x86-64-v2 baseline (SSE4.2). AES/SHA/PCLMUL only make those intrinsics
# available, as MSVC always does: Dolphin uses them after a runtime CPU check,
# and the compiler never emits them on its own.
set(_target "--target=x86_64-pc-windows-msvc -fms-compatibility-version=19.40 -march=x86-64-v2 -maes -msha -mpclmul")
set(CMAKE_C_FLAGS_INIT "${_target} ${_includes}")
set(CMAKE_CXX_FLAGS_INIT "${_target} ${_includes}")
set(CMAKE_RC_FLAGS_INIT "/I \"${XWIN}/sdk/include/um\" /I \"${XWIN}/sdk/include/shared\"")
set(_libs "/libpath:\"${XWIN}/crt/lib/x86_64\" /libpath:\"${XWIN}/sdk/lib/um/x86_64\" /libpath:\"${XWIN}/sdk/lib/ucrt/x86_64\"")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${_libs}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${_libs}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${_libs}")
# Static CRT: the build runs without the Visual C++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
set(CMAKE_TRY_COMPILE_CONFIGURATION Release)
set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)

# Never let host (Homebrew) packages into a Windows build.
set(CMAKE_IGNORE_PREFIX_PATH "/opt/homebrew;/usr/local")
set(CMAKE_SYSTEM_IGNORE_PREFIX_PATH "/opt/homebrew;/usr/local")
set(CMAKE_FIND_ROOT_PATH "${XWIN}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Keep Homebrew pkg-config packages (speexdsp etc.) out of the Windows build.
set(ENV{PKG_CONFIG_LIBDIR} "/nonexistent")
set(ENV{PKG_CONFIG_PATH} "")
