#!/bin/bash
# Run baseline-only delta formula verification against SLS xiaotian-logstore.
# Usage:
#   ./verify_baseline_delta.sh --session-id e2e-baseline-1717654321
#   ./verify_baseline_delta.sh --session-id "$SID" --wait 60
#   ./verify_baseline_delta.sh --marker TOOL_OK --minutes 20
#   AGENT_TYPE=hermes ./verify_baseline_delta.sh --marker E2E_HERMES_20260606_110133
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
PY="${SCRIPT_DIR}/verify_delta_baseline.py"

WAIT_SEC="${VERIFY_WAIT:-0}"
SESSION_ID=""
MARKER=""
MINUTES="${VERIFY_MINUTES:-15}"
AGENT_TYPE="${AGENT_TYPE:-openclaw}"
EXTRA=()

usage() {
  echo "Usage: $0 --session-id <id> [--wait <sec>] [--minutes <n>] [--marker <token>] [--agent-type <type>]" >&2
  echo "  Or:  $0 --marker <token> [--minutes <n>] [--agent-type hermes|openclaw]" >&2
  echo "  AGENT_TYPE env (default openclaw) selects gen_ai.agent.type filter." >&2
  exit 1
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --session-id) SESSION_ID="${2:-}"; shift 2 ;;
    --marker) MARKER="${2:-}"; shift 2 ;;
    --agent-type) AGENT_TYPE="${2:-}"; shift 2 ;;
    --wait) WAIT_SEC="${2:-0}"; shift 2 ;;
    --minutes) MINUTES="${2:-15}"; shift 2 ;;
    -h|--help) usage ;;
    *) EXTRA+=("$1"); shift ;;
  esac
done

if [[ -z "${SESSION_ID}" && -z "${MARKER}" ]]; then
  usage
fi

if [[ ! -f "${PY}" ]]; then
  echo "error: missing ${PY}" >&2
  exit 1
fi

if [[ "${WAIT_SEC}" -gt 0 ]]; then
  echo "Waiting ${WAIT_SEC}s for SLS ingest..."
  sleep "${WAIT_SEC}"
fi

CFG="${LOONGCOLLECTOR_CONFIG:-${REPO_ROOT}/.vscode/loongcollector_config.json}"
ARGS=(--minutes "${MINUTES}" --config "${CFG}" --agent-type "${AGENT_TYPE}")
[[ -n "${SESSION_ID}" ]] && ARGS+=(--session-id "${SESSION_ID}")
[[ -n "${MARKER}" ]] && ARGS+=(--marker "${MARKER}")
ARGS+=("${EXTRA[@]}")

exec python3 "${PY}" "${ARGS[@]}"
