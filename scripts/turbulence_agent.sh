#!/bin/bash
# Turbulence test runner: lets Claude rebuild, launch and close Sonic Riders
# on its own (adapted from ReFare's scripts/refare_agent.sh).
#
# Start it once and leave it running (Ctrl+C stops it):
#   caffeinate -dims scripts/turbulence_agent.sh
# caffeinate keeps the Mac and display awake: a sleeping screen stops the game
# from presenting frames, which would make test runs meaningless.
#
# It watches build/claude-jobs/ for a file named next.job. A job may contain
# only these lines (anything else rejects the whole job, so a job can't run
# any other command on this Mac):
#   seconds=N            run for N seconds after boot (10-900), then quit
#   env=NAME=VALUE       NAME must start with TURBULENCE_, MTL_ or DOLRECOMP_;
#                        VALUE plain characters only
#   build=1              build tools and the game module, but don't launch
# Every job rebuilds first (make does nothing extra when up to date). Runs use
# --automation-dir build/claude-auto so pad input, screenshots and a clean stop
# can be driven through files. The log goes to build/logs/job-<id>.log, any new
# macOS crash report is copied next to it, and the result goes to
# build/claude-jobs/<id>.done.
set -u
cd "$(dirname "$0")/.." || exit 1
ROOT=$PWD
GAME=Sonic-Riders-USA-En-Ja-Fr-De-Es-It
JOBS=$ROOT/build/claude-jobs
AUTO=build/claude-auto
USERDIR=build/user
CRASHES=$HOME/Library/Logs/DiagnosticReports
mkdir -p "$JOBS" "$ROOT/build/logs" "$ROOT/$USERDIR/Config"

LOCK=$JOBS/.agent-lock
if ! mkdir "$LOCK" 2>/dev/null; then
  old=$(cat "$LOCK/pid" 2>/dev/null)
  if [ -n "$old" ] && kill -0 "$old" 2>/dev/null; then
    echo "Another Turbulence test runner is already running (pid $old)."
    exit 1
  fi
  echo "Removing a stale lock left by an earlier runner."
  rm -rf "$LOCK" && mkdir "$LOCK" || exit 1
fi
echo $$ > "$LOCK/pid"
trap 'rm -rf "$LOCK"; echo; echo "Turbulence test runner stopped."; exit 0' INT TERM EXIT
date +%s > "$JOBS/.agent-alive"

echo "Turbulence test runner waiting for jobs in build/claude-jobs (Ctrl+C to stop)."
while true; do
  date +%s > "$JOBS/.agent-alive"
  if [ ! -f "$JOBS/next.job" ]; then sleep 2; continue; fi
  id=$(date +%Y%m%d-%H%M%S)
  mv "$JOBS/next.job" "$JOBS/$id.job"
  seconds=60; envs=(); bad=""; buildonly=""
  while IFS= read -r line || [ -n "$line" ]; do
    line=${line%$'\r'}
    case "$line" in
      ""|"#"*) ;;
      seconds=*)
        v=${line#seconds=}
        if [[ "$v" =~ ^[0-9]+$ ]] && [ "$v" -ge 10 ] && [ "$v" -le 900 ]; then seconds=$v
        else bad="bad seconds: $v"; fi ;;
      env=*)
        kv=${line#env=}
        if [[ "$kv" =~ ^(TURBULENCE_|MTL_|DOLRECOMP_)[A-Z0-9_]+=[A-Za-z0-9._/:,-]*$ ]]; then envs+=("$kv")
        else bad="rejected env line: $kv"; fi ;;
      build=1) buildonly=1 ;;
      *) bad="rejected line: $line" ;;
    esac
  done < "$JOBS/$id.job"
  if [ -n "$bad" ]; then
    echo "job $id rejected ($bad)" | tee "$JOBS/$id.done"
    continue
  fi

  log=$ROOT/build/logs/job-$id.log
  marker=$JOBS/.$id.start; touch "$marker"
  if [ -n "$buildonly" ]; then
    echo "job $id: build only"
    env "${envs[@]+"${envs[@]}"}" make recompile GAME=$GAME JOBS=8 > "$log" 2>&1; rc=$?
    printf 'exit=%s\nlog=%s\n' "$rc" "build/logs/job-$id.log" > "$JOBS/$id.done"
    rm -f "$marker"; echo "job $id finished (exit $rc)"
    continue
  fi
  echo "job $id: run ${seconds}s ${envs[*]:-}"
  rm -rf "$ROOT/$AUTO"
  cp -f "$ROOT/config/GCPadNew.ini" "$ROOT/$USERDIR/Config/GCPadNew.ini" 2>/dev/null
  start=$(date +%s)
  env "${envs[@]+"${envs[@]}"}" make run GAME=$GAME JOBS=8 \
    RUN_ARGS="--graphics Metal --user-dir $USERDIR --automation-dir $AUTO" > "$log" 2>&1 &
  pid=$!
  booted=""
  while kill -0 $pid 2>/dev/null; do
    if [ -z "$booted" ] && grep -q '^booted=1' "$ROOT/$AUTO/status.txt" 2>/dev/null; then
      booted=$(date +%s)
    fi
    if [ -n "$booted" ] && [ $(( $(date +%s) - booted )) -ge "$seconds" ]; then break; fi
    if [ $(( $(date +%s) - start )) -ge $(( seconds + 2400 )) ]; then break; fi
    date +%s > "$JOBS/.agent-alive"
    sleep 1
  done
  if kill -0 $pid 2>/dev/null; then
    mkdir -p "$ROOT/$AUTO/commands"
    printf 'command=stop\n' > "$ROOT/$AUTO/commands/zzzz-stop.tmp"
    mv "$ROOT/$AUTO/commands/zzzz-stop.tmp" "$ROOT/$AUTO/commands/zzzz-stop.txt"
    for _ in $(seq 1 20); do kill -0 $pid 2>/dev/null || break; sleep 1; done
    if kill -0 $pid 2>/dev/null; then
      pkill -f "lib/ModernGekko/build/.*moderngekko-run" 2>/dev/null
      sleep 2
    fi
  fi
  wait $pid 2>/dev/null; rc=$?
  crash=""
  if [ -d "$CRASHES" ]; then
    for f in $(find "$CRASHES" -name 'moderngekko-run*' -newer "$marker" 2>/dev/null); do
      cp "$f" "$ROOT/build/logs/job-$id-$(basename "$f")"; crash="build/logs/job-$id-$(basename "$f")"
    done
  fi
  rm -f "$marker"
  printf 'exit=%s\nbooted=%s\nlog=%s\ncrash=%s\n' "$rc" "${booted:+yes}" "build/logs/job-$id.log" "$crash" > "$JOBS/$id.done"
  echo "job $id finished (exit $rc)"
done
