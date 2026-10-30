#!/bin/bash
# Export Turbulence's changes inside the submodules to patches/ so they live in
# this repo's history. Run after changing anything under lib/.
set -eu
cd "$(dirname "$0")/.."
export_diff() {  # <submodule dir> <patch name>
  ( cd "$1" && git ls-files --others --exclude-standard -z | xargs -0 -r git add -N -- \
      && git diff --ignore-submodules=all ) > "patches/$2.patch"
  echo "patches/$2.patch: $(grep -c '^diff --git' "patches/$2.patch") files"
}
mkdir -p patches
export_diff lib/ModernGekko moderngekko
export_diff lib/ModernGekko/vendor/dolphin dolphin
export_diff lib/ModernGekko/vendor/dolphin/DolRecomp dolrecomp
{
  echo "moderngekko $(git -C lib/ModernGekko rev-parse HEAD)"
  echo "dolphin $(git -C lib/ModernGekko/vendor/dolphin rev-parse HEAD)"
  echo "dolrecomp $(git -C lib/ModernGekko/vendor/dolphin/DolRecomp rev-parse HEAD)"
} > patches/base-commits.txt
