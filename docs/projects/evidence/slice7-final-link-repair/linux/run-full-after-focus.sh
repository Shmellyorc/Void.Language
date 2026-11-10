#!/usr/bin/env bash
set -euo pipefail
ev="$PWD/docs/projects/evidence/slice7-final-link-repair/linux"
for p in release debug strict; do
    for attempt in $(seq 1 1800); do [[ ! -s "$ev/$p-focused-exit.txt" ]] || break; sleep 3; done
    [[ "$(cat "$ev/$p-focused-exit.txt")" == 0 ]]
done
for p in release debug strict; do
    python3 "$ev/isolate-full-tests.py" "$p" > "$ev/$p-inventory.json"
done
pids=()
for p in release debug strict; do
    bash "$ev/run-profile.sh" "$p" full &
    pids+=("$!")
done
result=0
for pid in "${pids[@]}"; do
    wait "$pid" || result=1
done
exit "$result"
