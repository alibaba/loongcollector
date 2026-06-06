#!/bin/bash
# Send one OpenClaw agent turn on the remote host (for AgentSight E2E).
# Usage: ./trigger_openclaw.sh [message]
# Optional: VERIFY_BASELINE_DELTA=1 runs verify_baseline_delta.sh after trigger (baseline only).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REMOTE_HOST="${REMOTE_HOST:-47.239.119.134}"
REMOTE_USER="${REMOTE_USER:-root}"
OPENCLAW_BIN="${OPENCLAW_BIN:-/root/.hermes/node/bin/openclaw}"
MSG="${1:-Please reply: OK}"
SID="${OPENCLAW_SESSION_ID:-e2e-baseline-$(date +%s)}"
TIMEOUT="${OPENCLAW_TIMEOUT:-90}"

echo "Remote: ${REMOTE_USER}@${REMOTE_HOST}"
echo "session-id: ${SID}"
echo "message: ${MSG}"

ssh -o BatchMode=yes "${REMOTE_USER}@${REMOTE_HOST}" \
  "timeout ${TIMEOUT} '${OPENCLAW_BIN}' agent --session-id '${SID}' -m '${MSG}' --agent main --timeout 60 --json" \
  | tail -40

echo ""
echo "Check SLS xiaotian-logstore (last 10m), e.g. gen_ai.response.id or gen_ai.output.messages containing OK"
echo "session-id for delta verify: ${SID}"

if [[ "${VERIFY_BASELINE_DELTA:-0}" == "1" ]]; then
  echo ""
  VERIFY_ARGS=(--wait "${VERIFY_WAIT:-60}" --agent-type openclaw)
  [[ -n "${SID}" ]] && VERIFY_ARGS+=(--session-id "${SID}")
  if [[ "${MSG}" == *"BASELINE_OK"* ]]; then
    VERIFY_ARGS+=(--marker BASELINE_OK)
  fi
  bash "${SCRIPT_DIR}/verify_baseline_delta.sh" "${VERIFY_ARGS[@]}"
fi
