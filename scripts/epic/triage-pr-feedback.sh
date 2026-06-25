#!/usr/bin/env bash
# Triage PR comments for epic-delivery feedback.
# Agent comments: must carry [epic-delivery] from=agent + action=...
# Human comments: no tag required; default action=required
#
# Usage:
#   triage-pr-feedback.sh --repo owner/repo --pr <number> [--latest-only]
#   triage-pr-feedback.sh --repo owner/repo --epic <number>
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
  if [[ "$body" == *'**自检**'* ]] || [[ "$body" == *'[epic-delivery triage]'* ]]; then
    echo "skip untagged-agent"
    return
  fi
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

# Sub-issue numbers from Epic checklist (exclude epic itself)
epic_sub_issue_numbers() {
  local body
  body="$(gh issue view "$EPIC" --repo "$REPO" --json body -q .body)"
  grep -oE '#[0-9]+' <<<"$body" | tr -d '#' | sort -nu | while read -r n; do
    [[ "$n" == "$EPIC" ]] && continue
    echo "$n"
  done
}

# Open PRs whose body closes a given issue
prs_for_issue() {
  local issue_num="$1"
  gh pr list --repo "$REPO" --state open --json number,body \
    -q ".[] | select(.body | test(\"(?i)closes #${issue_num}\\\\b\")) | .number"
}

epic_open_prs() {
  local issue_num pr_num
  while IFS= read -r issue_num; do
    [[ -z "$issue_num" ]] && continue
    while IFS= read -r pr_num; do
      [[ -z "$pr_num" ]] && continue
      echo "$pr_num"
    done < <(prs_for_issue "$issue_num")
  done < <(epic_sub_issue_numbers)
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
done < <(epic_open_prs | sort -nu)

if [[ "$found" -eq 0 ]]; then
  echo "No open PRs linked to Epic #$EPIC sub-issues (Closes #<issue>); pass --pr explicitly"
fi
