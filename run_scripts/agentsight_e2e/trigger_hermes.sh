#!/bin/bash
# Send one Hermes agent chat turn on the remote host (for AgentSight E2E).
# Usage: ./trigger_hermes.sh [message]
# Optional: VERIFY_BASELINE_DELTA=1 runs verify_baseline_delta.sh after trigger (baseline only).
# Optional: HERMES_SESSION_ID=<id> passes --resume for session continuation (full session_id from prior run).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REMOTE_HOST="${REMOTE_HOST:-47.239.119.134}"
REMOTE_USER="${REMOTE_USER:-root}"
HERMES_BIN="${HERMES_BIN:-/usr/local/bin/hermes}"
HERMES_HOME="${HERMES_HOME:-/root/.hermes}"
TIMEOUT="${HERMES_TIMEOUT:-120}"
RESUME="${HERMES_SESSION_ID:-}"

MARKER="E2E_HERMES_$(date +%Y%m%d_%H%M%S)"
MSG="${1:-Please reply: OK}"
if [[ "${MSG}" != *"E2E_HERMES_"* ]]; then
  MSG="${MSG} [${MARKER}]"
else
  # Caller supplied a custom marker; extract for SLS verify.
  MARKER=$(echo "${MSG}" | grep -oE 'E2E_HERMES_[0-9_]+' | tail -1 || echo "${MARKER}")
fi

echo "Remote: ${REMOTE_USER}@${REMOTE_HOST}"
echo "HERMES_HOME: ${HERMES_HOME}"
echo "marker: ${MARKER}"
echo "message: ${MSG}"
if [[ -n "${RESUME}" ]]; then
  echo "resume session: ${RESUME}"
fi

RESUME_SHELL=""
if [[ -n "${RESUME}" ]]; then
  RESUME_SHELL="--resume $(printf '%q' "${RESUME}")"
fi

set +e
output=$(
  ssh -o BatchMode=yes "${REMOTE_USER}@${REMOTE_HOST}" \
    "export HERMES_HOME='${HERMES_HOME}'; timeout ${TIMEOUT} '${HERMES_BIN}' chat -Q -q $(printf '%q' "${MSG}") --yolo --accept-hooks ${RESUME_SHELL}" \
    2>&1
)
rc=$?
set -e

echo "${output}" | tail -40
if [[ "${rc}" -ne 0 ]]; then
  echo "warn: hermes chat exited ${rc}" >&2
fi

SESSION_ID=$(echo "${output}" | grep -E '^session_id:' | tail -1 | sed 's/^session_id:[[:space:]]*//' || true)
if [[ -n "${SESSION_ID}" ]]; then
  echo ""
  echo "session-id (Hermes CLI): ${SESSION_ID}"
  echo "  Use HERMES_SESSION_ID=${SESSION_ID} for --resume on the next turn."
fi

echo ""
echo "Check SLS xiaotian-logstore (last 10m):"
echo "  gen_ai.agent.type: hermes and ${MARKER}"
echo "marker for delta verify: ${MARKER}"

if [[ "${VERIFY_BASELINE_DELTA:-0}" == "1" ]]; then
  echo ""
  AGENT_TYPE=hermes bash "${SCRIPT_DIR}/verify_baseline_delta.sh" \
    --marker "${MARKER}" \
    --wait "${VERIFY_WAIT:-60}"
fi
