#!/bin/bash
# Set limits before starting Python: custom Mach-O stacks can reject setrlimit.
set -euo pipefail
printf 'stack before (KiB): soft=%s hard=%s\n' "$(ulimit -S -s)" "$(ulimit -H -s)"
ulimit -S -s 1024
ulimit -H -s 1024
soft=$(ulimit -S -s)
hard=$(ulimit -H -s)
printf 'stack inherited (KiB): soft=%s hard=%s\n' "$soft" "$hard"
[[ "$soft" == 1024 && "$hard" == 1024 ]]
exec "$@"
