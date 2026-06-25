#!/usr/bin/env bash
# Invoke Cursor Agent to handle Epic PR feedback (local CLI or Cloud API).
#
# Requires: CURSOR_API_KEY (https://cursor.com/dashboard → Integrations)
#
# Usage:
#   invoke-cursor-agent.sh --repo owner/repo --epic N --pr N [--discussion D]
set -euo pipefail

REPO=""
EPIC=""
PR=""
DISCUSSION=""
REPO_ROOT="${EPIC_REPO_ROOT:-/apsara/workspace/loongcollector}"
MODEL="${CURSOR_AGENT_MODEL:-composer-2.5}"
API_KEY="${CURSOR_API_KEY:-}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --repo) REPO="$2"; shift 2 ;;
    --epic) EPIC="$2"; shift 2 ;;
    --pr) PR="$2"; shift 2 ;;
    --discussion) DISCUSSION="$2"; shift 2 ;;
    -h|--help)
      echo "Usage: invoke-cursor-agent.sh --repo owner/repo --epic N --pr N [--discussion D]"
      exit 0
      ;;
    *) echo "unknown: $1" >&2; exit 1 ;;
  esac
done

[[ -n "$REPO" && -n "$EPIC" && -n "$PR" ]] || {
  echo "need --repo --epic --pr" >&2
  exit 1
}

if [[ -z "$API_KEY" ]]; then
  echo "CURSOR_API_KEY is not set; export it from Cursor Dashboard → Integrations" >&2
  exit 1
fi

PR_URL="$(gh pr view "$PR" --repo "$REPO" --json url -q .url)"
PR_TITLE="$(gh pr view "$PR" --repo "$REPO" --json title -q .title)"
REPO_URL="https://github.com/${REPO}"

PROMPT_FILE="$(mktemp)"
trap 'rm -f "$PROMPT_FILE"' EXIT

cat >"$PROMPT_FILE" <<EOF
按 .skill/skills/epic-delivery 处理 Epic #${EPIC} / PR #${PR}（${REPO_URL}，PR: ${PR_URL}${DISCUSSION:+, Discussion #${DISCUSSION}}）。

约束：
- 只处理本 Epic / 本 PR；不要扫全仓其它 PR
- 加载 .skill/skills/epic-delivery/SKILL.md 阶段 6（AddressFeedback）
- 子 skill 暂从 .claude/skills/ 加载（compile / commit / code-review 等）
- C++ 改动须 make core + run_core_ut.sh，禁止写「待 CI」糊弄
- 禁止 merge / approve / force-push / 改 .github/workflows/
- Agent 回复 PR 评论须带 footer: \`[epic-delivery]\` from=agent role=feedback-handler action=none

步骤：
1. bash scripts/epic/triage-pr-feedback.sh --repo ${REPO} --pr ${PR} --latest-only
2. gh pr view ${PR} --repo ${REPO}；处理最新人工评论
3. push 到 PR 分支并回复 thread

PR 标题：${PR_TITLE}
EOF

PROMPT="$(cat "$PROMPT_FILE")"

invoke_local_cli() {
  local agent_bin=""
  for candidate in "${CURSOR_AGENT_BIN:-}" "$HOME/.local/bin/agent" "$HOME/.cursor/bin/agent"; do
    [[ -n "$candidate" && -x "$candidate" ]] || continue
    if "$candidate" --help >/dev/null 2>&1; then
      agent_bin="$candidate"
      break
    fi
  done
  [[ -n "$agent_bin" ]] || return 1

  echo "Invoking local Cursor CLI: $agent_bin (cwd=$REPO_ROOT)"
  cd "$REPO_ROOT"
  export CURSOR_API_KEY
  "$agent_bin" -p "$PROMPT" --model "$MODEL" --force
}

invoke_cloud_api() {
  echo "Invoking Cursor Cloud Agent API for PR #${PR}"
  local payload
  payload="$(python3 - "$PROMPT_FILE" "$PR_URL" "$REPO_URL" "$MODEL" <<'PY'
import json, sys
prompt_path, pr_url, repo_url, model = sys.argv[1:5]
text = open(prompt_path, encoding="utf-8").read()
body = {
    "prompt": {"text": text},
    "model": {"id": model},
    "repos": [{"url": repo_url, "prUrl": pr_url}],
    "workOnCurrentBranch": True,
    "autoCreatePR": False,
}
print(json.dumps(body))
PY
)"
  curl -fsS --request POST \
    --url "https://api.cursor.com/v1/agents" \
    -u "${API_KEY}:" \
    --header "Content-Type: application/json" \
    --data "$payload"
}

if invoke_local_cli; then
  echo "Local agent finished for PR #${PR}"
  exit 0
fi

echo "Local agent CLI unavailable; falling back to Cloud Agents API" >&2
resp="$(invoke_cloud_api)" || {
  echo "Cloud API call failed" >&2
  exit 1
}
echo "$resp"
agent_id="$(python3 -c "import json,sys; print(json.load(sys.stdin).get('agent',{}).get('id',''))" <<<"$resp" 2>/dev/null || true)"
echo "Cloud agent dispatched${agent_id:+ id=$agent_id} for PR #${PR}"
