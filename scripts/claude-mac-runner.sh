#!/bin/bash
# Runs job scripts that Claude drops into build/claude-mac/queue, one at a time,
# on this Mac (adb, NDK builds). Output: build/claude-mac/done/<name>.log/.rc
# A job may set its time limit with a "# timeout=SECONDS" line (default 600).
# Start:  bash scripts/claude-mac-runner.sh     Stop: Ctrl-C
cd "$(dirname "$0")/.." || exit 1
SELF="$PWD/scripts/claude-mac-runner.sh"; STAMP=$(stat -f %m "$SELF")
Q=build/claude-mac; mkdir -p "$Q/queue" "$Q/done"
export ANDROID_HOME="$HOME/Documents/ChatGPT/Crazy Taxi Recompilation (ReFare)/build/android-sdk"
export PATH="$ANDROID_HOME/platform-tools:/opt/homebrew/bin:/usr/local/bin:$PATH"
echo "Claude runner watching $PWD/$Q/queue (Ctrl-C to stop)"
while true; do
  date +%s > "$Q/.alive"
  for f in "$Q"/queue/*.sh; do
    [ -e "$f" ] || continue
    n=$(basename "$f" .sh); mv "$f" "$Q/done/$n.sh"
    t=$(sed -n 's/^# timeout=\([0-9]*\).*/\1/p' "$Q/done/$n.sh" | head -1); t=${t:-600}
    echo "[$(date +%T)] $n (limit ${t}s)"
    perl -e '$SIG{ALRM}=sub{kill "TERM",-$$; exit 124}; setpgrp; alarm shift; system @ARGV; exit($?>>8)' \
      "$t" bash "$Q/done/$n.sh" < /dev/null > "$Q/done/$n.log" 2>&1
    echo $? > "$Q/done/$n.rc"
  done
  # Pick up a new version of this script without a manual restart.
  [ "$(stat -f %m "$SELF")" != "$STAMP" ] && { echo "runner updated, restarting"; exec bash "$SELF"; }
  sleep 1
done
