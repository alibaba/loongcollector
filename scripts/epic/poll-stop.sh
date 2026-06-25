#!/usr/bin/env bash
# Stop poll-loop for the given epic.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/lib/common.sh"

epic_parse_global_args "$@"
set -- ${EPIC_ARGV[@]+"${EPIC_ARGV[@]}"}

if ! epic_poll_loop_running; then
  echo "poll-loop not running for epic #${EPIC}"
  exit 0
fi

pid="$(cat "${PID_FILE}")"
kill "${pid}" 2>/dev/null || true
rm -f "${PID_FILE}"
echo "poll-loop stopped for epic #${EPIC} (was pid ${pid})"
