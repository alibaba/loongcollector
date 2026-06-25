#!/usr/bin/env bash
# Local wake: triage current Epic scope and print Desktop Agent prompt.
#
# Usage:
#   scripts/epic/wake-local.sh --repo owner/repo --epic <EPIC> [--pr <PR>] [--discussion <N>]
set -euo pipefail

REPO="${EPIC_REPO:-}"
PR=""
EPIC=""
DISCUSSION=""
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --repo) REPO="$2"; shift 2 ;;
    --pr) PR="$2"; shift 2 ;;
    --epic) EPIC="$2"; shift 2 ;;
    --discussion) DISCUSSION="$2"; shift 2 ;;
    -h|--help)
      echo "Usage: wake-local.sh --repo owner/repo --epic N [--pr N] [--discussion N]"
      exit 0
      ;;
    *) echo "unknown: $1" >&2; exit 1 ;;
  esac
done

[[ -n "$REPO" ]] || { echo "need --repo" >&2; exit 1; }
[[ -n "$EPIC" ]] || { echo "need --epic" >&2; exit 1; }

echo "=== Epic Delivery local wake ($REPO · Epic #$EPIC) ==="
[[ -n "$DISCUSSION" ]] && echo "Discussion #$DISCUSSION"
echo ""

if [[ -n "$PR" ]]; then
  bash "$SCRIPT_DIR/triage-pr-feedback.sh" --repo "$REPO" --pr "$PR" --latest-only || true
else
  bash "$SCRIPT_DIR/triage-pr-feedback.sh" --repo "$REPO" --epic "$EPIC" || true
fi

echo ""
echo "--- Paste into Cursor Desktop Agent ---"
if [[ -n "$PR" ]]; then
  cat <<EOF
按 .skill/skills/epic-delivery 处理 Epic #$EPIC / PR #$PR（repo $REPO${DISCUSSION:+, Discussion #$DISCUSSION}）。
只跟踪本 Epic 相关 PR/Issue，不要扫全仓。

1. bash scripts/epic/triage-pr-feedback.sh --repo $REPO --pr $PR --latest-only
2. 加载 SKILL.md 阶段 6；子 skill 暂从 .claude/skills/ 加载（compile 等）
3. Agent 回复须带 from=agent footer
EOF
else
  cat <<EOF
按 .skill/skills/epic-delivery 处理 Epic #$EPIC（repo $REPO${DISCUSSION:+, Discussion #$DISCUSSION}）下 open PR 的人工反馈。
只跟踪本 Epic 子 Issue 关联的 PR。

1. bash scripts/epic/triage-pr-feedback.sh --repo $REPO --epic $EPIC
2. 对每个 ACTION REQUIRED 的 PR 进入阶段 6
3. 派发新子 Issue 前检查 Blocked by，不得跳过依赖边（例：B2 须等 B1）
EOF
fi
