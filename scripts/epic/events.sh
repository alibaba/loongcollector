#!/usr/bin/env bash
# Show pending (unprocessed) events and optionally mark handled.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/lib/common.sh"

epic_parse_global_args "$@"
set -- ${EPIC_ARGV[@]+"${EPIC_ARGV[@]}"}

HANDLED_FILE="${STATE_DIR}/handled-ids.txt"
touch "${HANDLED_FILE}"

ACTION=list
while [[ $# -gt 0 ]]; do
  case "$1" in
    list) ACTION=list; shift ;;
    mark-handled)
      ACTION=mark
      MARK_ID="$2"
      shift 2
      ;;
    pending-count)
      ACTION=count
      shift
      ;;
    *) epic_die "usage: events.sh --epic <n> [--repo owner/repo] [list|pending-count|mark-handled <comment_id>]" ;;
  esac
done

pending_lines() {
  while IFS= read -r line; do
    [[ -z "${line}" ]] && continue
    id="$(python3 -c 'import json,sys; print(json.loads(sys.argv[1])["comment_id"])' "${line}")"
    if ! grep -Fxq "${id}" "${HANDLED_FILE}" 2>/dev/null; then
      echo "${line}"
    fi
  done < "${EVENTS_FILE}"
}

case "${ACTION}" in
  list) pending_lines ;;
  count) pending_lines | wc -l ;;
  mark)
    echo "${MARK_ID}" >> "${HANDLED_FILE}"
    epic_log "marked handled ${MARK_ID}"
    ;;
esac
