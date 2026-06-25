#!/usr/bin/env bash
# Poll Epic comments every INTERVAL seconds; append actionable events to EVENTS_FILE.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/lib/common.sh"

epic_parse_global_args "$@"
set -- ${EPIC_ARGV[@]+"${EPIC_ARGV[@]}"}

if [[ ! -s "${STATE_FILE}" ]]; then
  "${SCRIPT_DIR}/poll-once.sh" --repo "${REPO}" --epic "${EPIC}" --init
else
  epic_log "poll-loop resumed with existing baseline ($(wc -l < "${STATE_FILE}") ids)"
fi
echo $$ > "${PID_FILE}"
epic_log "poll-loop started pid=$$ interval=${INTERVAL}s events=${EVENTS_FILE}"

while true; do
  "${SCRIPT_DIR}/poll-once.sh" --repo "${REPO}" --epic "${EPIC}" || epic_log "poll-once failed (continuing)"
  sleep "${INTERVAL}"
done
