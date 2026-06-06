#!/bin/bash
# E2E: trigger OpenClaw with an explicit tool call (read) for AgentSight field coverage.
# Usage: ./trigger_openclaw_tool_e2e.sh [round]
#   round=1 (default): create probe file + read tool
#   round=2: second turn on same session (append to session file)
# Optional: VERIFY_BASELINE_DELTA=1 runs verify_baseline_delta.sh after trigger (baseline only).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REMOTE_HOST="${REMOTE_HOST:-47.239.119.134}"
REMOTE_USER="${REMOTE_USER:-root}"
OPENCLAW_BIN="${OPENCLAW_BIN:-/root/.hermes/node/bin/openclaw}"
ROUND="${1:-1}"
PROBE_DIR="/root/.openclaw/etl-probe"
PROBE_FILE="${PROBE_DIR}/e2e-agentsight-tool-probe.txt"
SESSION_FILE="/tmp/e2e-agentsight-openclaw-session.id"

setup_probe() {
  ssh -o BatchMode=yes "${REMOTE_USER}@${REMOTE_HOST}" "mkdir -p '${PROBE_DIR}' && echo 'agentsight-e2e-tool-probe-$(date -Iseconds)' > '${PROBE_FILE}' && cat '${PROBE_FILE}'"
}

run_round1() {
  local sid="e2e-tools-$(date +%s)"
  echo "${sid}" > /tmp/local-e2e-session.id
  ssh -o BatchMode=yes "${REMOTE_USER}@${REMOTE_HOST}" "echo '${sid}' > '${SESSION_FILE}'"
  local msg
  msg=$(cat <<EOF
E2E agentsight tool test round1. You MUST call the read tool exactly once to read file path ${PROBE_FILE}, then reply with one line: TOOL_OK=<first 40 chars of file content>. Do not skip the tool call.
EOF
)
  echo "=== Round 1 session=${sid} ==="
  ssh -o BatchMode=yes "${REMOTE_USER}@${REMOTE_HOST}" \
    "timeout 180 '${OPENCLAW_BIN}' agent --session-id '${sid}' -m $(printf '%q' "${msg}") --agent main --timeout 120 --json" \
    | tail -50
}

run_round2() {
  local sid
  sid=$(ssh -o BatchMode=yes "${REMOTE_USER}@${REMOTE_HOST}" "cat '${SESSION_FILE}' 2>/dev/null" || cat /tmp/local-e2e-session.id 2>/dev/null || true)
  if [[ -z "${sid}" ]]; then
    echo "error: no session id; run round 1 first" >&2
    exit 1
  fi
  local msg
  msg=$(cat <<EOF
E2E agentsight tool test round2. You MUST call the read tool once on ${PROBE_FILE} again, then use write or edit tool to append the word ROUND2 to ${PROBE_FILE}. Reply TOOL_ROUND2=done when finished.
EOF
)
  echo "=== Round 2 session=${sid} ==="
  ssh -o BatchMode=yes "${REMOTE_USER}@${REMOTE_HOST}" \
    "timeout 240 '${OPENCLAW_BIN}' agent --session-id '${sid}' -m $(printf '%q' "${msg}") --agent main --timeout 180 --json" \
    | tail -60
}

echo "Setup probe file on remote..."
setup_probe

case "${ROUND}" in
  1) run_round1 ;;
  2) run_round2 ;;
  both)
    run_round1
    echo ""
    sleep 3
    run_round2
    ;;
  *) echo "Usage: $0 [1|2|both]" >&2; exit 1 ;;
esac

echo ""
echo "SLS check (xiaotian-logstore, last 15m):"
echo "  gen_ai.tool.definitions not empty"
echo "  gen_ai.output.messages contains tool_call / read / write"
echo "  gen_ai.input.messages on first round or when prefix changes (MessageDeltaOnly=false + dedup)"

if [[ "${VERIFY_BASELINE_DELTA:-0}" == "1" ]]; then
  local_sid=""
  if [[ -f /tmp/local-e2e-session.id ]]; then
    local_sid=$(cat /tmp/local-e2e-session.id)
  fi
  if [[ -n "${local_sid}" ]]; then
    echo ""
    bash "${SCRIPT_DIR}/verify_baseline_delta.sh" \
      --session-id "${local_sid}" \
      --wait "${VERIFY_WAIT:-90}"
  else
    echo "warn: VERIFY_BASELINE_DELTA=1 but no session id in /tmp/local-e2e-session.id" >&2
  fi
fi
