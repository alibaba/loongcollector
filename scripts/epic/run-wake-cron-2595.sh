#!/usr/bin/env bash
# Cron wrapper: triage Epic #2595; auto-invoke Cursor when new human comments appear.
set -euo pipefail

REPO_ROOT="/apsara/workspace/loongcollector"
LOG="${EPIC_WAKE_LOG:-/tmp/epic-2595-wake.log}"
LOCK="/tmp/epic-2595-wake.lock"
ENV_FILE="${EPIC_WAKE_ENV:-$REPO_ROOT/scripts/epic/epic-wake.env}"

# Optional: scripts/epic/epic-wake.env with CURSOR_API_KEY=...
if [[ -f "$ENV_FILE" ]]; then
  # shellcheck disable=SC1090
  source "$ENV_FILE"
fi

exec 9>"$LOCK"
if ! flock -n 9; then
  echo "$(date -Iseconds) skip: previous wake still running" >>"$LOG"
  exit 0
fi

{
  echo "===== $(date -Iseconds) ====="
  bash "$REPO_ROOT/scripts/epic/wake-local.sh" \
    --repo alibaba/loongcollector \
    --epic 2595 \
    --discussion 1928 \
    --auto
  echo
} >>"$LOG" 2>&1
