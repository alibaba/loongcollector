#!/usr/bin/env bash
# Optional manual E2E cleanup (not invoked by ./scripts/e2e.sh).
# Usage: bash skills/e2e/scripts/e2e-cleanup.sh [case_name]
set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
E2E_DIR="$REPO_ROOT/test/e2e"
CASE_NAME="${1:-}"

echo "==> Removing E2E loongcollector / compose sidecar containers..."
if command -v docker >/dev/null 2>&1; then
  for filter in loongcollectorC; do
    ids=$(docker ps -aq --filter "name=${filter}" 2>/dev/null || true)
    if [[ -n "${ids}" ]]; then
      # shellcheck disable=SC2086
      docker rm -f ${ids} 2>/dev/null || true
    fi
  done
  docker network prune -f 2>/dev/null || true
fi

echo "==> Cleaning runtime dirs under test/e2e..."
rm -rf "$E2E_DIR/config" "$E2E_DIR/onetime_pipeline_config"
rm -rf "$E2E_DIR/report" 2>/dev/null || true

if [[ -n "$CASE_NAME" ]]; then
  CASE_DIR="$E2E_DIR/test_cases/$CASE_NAME"
  if [[ -d "$CASE_DIR" ]]; then
    rm -f "$CASE_DIR/testcase-compose.yaml"
    rm -f "$CASE_DIR/otel-export/"*.json 2>/dev/null || true
  fi
else
  find "$E2E_DIR/test_cases" -name "testcase-compose.yaml" -delete 2>/dev/null || true
fi

echo "==> Done"
