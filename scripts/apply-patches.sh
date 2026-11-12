#!/bin/bash
# Apply patches/ to freshly checked-out submodules (see save-patches.sh).
set -eu
cd "$(dirname "$0")/.."
apply() {  # <submodule dir> <patch name>
  if [ -s "patches/$2.patch" ]; then
    if git -C "$1" apply --reverse --check "$PWD/patches/$2.patch" 2>/dev/null; then
      echo "already applied $2"
    else
      git -C "$1" apply --whitespace=nowarn "$PWD/patches/$2.patch"
      echo "applied $2"
    fi
  fi
}
apply lib/ModernGekko moderngekko
apply lib/ModernGekko/vendor/dolphin dolphin
apply lib/ModernGekko/vendor/dolphin/DolRecomp dolrecomp

apply lib/ModernGekko/vendor/dolphin/Externals/watcher/watcher watcher-mobile
apply lib/ModernGekko/vendor/dolphin/Externals/libspng/libspng libspng-windows
