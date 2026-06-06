#!/bin/bash
# Baseline E2E: deploy (optional) -> agent trigger -> delta formula verify.
# Usage:
#   ./run_baseline_e2e.sh              # trigger + verify only (assumes baseline already deployed)
#   ./run_baseline_e2e.sh --deploy     # deploy baseline case first
#   ./run_baseline_e2e.sh --tool       # use tool E2E (both rounds) instead of simple chat
#   AGENT_TYPE=hermes ./run_baseline_e2e.sh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

DEPLOY=0
TOOL=0
AGENT_TYPE="${AGENT_TYPE:-openclaw}"
MSG="E2E baseline delta verify $(date +%Y%m%d_%H%M%S). Reply: BASELINE_OK"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --deploy) DEPLOY=1; shift ;;
    --tool) TOOL=1; shift ;;
    --agent-type) AGENT_TYPE="${2:-openclaw}"; shift 2 ;;
    -h|--help)
      echo "Usage: $0 [--deploy] [--tool] [--agent-type openclaw|hermes] [message]" >&2
      exit 0
      ;;
    *) MSG="$1"; shift ;;
  esac
done

if [[ "${DEPLOY}" -eq 1 ]]; then
  bash "${SCRIPT_DIR}/deploy_case.sh" baseline
fi

export VERIFY_BASELINE_DELTA=1
export VERIFY_WAIT="${VERIFY_WAIT:-90}"
export AGENT_TYPE

if [[ "${TOOL}" -eq 1 ]]; then
  if [[ "${AGENT_TYPE}" != "openclaw" ]]; then
    echo "error: --tool E2E is only supported for AGENT_TYPE=openclaw" >&2
    exit 1
  fi
  bash "${SCRIPT_DIR}/trigger_openclaw_tool_e2e.sh" both
elif [[ "${AGENT_TYPE}" == "hermes" ]]; then
  bash "${SCRIPT_DIR}/trigger_hermes.sh" "${MSG}"
else
  export OPENCLAW_SESSION_ID="${OPENCLAW_SESSION_ID:-e2e-baseline-$(date +%s)}"
  bash "${SCRIPT_DIR}/trigger_openclaw.sh" "${MSG}"
fi
