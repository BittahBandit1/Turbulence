#!/bin/bash
# Build the Linux Flatpak for x86_64 or aarch64 in a container (Docker/Colima;
# x86_64 programs run through Rosetta on Apple Silicon). Needs the LLVM objects in
# build/llvm-linux-<arch> (see docs/STATUS.md) and the C REL output.
# Usage: scripts/flatpak.sh x86_64|aarch64   Output: build/release/
set -euo pipefail
cd "$(dirname "$0")/.."
ARCH=${1:?x86_64 or aarch64}
case $ARCH in x86_64|aarch64) ;; *) echo "bad arch" >&2; exit 2;; esac
# Run flatpak-builder natively on the host arch and let it build for $ARCH:
# an x86_64 bwrap under Rosetta cannot install its seccomp filter, but x86_64
# programs started from a native bwrap run fine (Rosetta binfmt, native syscalls).
case $(uname -m) in arm64|aarch64) PLATFORM=linux/arm64;; *) PLATFORM=linux/amd64;; esac
mkdir -p build/release
docker run --rm --privileged --platform $PLATFORM \
  -v "$PWD":/src:ro -v "$PWD/build/release":/out \
  -v turbulence-flatpak-$ARCH:/state -v turbulence-flatpak-sys-$ARCH:/var/lib/flatpak \
  fedora:41 bash /src/ports/linux/container-build.sh $ARCH
