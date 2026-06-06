#!/bin/bash
# Restore wholefile pipeline config after AgentSight E2E.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
CONFIGS_DIR="${REPO_ROOT}/.vscode/configs"
WHOLEFILE="${CONFIGS_DIR}/##1.0##xiaotian-project\$wholefile-overwrite.yaml"
WHOLEFILE_BAK="${SCRIPT_DIR}/wholefile-overwrite.yaml.e2e-bak"

if [[ -f "${WHOLEFILE_BAK}" ]]; then
  mv -f "${WHOLEFILE_BAK}" "${WHOLEFILE}"
  echo "Restored ${WHOLEFILE}"
else
  echo "No wholefile backup to restore."
fi
