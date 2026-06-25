#!/usr/bin/env bash
# One-shot poll: emit NEW actionable comments as JSON lines; update seen-ids.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/lib/common.sh"

INIT=false
epic_parse_global_args "$@"
set -- ${EPIC_ARGV[@]+"${EPIC_ARGV[@]}"}
while [[ $# -gt 0 ]]; do
  case "$1" in
    --init) INIT=true; shift ;;
    *) epic_die "unknown arg: $1" ;;
  esac
done

new_count=0
while IFS= read -r row; do
  [[ -z "${row}" ]] && continue
  id="$(python3 -c 'import json,sys; print(json.loads(sys.argv[1])["id"])' "${row}")"
  body="$(python3 -c 'import json,sys; print(json.loads(sys.argv[1])["body"])' "${row}")"
  if epic_seen_id "${id}"; then
    continue
  fi
  epic_mark_seen "${id}"
  if [[ "${INIT}" == true ]]; then
    continue
  fi
  if ! epic_is_actionable_body "${body}"; then
    epic_log "skip agent info id=${id}"
    continue
  fi
  event="$(python3 -c 'import json,sys; r=json.loads(sys.argv[1]); print(json.dumps({k:r[k] for k in ("kind","target","id","author","created_at","url")} | {"comment_id": r["id"], "preview": r["body"].replace(chr(10)," ")[:200], "action":"AddressFeedback", "epic": int(sys.argv[2])}, ensure_ascii=False))' "${row}" "${EPIC}")"
  echo "${event}"
  echo "${event}" >> "${EVENTS_FILE}"
  epic_log "NEW event id=${id}"
  new_count=$((new_count + 1))
done < <(epic_scan_all_comments)

if [[ "${INIT}" == true ]]; then
  tracked="$(wc -l < "${STATE_FILE}" | tr -d ' ')"
  epic_log "init baseline: ${tracked} comment ids tracked"
  echo "{\"init\":true,\"epic\":${EPIC},\"tracked\":${tracked}}"
else
  echo "{\"poll\":true,\"epic\":${EPIC},\"new_events\":${new_count}}" >&2
fi
