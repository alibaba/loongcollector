#!/bin/bash
# Full deploy for one E2E case: select config -> restart.sh (sync output/) -> send_remote -> remote_restart
# Usage: ./deploy_case.sh baseline|split|no-detail|split-no-detail
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
CASE="${1:-}"

if [[ -z "${CASE}" ]]; then
  echo "Usage: $0 <case>" >&2
  exit 1
fi

"${SCRIPT_DIR}/use_case.sh" "${CASE}"
bash "${REPO_ROOT}/run_scripts/restart.sh"
bash "${REPO_ROOT}/run_scripts/send_remote.sh"
bash "${REPO_ROOT}/run_scripts/remote_restart.sh"

echo ""
echo "Deployed case=${CASE} -> ${REMOTE_USER:-root}@${REMOTE_HOST:-47.239.119.134}:${REMOTE_PATH:-/usr/local/loongcollector}"
echo "On remote OpenClaw, send one chat (note time). Then verify in SLS xiaotian-logstore."
echo "See: ${SCRIPT_DIR}/E2E_VERIFY.md"
