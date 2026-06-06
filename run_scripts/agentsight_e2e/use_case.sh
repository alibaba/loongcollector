#!/bin/bash
# Select an AgentSight E2E pipeline config and install it under .vscode/configs/.
# Usage: ./use_case.sh baseline|split|no-detail|split-no-detail
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
CASE="${1:-}"

usage() {
  echo "Usage: $0 <case>" >&2
  echo "  baseline         EventStreamFormat=false, MessageDeltaOnly=false" >&2
  echo "  split            EventStreamFormat=true,  MessageDeltaOnly=false" >&2
  echo "  no-detail        EventStreamFormat=false, MessageDeltaOnly=true" >&2
  echo "  split-no-detail  EventStreamFormat=true,  MessageDeltaOnly=true (defaults)" >&2
  exit 1
}

case "${CASE}" in
  baseline) SRC="${SCRIPT_DIR}/configs/##1.0##xiaotian-project\$agentsight-baseline.yaml" ;;
  split) SRC="${SCRIPT_DIR}/configs/##1.0##xiaotian-project\$agentsight-split.yaml" ;;
  no-detail) SRC="${SCRIPT_DIR}/configs/##1.0##xiaotian-project\$agentsight-no-detail.yaml" ;;
  split-no-detail) SRC="${SCRIPT_DIR}/configs/##1.0##xiaotian-project\$agentsight-split-no-detail.yaml" ;;
  *) usage ;;
esac

if [[ ! -f "${SRC}" ]]; then
  echo "error: missing config ${SRC}" >&2
  exit 1
fi

CONFIGS_DIR="${REPO_ROOT}/.vscode/configs"
ACTIVE="${CONFIGS_DIR}/##1.0##xiaotian-project\$agentsight.yaml"
WHOLEFILE="${CONFIGS_DIR}/##1.0##xiaotian-project\$wholefile-overwrite.yaml"
WHOLEFILE_BAK="${SCRIPT_DIR}/wholefile-overwrite.yaml.e2e-bak"

mkdir -p "${CONFIGS_DIR}"
cp -f "${SRC}" "${ACTIVE}"

if [[ -f "${WHOLEFILE}" ]]; then
  cp -f "${WHOLEFILE}" "${WHOLEFILE_BAK}"
  rm -f "${WHOLEFILE}"
  echo "Disabled wholefile config (backup outside configs: ${WHOLEFILE_BAK})"
fi

echo "Active agentsight config: case=${CASE}"
echo "  -> ${ACTIVE}"
grep -E 'EventStreamFormat|MessageDeltaOnly' "${ACTIVE}" || true
