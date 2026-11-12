#!/bin/bash
# Runs inside a Fedora container (see scripts/flatpak.sh). $1 = x86_64|aarch64.
# The project is mounted read-only at /src; bundles go to /out.
set -euo pipefail
ARCH=$1
dnf -y -q install flatpak flatpak-builder python3 >/dev/null
flatpak remote-add --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
flatpak install -y --noninteractive --arch=$ARCH flathub org.freedesktop.Platform//25.08 org.freedesktop.Sdk//25.08
REL=$(ls -d /src/build/modules/GXEE8P/*-5f8c8e46802d6f00/dolrecomp-output/rel/generated | head -1)
mkdir -p /out /state/work
sed -e "s#@ROOT@#/src#g" -e "s#@ARCH@#$ARCH#g" -e "s#@REL@#$REL#g" /src/ports/linux/manifest.yml.in > /state/work/io.github.bittahbandit1.Turbulence.yml
# flatpak-builder needs its state dir and build dir on one filesystem.
cd /state/work
flatpak-builder --arch=$ARCH --disable-rofiles-fuse --ccache --force-clean --state-dir=/state/fb \
  --repo=/state/repo-$ARCH build-$ARCH io.github.bittahbandit1.Turbulence.yml
flatpak build-bundle --arch=$ARCH --runtime-repo=https://dl.flathub.org/repo/flathub.flatpakrepo \
  /state/repo-$ARCH /out/Turbulence-linux-$ARCH.flatpak io.github.bittahbandit1.Turbulence
ls -la /out
