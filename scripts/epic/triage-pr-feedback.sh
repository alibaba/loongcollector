#!/usr/bin/env bash
# Triage PR comments for epic-delivery feedback.
# Agent comments: must carry [epic-delivery] from=agent + action=...
# Human comments: no tag required; default action=required
#
# Usage:
#   triage-pr-feedback.sh --repo alibaba/loongcollector --pr 2619
#   triage-pr-feedback.sh --repo alibaba/loongcollector --pr 2619 --latest-only
#   triage-pr-feedback.sh --repo alibaba/loongcollector --epic 2595
set -euo pipefail

REPO=""
PR=""
EPIC=""
JSON_MODE=false
LATEST_ONLY=false

usage() {
  cat <<'EOF'
Usage:
  triage-pr-feedback.sh --repo owner/repo --pr <number> [--latest-only] [--json]
  triage-pr-feedback.sh --repo owner/repo --epic <number>
EOF
  exit 1
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --repo) REPO="$2"; shift 2 ;;
    --pr) PR="$2"; shift 2 ;;
    --epic) EPIC="$2"; shift 2 ;;
    --json) JSON_MODE=true; shift ;;
    --latest-only) LATEST_ONLY=true; shift ;;
    -h|--help) usage ;;
    *) echo "unknown: $1" >&2; usage ;;
  esac
done

[[ -n "$REPO" ]] || usage
[[ -n "$PR" || -n "$EPIC" ]] || usage

# stdout: "<status> <source> <author>"
# status: required | skip
classify_body() {
  local body="$1"
  if [[ "$body" == *'[epic-delivery]'* && "$body" == *'from=agent'* ]]; then
    if [[ "$body" == *'action=required'* ]]; then
      echo "required agent-tagged"
    else
      echo "skip agent-tagged"
    fi
    return
  fi
  # No agent tag → human, needs action
  echo "required human-default"
}

emit_required() {
  local pr_num="$1" author="$2" created="$3" body="$4" verdict="$5" title="$6" head="$7"
  if ! $JSON_MODE; then
    echo "ACTION REQUIRED  PR #$pr_num  comment by $author at $created  (${verdict#required })"
    echo "  head: $head"
    echo "  title: $title"
    echo "---"
    printf '%s\n' "$body" | head -20
    echo "..."
    echo ""
  fi
}

triage_pr() {
  local pr_num="$1"
  local title head count required_count human_count
  title="$(gh pr view "$pr_num" --repo "$REPO" --json title -q .title)"
  head="$(gh pr view "$pr_num" --repo "$REPO" --json headRefName -q .headRefName)"
  count="$(gh pr view "$pr_num" --repo "$REPO" --json comments -q '.comments | length')"

  required_count=0
  human_count=0

  if $LATEST_ONLY && [[ "$count" -gt 0 ]]; then
    local i=$((count - 1))
    local author created body verdict
    author="$(gh pr view "$pr_num" --repo "$REPO" --json comments -q ".comments[$i].author.login")"
    created="$(gh pr view "$pr_num" --repo "$REPO" --json comments -q ".comments[$i].createdAt")"
    body="$(gh pr view "$pr_num" --repo "$REPO" --json comments -q ".comments[$i].body")"
    verdict="$(classify_body "$body")"
    if [[ "$verdict" == required* ]]; then
      required_count=1
      human_count=1
      emit_required "$pr_num" "$author" "$created" "$body" "$verdict" "$title" "$head"
    fi
  else
    local i=0
    while [[ "$i" -lt "$count" ]]; do
      local author created body verdict
      author="$(gh pr view "$pr_num" --repo "$REPO" --json comments -q ".comments[$i].author.login")"
      created="$(gh pr view "$pr_num" --repo "$REPO" --json comments -q ".comments[$i].createdAt")"
      body="$(gh pr view "$pr_num" --repo "$REPO" --json comments -q ".comments[$i].body")"
      verdict="$(classify_body "$body")"
      if [[ "$verdict" == required* ]]; then
        required_count=$((required_count + 1))
        human_count=$((human_count + 1))
        emit_required "$pr_num" "$author" "$created" "$body" "$verdict" "$title" "$head"
      fi
      i=$((i + 1))
    done
  fi

  if $JSON_MODE; then
    gh pr view "$pr_num" --repo "$REPO" --json number,title,headRefName \
      -q "{repo: \"$REPO\", pr: .number, title: .title, head: .headRefName, required_count: $required_count, human_untagged: $human_count, needs_action: ($required_count > 0), latest_only: $LATEST_ONLY}"
  else
    if [[ "$required_count" -eq 0 ]]; then
      echo "OK  PR #$pr_num ($title) — no human feedback pending"
    else
      echo "SUMMARY  PR #$pr_num — $required_count human comment(s) need agent action"
    fi
  fi
}

if [[ -n "$PR" ]]; then
  triage_pr "$PR"
  exit 0
fi

echo "Epic #$EPIC triage ($REPO)"
gh issue view "$EPIC" --repo "$REPO" --json title -q .title
echo ""

found=0
while IFS= read -r pr_num; do
  [[ -z "$pr_num" ]] && continue
  triage_pr "$pr_num"
  found=1
done < <(gh pr list --repo "$REPO" --state open --json number,headRefName \
  --jq '.[] | select(.headRefName | test("feat/1928-")) | .number')

if [[ "$found" -eq 0 ]]; then
  echo "No open feat/1928-* PRs found; pass --pr explicitly"
fi
