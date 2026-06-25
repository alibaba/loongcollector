#!/usr/bin/env bash
# Local wake: triage Epic scope; optionally auto-invoke Cursor Agent.
#
# Usage:
#   wake-local.sh --repo owner/repo --epic <EPIC> [--discussion N] [--auto]
#   wake-local.sh --repo owner/repo --epic <EPIC> --pr <PR> [--auto]
set -euo pipefail

REPO="${EPIC_REPO:-}"
PR=""
EPIC=""
DISCUSSION=""
AUTO=false
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STATE_DIR="${EPIC_WAKE_STATE_DIR:-/tmp/epic-wake-state}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --repo) REPO="$2"; shift 2 ;;
    --pr) PR="$2"; shift 2 ;;
    --epic) EPIC="$2"; shift 2 ;;
    --discussion) DISCUSSION="$2"; shift 2 ;;
    --auto) AUTO=true; shift ;;
    -h|--help)
      echo "Usage: wake-local.sh --repo owner/repo --epic N [--pr N] [--discussion N] [--auto]"
      exit 0
      ;;
    *) echo "unknown: $1" >&2; exit 1 ;;
  esac
done

[[ -n "$REPO" ]] || { echo "need --repo" >&2; exit 1; }
[[ -n "$EPIC" ]] || { echo "need --epic" >&2; exit 1; }

mkdir -p "$STATE_DIR"
DISPATCH_STATE="${STATE_DIR}/epic-${EPIC}-dispatched.tsv"

echo "=== Epic Delivery local wake ($REPO · Epic #$EPIC) ==="
[[ -n "$DISCUSSION" ]] && echo "Discussion #$DISCUSSION"
$AUTO && echo "Mode: auto (Cursor Agent)"
echo ""

# Returns 0 if latest comment on PR needs action
pr_latest_needs_action() {
  local pr_num="$1"
  local count i body verdict created cid
  count="$(gh pr view "$pr_num" --repo "$REPO" --json comments -q '.comments | length')"
  [[ "$count" -gt 0 ]] || return 1
  i=$((count - 1))
  body="$(gh pr view "$pr_num" --repo "$REPO" --json comments -q ".comments[$i].body")"
  created="$(gh pr view "$pr_num" --repo "$REPO" --json comments -q ".comments[$i].createdAt")"
  cid="$(gh pr view "$pr_num" --repo "$REPO" --json comments -q ".comments[$i].id")"
  # shellcheck source=scripts/epic/lib/triage-classify.sh
  source "$SCRIPT_DIR/lib/triage-classify.sh"
  verdict="$(classify_comment_body "$body")"
  [[ "$verdict" == "required" ]] || return 1
  echo "${created} ${cid}"
}

already_dispatched() {
  local pr_num="$1" comment_id="$2"
  [[ -f "$DISPATCH_STATE" ]] || return 1
  grep -q "^${pr_num}[[:space:]]${comment_id}[[:space:]]" "$DISPATCH_STATE"
}

mark_dispatched() {
  local pr_num="$1" comment_id="$2"
  echo -e "${pr_num}\t${comment_id}\t$(date -Iseconds)" >>"$DISPATCH_STATE"
}

list_epic_prs() {
  bash "$SCRIPT_DIR/triage-pr-feedback.sh" --repo "$REPO" --epic "$EPIC" >/dev/null 2>&1 || true
  # Re-use epic open PR discovery via gh
  local body issue_num pr_num
  body="$(gh issue view "$EPIC" --repo "$REPO" --json body -q .body)"
  while read -r issue_num; do
    [[ -z "$issue_num" ]] || [[ "$issue_num" == "$EPIC" ]] && continue
    gh pr list --repo "$REPO" --state open --json number,body \
      -q ".[] | select(.body | test(\"(?i)closes #${issue_num}\\\\b\")) | .number"
  done < <(grep -oE '#[0-9]+' <<<"$body" | tr -d '#' | sort -nu)
}

if [[ -n "$PR" ]]; then
  bash "$SCRIPT_DIR/triage-pr-feedback.sh" --repo "$REPO" --pr "$PR" --latest-only || true
  if $AUTO && read -r created cid <<<"$(pr_latest_needs_action "$PR" || true)" && [[ -n "${cid:-}" ]]; then
    if already_dispatched "$PR" "$cid"; then
      echo "SKIP auto: PR #$PR comment $cid already dispatched"
    else
      bash "$SCRIPT_DIR/invoke-cursor-agent.sh" --repo "$REPO" --epic "$EPIC" --pr "$PR" ${DISCUSSION:+--discussion "$DISCUSSION"}
      mark_dispatched "$PR" "$cid"
    fi
  fi
else
  bash "$SCRIPT_DIR/triage-pr-feedback.sh" --repo "$REPO" --epic "$EPIC" || true
  if $AUTO; then
    while read -r pr_num; do
      [[ -z "$pr_num" ]] && continue
      if ! read -r _created cid <<<"$(pr_latest_needs_action "$pr_num" || true)" || [[ -z "${cid:-}" ]]; then
        continue
      fi
      if already_dispatched "$pr_num" "$cid"; then
        echo "SKIP auto: PR #$pr_num comment $cid already dispatched"
        continue
      fi
      echo "--- Auto dispatch PR #$pr_num (comment $cid) ---"
      bash "$SCRIPT_DIR/invoke-cursor-agent.sh" --repo "$REPO" --epic "$EPIC" --pr "$pr_num" ${DISCUSSION:+--discussion "$DISCUSSION"} || true
      mark_dispatched "$pr_num" "$cid"
    done < <(list_epic_prs | sort -nu)
  fi
fi

if ! $AUTO; then
  echo ""
  echo "--- Paste into Cursor Desktop Agent (or re-run with --auto) ---"
  if [[ -n "$PR" ]]; then
    cat <<EOF
按 .skill/skills/epic-delivery 处理 Epic #$EPIC / PR #$PR（repo $REPO${DISCUSSION:+, Discussion #$DISCUSSION}）。
只跟踪本 Epic 相关 PR/Issue。

或自动处理：scripts/epic/wake-local.sh --repo $REPO --epic $EPIC --pr $PR --auto
EOF
  else
    cat <<EOF
按 .skill/skills/epic-delivery 处理 Epic #$EPIC（repo $REPO${DISCUSSION:+, Discussion #$DISCUSSION}）下 open PR 的人工反馈。

或自动处理：export CURSOR_API_KEY=... && scripts/epic/wake-local.sh --repo $REPO --epic $EPIC --auto
EOF
  fi
fi
