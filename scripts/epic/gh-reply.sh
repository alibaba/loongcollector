#!/usr/bin/env bash
# Reply to a PR comment in-thread (review thread) or quote-reply (conversation).
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/lib/common.sh"

PR=""
COMMENT_ID=""
BODY=""
BODY_FILE=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --epic) EPIC="$2"; shift 2 ;;
    --repo) REPO="$2"; shift 2 ;;
    --pr) PR="$2"; shift 2 ;;
    --comment-id) COMMENT_ID="$2"; shift 2 ;;
    --body) BODY="$2"; shift 2 ;;
    --body-file) BODY_FILE="$2"; shift 2 ;;
    -h|--help)
      cat <<EOF
Usage: $(basename "$0") --pr <n> --comment-id <id> (--body text | --body-file path)

  Review 行评：优先 POST .../pulls/{pr}/comments/{id}/replies（真 thread）
  Conversation 评论：若 replies API 不可用，fallback 为 Quote reply（引用原文，禁止无引用顶层汇总评）
EOF
      exit 0
      ;;
    *) epic_die "unknown arg: $1" ;;
  esac
done
if [[ -n "${EPIC:-}" ]]; then
  epic_parse_global_args --epic "${EPIC}" ${REPO:+--repo "$REPO"}
else
  : "${REPO:=$(gh repo view --json nameWithOwner --jq '.nameWithOwner')}"
fi
: "${PR:?--pr required}"
: "${COMMENT_ID:?--comment-id required}"
if [[ -n "${BODY_FILE}" ]]; then
  BODY="$(cat "${BODY_FILE}")"
fi
: "${BODY:?--body or --body-file required}"

owner="${REPO%%/*}"
repo="${REPO##*/}"

if gh api -X POST "repos/${owner}/${repo}/pulls/${PR}/comments/${COMMENT_ID}/replies" -f body="${BODY}" >/dev/null 2>&1; then
  echo "replied in review thread (comment ${COMMENT_ID})"
  exit 0
fi

if gh api -X POST "repos/${owner}/${repo}/issues/comments/${COMMENT_ID}/replies" -f body="${BODY}" >/dev/null 2>&1; then
  echo "replied in conversation thread (comment ${COMMENT_ID})"
  exit 0
fi

parent_body="$(gh api "repos/${owner}/${repo}/issues/comments/${COMMENT_ID}" --jq '.body' 2>/dev/null || true)"
if [[ -z "${parent_body}" ]]; then
  epic_die "comment ${COMMENT_ID} not found on ${REPO}"
fi

quoted="$(python3 - "${parent_body}" <<'PY'
import sys
text = sys.argv[1].strip()
lines = text.splitlines() or [text]
print("\n".join("> " + line for line in lines))
PY
)"

full_body="${quoted}

${BODY}"

gh api -X POST "repos/${owner}/${repo}/issues/${PR}/comments" -f body="${full_body}" --jq '.html_url'
echo "replied via quote-reply (conversation comment ${COMMENT_ID})"
