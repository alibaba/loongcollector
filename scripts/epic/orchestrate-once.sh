#!/usr/bin/env bash
# Orchestrator: read pending comment events and print dispatch hints for sub-agents.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/lib/common.sh"

epic_parse_global_args "$@"
set -- ${EPIC_ARGV[@]+"${EPIC_ARGV[@]}"}

HANDLED_FILE="${STATE_DIR}/handled-ids.txt"
touch "${HANDLED_FILE}"

count=0
while IFS= read -r line; do
  [[ -z "${line}" ]] && continue
  id="$(python3 -c 'import json,sys; print(json.loads(sys.argv[1])["comment_id"])' "${line}")"
  if grep -Fxq "${id}" "${HANDLED_FILE}" 2>/dev/null; then
    continue
  fi
  count=$((count + 1))
  python3 - "${line}" "${REPO}" "${EPIC}" <<'PY'
import json, sys
e = json.loads(sys.argv[1])
repo, epic = sys.argv[2], sys.argv[3]
kind, target = e["kind"], e["target"]
if kind == "issue":
    print("DISPATCH issue #{} AddressFeedback (epic #{})".format(target, epic))
    print("  hint: gh issue view {} --repo {}".format(target, repo))
elif kind in ("pr", "review"):
    print("DISPATCH PR #{} AddressFeedback (epic #{})".format(target, epic))
    print("  hint: gh pr view {} --repo {}".format(target, repo))
print("  url: {}".format(e["url"]))
print("  author: {}".format(e["author"]))
print("  preview: {}".format(e.get("preview", "")[:120]))
print("  mark: ./scripts/epic/events.sh --epic {} mark-handled {}".format(epic, e["comment_id"]))
print("")
PY
done < "${EVENTS_FILE}"

if [[ "${count}" -eq 0 ]]; then
  echo "NO_PENDING_EVENTS epic=#${EPIC}"
else
  echo "PENDING_COUNT=${count} epic=#${EPIC}"
fi
