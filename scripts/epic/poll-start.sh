#!/usr/bin/env bash
# Start poll-loop in background if not already running for this epic.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/lib/common.sh"

epic_parse_global_args "$@"
set -- ${EPIC_ARGV[@]+"${EPIC_ARGV[@]}"}

if epic_poll_loop_running; then
  echo "poll-loop already running for epic #${EPIC} (pid $(cat "${PID_FILE}"))"
  exit 0
fi

nohup "${SCRIPT_DIR}/poll-loop.sh" --repo "${REPO}" --epic "${EPIC}" \
  >> "${STATE_DIR}/poll-loop.stdout" 2>&1 &
disown
sleep 1
if epic_poll_loop_running; then
  echo "poll-loop started for epic #${EPIC} (pid $(cat "${PID_FILE}"))"
  echo "state: ${STATE_DIR}"
else
  epic_die "failed to start poll-loop; see ${STATE_DIR}/poll-loop.stdout"
fi
