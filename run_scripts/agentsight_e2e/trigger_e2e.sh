#!/bin/bash
# Dispatch AgentSight E2E trigger by AGENT_TYPE (openclaw | hermes).
# Usage:
#   AGENT_TYPE=openclaw bash trigger_e2e.sh [message]     # default
#   AGENT_TYPE=hermes  bash trigger_e2e.sh "your message"
#   VERIFY_BASELINE_DELTA=1 AGENT_TYPE=hermes bash trigger_e2e.sh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
AGENT_TYPE="${AGENT_TYPE:-openclaw}"

case "${AGENT_TYPE}" in
  openclaw)
    exec bash "${SCRIPT_DIR}/trigger_openclaw.sh" "$@"
    ;;
  hermes)
    exec bash "${SCRIPT_DIR}/trigger_hermes.sh" "$@"
    ;;
  *)
    echo "error: unknown AGENT_TYPE=${AGENT_TYPE} (expected openclaw or hermes)" >&2
    exit 1
    ;;
esac
