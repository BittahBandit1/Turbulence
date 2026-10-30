#!/bin/bash
# Apply patches/ to freshly checked-out submodules (see save-patches.sh).
set -eu
cd "$(dirname "$0")/.."
apply() {  # <submodule dir> <patch name>
  if [ -s "patches/$2.patch" ]; then
    git -C "$1" apply --whitespace=nowarn "$PWD/patches/$2.patch" && echo "applied $2"
  fi
}
apply lib/ModernGekko moderngekko
apply lib/ModernGekko/vendor/dolphin dolphin
apply lib/ModernGekko/vendor/dolphin/DolRecomp dolrecomp
