#!/usr/bin/env bash
# Local wake helper when Cloud Agents / Automations are unavailable.
# Polls epic PRs for pending human feedback and prints a ready-to-paste Agent prompt.
#
# Usage:
#   scripts/epic/wake-local.sh --repo alibaba/loongcollector --epic 2595
#   scripts/epic/wake-local.sh --repo alibaba/loongcollector --pr 2619
#   watch -n 300 scripts/epic/wake-local.sh --repo alibaba/loongcollector --epic 2595
set -euo pipefail

REPO="${EPIC_REPO:-alibaba/loongcollector}"
PR=""
EPIC=""
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --repo) REPO="$2"; shift 2 ;;
    --pr) PR="$2"; shift 2 ;;
    --epic) EPIC="$2"; shift 2 ;;
    -h|--help)
      echo "Usage: wake-local.sh --repo owner/repo (--pr N | --epic N)"
      exit 0
      ;;
    *) echo "unknown: $1" >&2; exit 1 ;;
  esac
done

[[ -n "$PR" || -n "$EPIC" ]] || { echo "need --pr or --epic" >&2; exit 1; }

echo "=== Epic Delivery local wake ($REPO) ==="
echo ""

if [[ -n "$PR" ]]; then
  bash "$SCRIPT_DIR/triage-pr-feedback.sh" --repo "$REPO" --pr "$PR" --latest-only || true
  pending=$?
else
  bash "$SCRIPT_DIR/triage-pr-feedback.sh" --repo "$REPO" --epic "$EPIC" || true
  pending=0
fi

echo ""
echo "--- Paste into Cursor Desktop Agent (local) ---"
if [[ -n "$PR" ]]; then
  cat <<EOF
按 .skill/skills/epic-delivery 处理 PR #$PR（repo $REPO）上最新的人工评论。
1. bash scripts/epic/triage-pr-feedback.sh --repo $REPO --pr $PR
2. 加载 SKILL.md 阶段 6；C++ 验收遵循 .claude/skills/compile/SKILL.md（make core，禁止写「待 CI」）
3. Agent 回复须带 from=agent footer
EOF
else
  cat <<EOF
按 .skill/skills/epic-delivery 处理 Epic #$EPIC（repo $REPO）下 open PR 的人工反馈。
1. bash scripts/epic/triage-pr-feedback.sh --repo $REPO --epic $EPIC
2. 对每个 ACTION REQUIRED 的 PR 进入阶段 6
EOF
fi

echo ""
echo "Tip: cron 示例 — */15 * * * * cd /path/to/loongcollector && scripts/epic/wake-local.sh --repo $REPO --epic ${EPIC:-2595}"
