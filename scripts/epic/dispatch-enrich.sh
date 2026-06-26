#!/usr/bin/env bash
# dispatch-enrich.sh — 从 MD 模版 + gh 拉取上下文，生成子 Agent dispatch_prompt（纯 shell）
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TPL_DIR="$(cd "${SCRIPT_DIR}/../../.skill/skills/epic-delivery/references" && pwd)"

de_die() { echo "dispatch-enrich: $*" >&2; exit 1; }

de_json_escape() {
  local s="$1" out="" i c
  for (( i=0; i<${#s}; i++ )); do
    c="${s:i:1}"
    case "$c" in
      $'\\') out+='\\' ;;
      '"') out+='\"' ;;
      $'\n') out+='\n' ;;
      $'\r') out+='\r' ;;
      $'\t') out+='\t' ;;
      *) out+="$c" ;;
    esac
  done
  printf '%s' "$out"
}

de_render_tpl() {
  local tpl="$1"
  [[ -f "$tpl" ]] || de_die "模版不存在: $tpl"
  local line key val
  while IFS= read -r line || [[ -n "$line" ]]; do
    while [[ "$line" == *'{{'*'}}'* ]]; do
      key="${line#*\{\{}"
      key="${key%%\}\}*}"
      val="${!key-}"
      line="${line//\{\{${key}\}\}/${val}}"
    done
    printf '%s\n' "$line"
  done < "$tpl"
}

de_extract_section() {
  local body="$1" heading="$2"
  printf '%s\n' "$body" | awk -v h="$heading" '
    BEGIN { ig=1; cap=0; n=0 }
    ig && $0 ~ "^##? *" h { cap=1; next }
    cap && /^##/ { exit }
    cap { if (n++ < 40) print }
  ' | head -c 800
}

de_parse_wt_id_title() {
  local title="$1"
  if [[ "$title" =~ \[([0-9]+)-([A-Za-z0-9]+)\] ]]; then
    printf '%s-%s\n' "${BASH_REMATCH[1]}" "$(printf '%s' "${BASH_REMATCH[2]}" | tr '[:upper:]' '[:lower:]')"
  fi
}

de_parse_wt_id_branch() {
  local branch="$1"
  if [[ "$branch" =~ ^feat/([0-9]+)-([a-z0-9]+) ]]; then
    printf '%s-%s\n' "${BASH_REMATCH[1]}" "${BASH_REMATCH[2]}"
  fi
}

de_find_worktree() {
  local branch="${1:-}" wt_id="${2:-}"
  local wt_root root candidates=() c wpath b
  wt_root="${WT_ROOT_ABS:-..}"
  root="$(git rev-parse --show-toplevel 2>/dev/null || true)"
  if [[ -n "$root" ]]; then
    wt_root="$(cd "$root/$wt_root" 2>/dev/null && pwd || echo "$wt_root")"
  fi
  if [[ -n "$wt_id" ]]; then
    shopt -s nullglob
    for c in "${wt_root}/wt-${wt_id}-"*; do
      [[ -d "$c" ]] && candidates+=("$(cd "$c" && pwd)")
    done
    shopt -u nullglob
  fi
  if [[ -n "$branch" && -n "$root" ]]; then
    while IFS= read -r wpath; do
      [[ -n "$wpath" ]] && candidates+=("$wpath")
    done < <(git -C "$root" worktree list --porcelain | awk -v br="$branch" '
      /^worktree / { w=$2 }
      /^branch / { b=$2; sub("refs/heads/", "", b); if (b==br) print w }
    ')
  fi
  for c in "${candidates[@]}"; do
    [[ -d "$c" ]] && { printf '%s' "$c"; return 0; }
  done
  if [[ -n "$wt_id" ]]; then
    printf '(未找到；执行 ./scripts/epic/epic.sh wt new --id %s --slug <short-desc>)' "$wt_id"
  else
    printf '(未找到 worktree；按 PR head 分支 checkout 或 wt new)'
  fi
}

de_build_skills() {
  local text="${1,,}"
  local skills=(
    ".skill/skills/epic-delivery/SKILL.md"
    ".skill/skills/project-knowledge/SKILL.md"
    ".skill/skills/review-standards/SKILL.md"
    ".skill/skills/security-check/SKILL.md"
    ".skill/skills/commit/SKILL.md"
  )
  de_skills_add() {
    local s="$1" x
    for x in "${skills[@]}"; do [[ "$x" == "$s" ]] && return; done
    skills+=("$s")
  }
  if [[ "$text" == *"core/"* || "$text" == *"collectionconfig"* ]]; then
    de_skills_add ".skill/skills/compile/SKILL.md"
  fi
  if [[ "$text" == *"plugins/"* || "$text" == *"flusher"* || "$text" == *"processor"* ]]; then
    de_skills_add ".skill/skills/testing-standards/SKILL.md"
  fi
  if [[ "$text" == *"docs/"* || "$text" == *"overview.md"* ]]; then
    :
  fi
  if [[ "$text" == *"pipeline/"* ]]; then
    de_skills_add ".skill/skills/e2e/SKILL.md"
  fi
  local s
  for s in "${skills[@]}"; do printf -- '- %s\n' "$s"; done
}

de_fetch_comment_body() {
  local repo="$1" comment_id="$2" pr="${3:-}"
  if [[ "$comment_id" =~ ^[0-9]+$ ]]; then
    local body
    body="$(gh api "repos/${repo}/pulls/${pr}/comments/${comment_id}" --jq '.body // empty' 2>/dev/null || true)"
    [[ -n "$body" ]] && { printf '%s' "$body"; return 0; }
    body="$(gh api "repos/${repo}/issues/comments/${comment_id}" --jq '.body // empty' 2>/dev/null || true)"
    [[ -n "$body" && "$body" != *'"message":"Not Found"'* ]] && printf '%s' "$body"
    return 0
  fi
  if [[ "$comment_id" == IC_* ]]; then
    gh api graphql -f query='query($id:ID!){node(id:$id){... on IssueComment{body} ... on PullRequestReviewComment{body path line}}}' \
      -f id="${comment_id}" --jq '.data.node.body // empty' 2>/dev/null || true
    return 0
  fi
}

de_maintainer_from_body() {
  local body="$1" preview="${2:-}"
  local line out=""
  while IFS= read -r line || [[ -n "$line" ]]; do
    line="${line#"${line%%[![:space:]]*}"}"
    [[ -z "$line" || "$line" == \>* || "$line" == *from=agent* ]] && continue
    out+="${line}"$'\n'
  done <<< "$body"
  out="${out%"$'\n'"}"
  if [[ -z "$out" && -n "$preview" ]]; then
    out="${preview:0:500}"
  fi
  printf '%s' "${out:0:1200}"
}

de_issue_title() {
  gh issue view "$2" --repo "$1" --json title --jq '.title // ""'
}

de_issue_body() {
  gh issue view "$2" --repo "$1" --json body --jq '.body // ""'
}

de_cmd_develop() {
  local epic="$1" repo="$2" issue_num="$3"
  local prereq_issue="${4:-}" prereq_pr="${5:-}" prereq_title="${6:-}"
  local title body wt_id worktree scope_allow scope_deny acceptance blocked branch_hint wt_new_cmd
  local issue_url task prereq_block blocked_line skills

  title="$(de_issue_title "$repo" "$issue_num")"
  body="$(de_issue_body "$repo" "$issue_num")"

  wt_id="$(de_parse_wt_id_title "$title")"
  worktree="$(de_find_worktree "" "$wt_id")"
  scope_allow="$(de_extract_section "$body" "范围锁")"
  [[ -z "$scope_allow" ]] && scope_allow="$(de_extract_section "$body" "允许改")"
  scope_deny="$(de_extract_section "$body" "禁止改")"
  acceptance="$(de_extract_section "$body" "验收标准")"
  blocked="$(printf '%s\n' "$body" | awk 'tolower($0) ~ /blocked by/ { sub(/^.*blocked by[[:space:]]*:?[[:space:]]*/,""); print; exit }')"

  [[ -z "$scope_allow" ]] && scope_allow="(见 Issue body)"
  [[ -z "$scope_deny" ]] && scope_deny="(见 Issue body)"
  [[ -z "$acceptance" ]] && acceptance="(见 Issue body)"

  if [[ -n "$wt_id" ]]; then
    branch_hint="feat/${wt_id}-<slug>"
    wt_new_cmd="./scripts/epic/epic.sh wt new --id ${wt_id} --slug <short-desc>"
  else
    branch_hint="(wt new 自动创建)"
    wt_new_cmd="./scripts/epic/epic.sh wt new --epic ${epic} --step <step> --slug <short-desc>"
  fi

  issue_url="https://github.com/${repo}/issues/${issue_num}"
  task="Issue #${issue_num} Develop"
  blocked_line=""
  [[ -n "$blocked" ]] && blocked_line="- Blocked by: ${blocked}"

  prereq_block=""
  if [[ -n "$prereq_issue" ]]; then
    prereq_block="=== 前置依赖（已满足，Issue 评论含交付摘要）===
- 前置 Issue: #${prereq_issue}
- 前置 PR: #${prereq_pr:-?}
- 摘要: ${prereq_title:0:200}

"
  fi

  SKILLS="$(de_build_skills "${body} ${title}")"
  TASK="$task"
  EPIC="$epic"
  WORKTREE="$worktree"
  WT_NEW_CMD="$wt_new_cmd"
  BRANCH_HINT="$branch_hint"
  ISSUE_URL="$issue_url"
  ISSUE_NUM="$issue_num"
  ISSUE_TITLE="$title"
  BLOCKED_LINE="$blocked_line"
  SCOPE_ALLOW="$scope_allow"
  SCOPE_DENY="$scope_deny"
  ACCEPTANCE="$acceptance"
  PREREQ_BLOCK="$prereq_block"

  local prompt
  prompt="$(de_render_tpl "${TPL_DIR}/dispatch-develop.md.tpl")"

  if [[ "${DE_OUTPUT:-prompt}" == json ]]; then
    printf '{"task":"%s","dispatch_prompt":"%s","prompt":"%s","dispatch":"orchestrator: paste dispatch_prompt into Task; sub-agent has NO session memory"}\n' \
      "$(de_json_escape "$task")" "$(de_json_escape "$prompt")" "$(de_json_escape "$prompt")"
  else
    printf '%s' "$prompt"
  fi
}

de_cmd_feedback() {
  local epic="$1" repo="$2" kind="$3" target="$4" comment_id="$5" url="$6" preview="${7:-}"
  local branch="" push_remote="origin" closes_issue="" issue_title="" pr_title=""
  local comment_body="" review_path="" review_line="" issue_body=""
  local wt_id worktree scope_allow scope_deny acceptance maintainer skills task pr_num review_loc

  if [[ "$kind" == pr ]]; then
    pr_num="$target"
    branch="$(gh pr view "$target" --repo "$repo" --json headRefName --jq '.headRefName // ""')"
    local head_owner
    head_owner="$(gh pr view "$target" --repo "$repo" --json headRepository --jq '.headRepository.owner.login // ""' 2>/dev/null || true)"
    local repo_owner="${repo%%/*}"
    if [[ -n "$head_owner" && "$head_owner" != "$repo_owner" ]]; then
      push_remote="origin"
    elif [[ -n "$head_owner" ]]; then
      push_remote="upstream"
    fi
    pr_title="$(gh pr view "$target" --repo "$repo" --json title --jq '.title // ""')"
    closes_issue="$(gh pr view "$target" --repo "$repo" --json body --jq '.body // ""' | grep -oiE 'closes\s+#([0-9]+)' | head -1 | grep -oE '[0-9]+' || true)"
    if [[ "$comment_id" =~ ^[0-9]+$ ]]; then
      comment_body="$(de_fetch_comment_body "$repo" "$comment_id" "$target")"
      review_path="$(gh api "repos/${repo}/pulls/${target}/comments/${comment_id}" --jq '.path // ""' 2>/dev/null || true)"
      review_line="$(gh api "repos/${repo}/pulls/${target}/comments/${comment_id}" --jq '.line // empty' 2>/dev/null || true)"
    else
      comment_body="$(de_fetch_comment_body "$repo" "$comment_id" "$target")"
      if [[ "$comment_id" == IC_* ]]; then
        review_path="$(gh api graphql -f query='query($id:ID!){node(id:$id){... on PullRequestReviewComment{path line}}}' \
          -f id="${comment_id}" --jq '.data.node.path // empty' 2>/dev/null || true)"
        review_line="$(gh api graphql -f query='query($id:ID!){node(id:$id){... on PullRequestReviewComment{path line}}}' \
          -f id="${comment_id}" --jq '.data.node.line // empty' 2>/dev/null || true)"
      fi
    fi
  elif [[ "$kind" == issue ]]; then
    pr_num="(issue)"
    closes_issue="$target"
    comment_body="$(de_fetch_comment_body "$repo" "$comment_id")"
  else
    de_die "未知 kind: $kind"
  fi

  if [[ -n "$closes_issue" ]]; then
    issue_title="$(de_issue_title "$repo" "$closes_issue")"
    issue_body="$(de_issue_body "$repo" "$closes_issue")"
  fi

  scope_allow="$(de_extract_section "$issue_body" "范围锁")"
  [[ -z "$scope_allow" ]] && scope_allow="$(de_extract_section "$issue_body" "允许改")"
  scope_deny="$(de_extract_section "$issue_body" "禁止改")"
  acceptance="$(de_extract_section "$issue_body" "验收标准")"
  [[ -z "$scope_allow" ]] && scope_allow="(见 Issue body)"
  [[ -z "$scope_deny" ]] && scope_deny="(见 Issue body)"
  [[ -z "$acceptance" ]] && acceptance="(见 Issue body)"

  wt_id="$(de_parse_wt_id_branch "$branch")"
  [[ -z "$wt_id" ]] && wt_id="$(de_parse_wt_id_title "$issue_title")"
  worktree="$(de_find_worktree "$branch" "$wt_id")"

  maintainer="$(de_maintainer_from_body "$comment_body" "$preview")"
  [[ -z "$maintainer" ]] && maintainer="(读 URL)"

  review_loc=""
  if [[ -n "$review_path" ]]; then
    review_loc="- review 位置: ${review_path}:${review_line:-?}"
  fi

  if [[ "$kind" == issue ]]; then
    task="Issue #${target} AddressFeedback"
  else
    task="PR #${target} AddressFeedback"
  fi

  SKILLS="$(de_build_skills "${comment_body} ${issue_body} ${review_path}")"
  TASK="$task"
  EPIC="$epic"
  WORKTREE="$worktree"
  BRANCH="${branch:-(见 gh pr view headRefName)}"
  PUSH_REMOTE="$push_remote"
  PR_NUM="$pr_num"
  COMMENT_ID="$comment_id"
  URL="$url"
  CLOSES_ISSUE="${closes_issue:-?}"
  ISSUE_TITLE="$issue_title"
  PR_TITLE="$pr_title"
  SCOPE_ALLOW="$scope_allow"
  SCOPE_DENY="$scope_deny"
  ACCEPTANCE="$acceptance"
  MAINTAINER_COMMENT="$maintainer"
  REVIEW_LOC="$review_loc"

  local prompt steps
  prompt="$(de_render_tpl "${TPL_DIR}/dispatch-address-feedback.md.tpl")"
  steps="编排：复制 dispatch_prompt 派 Task 执行 Agent → 完成后 mark-handled ${comment_id}"

  if [[ "${DE_OUTPUT:-prompt}" == json ]]; then
    printf '{"task":"%s","steps":"%s","dispatch_prompt":"%s","prompt":"%s","dispatch":"orchestrator: paste dispatch_prompt into Task; sub-agent has NO session memory"}\n' \
      "$(de_json_escape "$task")" "$(de_json_escape "$steps")" "$(de_json_escape "$prompt")" "$(de_json_escape "$prompt")"
  else
    printf '%s' "$prompt"
  fi
}

de_usage() {
  cat <<EOF
用法:
  dispatch-enrich.sh develop --epic N --repo O/R --issue N [--prerequisite-issue N [--prerequisite-pr N] [--prerequisite-title T]] [--json]
  dispatch-enrich.sh feedback --epic N --repo O/R --kind pr|issue --target N --comment-id ID --url URL [--preview TEXT] [--json]
EOF
}

main() {
  local cmd="${1:-}"
  shift || true
  local epic="" repo="" issue="" kind="" target="" comment_id="" url="" preview=""
  local prereq_issue="" prereq_pr="" prereq_title="" json=false
  DE_OUTPUT=prompt

  while [[ $# -gt 0 ]]; do
    case "$1" in
      --epic) epic="$2"; shift 2 ;;
      --repo) repo="$2"; shift 2 ;;
      --issue) issue="$2"; shift 2 ;;
      --kind) kind="$2"; shift 2 ;;
      --target) target="$2"; shift 2 ;;
      --comment-id) comment_id="$2"; shift 2 ;;
      --url) url="$2"; shift 2 ;;
      --preview) preview="$2"; shift 2 ;;
      --prerequisite-issue) prereq_issue="$2"; shift 2 ;;
      --prerequisite-pr) prereq_pr="$2"; shift 2 ;;
      --prerequisite-title) prereq_title="$2"; shift 2 ;;
      --json) json=true; DE_OUTPUT=json; shift ;;
      -h|--help) de_usage; exit 0 ;;
      *) de_die "未知参数: $1" ;;
    esac
  done

  epic="${epic:-${EPIC:-}}"
  repo="${repo:-${REPO:-}}"
  [[ -n "$epic" && -n "$repo" ]] || de_die "--epic 与 --repo 必填"

  case "$cmd" in
    develop)
      [[ -n "$issue" ]] || de_die "develop 需要 --issue"
      de_cmd_develop "$epic" "$repo" "$issue" "$prereq_issue" "$prereq_pr" "$prereq_title"
      ;;
    feedback)
      [[ -n "$kind" && -n "$target" && -n "$comment_id" ]] || de_die "feedback 需要 --kind --target --comment-id"
      de_cmd_feedback "$epic" "$repo" "$kind" "$target" "$comment_id" "${url:-}" "${preview:-}"
      ;;
    *)
      de_usage
      exit 1
      ;;
  esac
}

main "$@"
