#!/usr/bin/env bash
#
# epic.sh — Epic 评论/状态轮询与编排的单一入口（与具体 Epic 解耦，仅传 --epic <Issue编号>）。
#
# 子命令：
#   poll        前台轮询循环（与编排 Agent 会话同生死；Ctrl-C 停止）
#   poll-once   单次扫描；--init 仅建立 baseline 不报事件
#   triage      读取 pending 事件并输出派发提示（编排 Agent 每轮调用）
#   events      list | count | mark-handled <comment_id>
#   scope       打印 Epic 范围（子 Issue + 关联 PR）
#   reply       在评论 thread 内回复（review 行评真 thread；conversation 用 quote reply）
#   stop        停止后台记录的 poll 循环
#
# 全局参数：--epic <n>（多数子命令必填）、--repo owner/repo、--interval 秒、--state-dir 路径
# reply 专用：--pr <n> --comment-id <id> (--body 文本 | --body-file 路径)
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# ------------------------------------------------------------------ helpers
die() { echo "epic: $*" >&2; exit 1; }

usage() {
  awk 'NR<3{next} /^#/{sub(/^# ?/,""); print; next} {exit}' "${BASH_SOURCE[0]}"
}

load_env() {
  if [[ -f "${SCRIPT_DIR}/epic.env" ]]; then
    # shellcheck disable=SC1091
    source "${SCRIPT_DIR}/epic.env"
  fi
  if [[ -z "${REPO:-}" ]]; then
    REPO="$(gh repo view --json nameWithOwner --jq '.nameWithOwner' 2>/dev/null || true)"
  fi
  : "${REPO:?REPO 必须经 --repo / epic.env / gh repo view 提供}"
  : "${INTERVAL:=60}"
  if [[ -n "${EPIC:-}" ]]; then
    : "${STATE_DIR:=/tmp/epic-${EPIC}-poll}"
    EVENTS_FILE="${STATE_DIR}/events.jsonl"
    STATE_FILE="${STATE_DIR}/seen-ids.txt"
    HANDLED_FILE="${STATE_DIR}/handled-ids.txt"
    LOG_FILE="${STATE_DIR}/poll.log"
    PID_FILE="${STATE_DIR}/poll-loop.pid"
    mkdir -p "${STATE_DIR}"
    touch "${EVENTS_FILE}" "${STATE_FILE}" "${HANDLED_FILE}" "${LOG_FILE}"
  fi
}

require_epic() { [[ -n "${EPIC:-}" ]] || die "--epic <Issue编号> 必填"; }

log() { echo "$(date -u +%Y-%m-%dT%H:%M:%SZ) [epic=${EPIC}] $*" >> "${LOG_FILE}"; }

# 仅看非引用行：避免人工评论「引用」了 Agent 带 footer 的回复而被误判为 Agent 信息
is_actionable_body() {
  local body="$1" non_quoted
  non_quoted="$(printf '%s\n' "${body}" | grep -v '^[[:space:]]*>' || true)"
  if [[ "${non_quoted}" == *'from=agent'* ]]; then
    if [[ "${non_quoted}" == *'action=none'* ]] || [[ "${non_quoted}" == *'action=fyi'* ]]; then
      return 1
    fi
  fi
  return 0
}

seen_id()  { grep -Fxq "$1" "${STATE_FILE}" 2>/dev/null; }
mark_seen() { echo "$1" >> "${STATE_FILE}"; }

discover_sub_issues() {
  gh issue view "${EPIC}" --repo "${REPO}" --json body --jq '.body' \
    | grep -E '^- \[[ xX]\] #[0-9]+' | grep -oE '#[0-9]+' | tr -d '#' | sort -un
}

# stdout: JSON 行 {kind,target,id,author,created_at,url,body}
# kind: issue | pr | review | pr_state
scan_all_comments() {
  python3 - "${REPO}" "${EPIC}" <<'PY'
import json, re, subprocess, sys

repo, epic = sys.argv[1], sys.argv[2]

def gh_json(cmd):
    r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
    if r.returncode != 0 or not r.stdout.strip():
        return None
    return json.loads(r.stdout)

body = subprocess.check_output(
    ["gh", "issue", "view", epic, "--repo", repo, "--json", "body", "--jq", ".body"],
    universal_newlines=True,
)
issues = sorted(set(re.findall(r"^- \[[ xX]\] #(\d+)", body, re.M)))
targets = [int(epic)] + [int(x) for x in issues]

def emit(d):
    print(json.dumps(d, ensure_ascii=False))

for n in targets:
    data = gh_json(["gh", "issue", "view", str(n), "--repo", repo, "--json", "comments"])
    if not data:
        continue
    for c in data.get("comments", []):
        emit({"kind": "issue", "target": n, "id": c["id"], "author": c["author"]["login"],
              "created_at": c["createdAt"], "url": c["url"], "body": c["body"]})

prs = set()
for n in issues:
    out = subprocess.run(
        ["gh", "pr", "list", "--repo", repo, "--state", "all", "--limit", "100",
         "--search", "Closes #{}".format(n), "--json", "number"],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
    if out.returncode == 0 and out.stdout.strip():
        for row in json.loads(out.stdout):
            prs.add(row["number"])
    icom = subprocess.run(
        ["gh", "issue", "view", str(n), "--repo", repo, "--json", "comments"],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
    if icom.returncode == 0 and icom.stdout.strip():
        for c in json.loads(icom.stdout).get("comments", []):
            for m in re.finditer(r"#(\d+)", c.get("body", "")):
                chk = subprocess.run(
                    ["gh", "pr", "view", m.group(1), "--repo", repo, "--json", "number"],
                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
                if chk.returncode == 0 and chk.stdout.strip():
                    prs.add(json.loads(chk.stdout)["number"])

for pr in sorted(prs):
    data = gh_json(["gh", "pr", "view", str(pr), "--repo", repo,
                    "--json", "comments,state,mergedAt,updatedAt"])
    if data:
        state = data.get("state", "")
        if state:
            emit({"kind": "pr_state", "target": pr, "id": "prstate-{}-{}".format(pr, state),
                  "author": "", "created_at": data.get("mergedAt") or data.get("updatedAt") or "",
                  "url": "https://github.com/{}/pull/{}".format(repo, pr),
                  "body": "PR #{} state={}".format(pr, state)})
        for c in data.get("comments", []):
            emit({"kind": "pr", "target": pr, "id": c["id"], "author": c["author"]["login"],
                  "created_at": c["createdAt"], "url": c["url"], "body": c["body"]})
    rc = subprocess.run(
        ["gh", "api", "repos/{}/pulls/{}/comments".format(repo, pr)],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
    if rc.returncode == 0 and rc.stdout.strip():
        for c in json.loads(rc.stdout):
            emit({"kind": "review", "target": pr, "id": str(c["id"]), "author": c["user"]["login"],
                  "created_at": c["created_at"], "url": c["html_url"], "body": c["body"]})
PY
}

discover_prs() {
  scan_all_comments | python3 -c '
import json, sys
seen=set()
for line in sys.stdin:
    line=line.strip()
    if not line: continue
    r=json.loads(line)
    if r["kind"] in ("pr","review","pr_state"):
        seen.add(r["target"])
for n in sorted(seen): print(n)'
}

# ------------------------------------------------------------------ subcommands
cmd_poll_once() {
  require_epic
  local new_count=0
  while IFS= read -r row; do
    [[ -z "${row}" ]] && continue
    local id body
    id="$(printf '%s' "${row}" | python3 -c 'import json,sys; print(json.loads(sys.stdin.read())["id"])')"
    body="$(printf '%s' "${row}" | python3 -c 'import json,sys; print(json.loads(sys.stdin.read())["body"])')"
    if seen_id "${id}"; then continue; fi
    mark_seen "${id}"
    [[ "${INIT:-false}" == true ]] && continue
    if ! is_actionable_body "${body}"; then
      log "skip agent info id=${id}"
      continue
    fi
    local event
    event="$(printf '%s' "${row}" | python3 -c 'import json,sys; r=json.loads(sys.stdin.read()); print(json.dumps({k:r[k] for k in ("kind","target","id","author","created_at","url")} | {"comment_id": r["id"], "preview": r["body"].replace(chr(10)," ")[:200], "action":"AddressFeedback", "epic": '"${EPIC}"'}, ensure_ascii=False))')"
    echo "${event}"
    echo "${event}" >> "${EVENTS_FILE}"
    log "NEW event id=${id}"
    new_count=$((new_count + 1))
  done < <(scan_all_comments)

  if [[ "${INIT:-false}" == true ]]; then
    local tracked; tracked="$(wc -l < "${STATE_FILE}" | tr -d ' ')"
    log "init baseline: ${tracked} ids tracked"
    echo "{\"init\":true,\"epic\":${EPIC},\"tracked\":${tracked}}"
  else
    echo "{\"poll\":true,\"epic\":${EPIC},\"new_events\":${new_count}}" >&2
  fi
}

cmd_poll() {
  require_epic
  cleanup() { rm -f "${PID_FILE}"; log "poll-loop stopped pid=$$"; echo "[$(date -u +%H:%M:%S)] poll-loop stopped (epic #${EPIC})"; exit 0; }
  trap cleanup INT TERM

  if [[ ! -s "${STATE_FILE}" ]]; then
    INIT=true cmd_poll_once
  else
    log "poll-loop resumed with existing baseline ($(wc -l < "${STATE_FILE}") ids)"
  fi
  echo $$ > "${PID_FILE}"
  log "poll-loop started pid=$$ interval=${INTERVAL}s"
  echo "[$(date -u +%H:%M:%S)] poll-loop started (epic #${EPIC}, every ${INTERVAL}s). Ctrl-C to stop."

  while true; do
    local new count ts
    new="$(cmd_poll_once 2>/dev/null || true)"
    count="$(printf '%s\n' "${new}" | grep -c '"comment_id"' || true)"
    ts="[$(date -u +%H:%M:%S)]"
    if [[ "${count}" -gt 0 ]]; then
      echo "${ts} ${count} new event(s) -> run: ./scripts/epic/epic.sh triage --epic ${EPIC}"
      printf '%s\n' "${new}" | grep '"comment_id"' | sed 's/^/    /'
    else
      echo "${ts} no new events (epic #${EPIC})"
    fi
    sleep "${INTERVAL}"
  done
}

cmd_triage() {
  require_epic
  local count=0
  while IFS= read -r line; do
    [[ -z "${line}" ]] && continue
    local id
    id="$(printf '%s' "${line}" | python3 -c 'import json,sys; print(json.loads(sys.stdin.read())["comment_id"])')"
    if grep -Fxq "${id}" "${HANDLED_FILE}" 2>/dev/null; then continue; fi
    count=$((count + 1))
    printf '%s' "${line}" | python3 -c '
import json, sys
e = json.loads(sys.stdin.read())
repo, epic = sys.argv[1], sys.argv[2]
kind, target = e["kind"], e["target"]
if kind == "pr_state":
    preview = e.get("preview", "")
    print("DISPATCH PR #{} STATE-CHANGE: {} (epic #{})".format(target, preview, epic))
    if "MERGED" in preview:
        print("  action: 勾选 Epic checklist；清理 worktree；解锁被 Blocked by 此 PR 的后续 Issue")
    elif "CLOSED" in preview:
        print("  action: 确认是否需重开 / 调整 Epic 计划")
    print("  hint: gh pr view {} --repo {}".format(target, repo))
elif kind == "issue":
    print("DISPATCH issue #{} AddressFeedback (epic #{})".format(target, epic))
    print("  hint: gh issue view {} --repo {}".format(target, repo))
else:
    print("DISPATCH PR #{} AddressFeedback (epic #{})".format(target, epic))
    print("  hint: gh pr view {} --repo {}".format(target, repo))
print("  url: {}".format(e["url"]))
print("  author: {}".format(e["author"]))
print("  preview: {}".format(e.get("preview", "")[:120]))
print("  mark: ./scripts/epic/epic.sh events --epic {} mark-handled {}".format(epic, e["comment_id"]))
print("")
' "${REPO}" "${EPIC}"
  done < "${EVENTS_FILE}"
  if [[ "${count}" -eq 0 ]]; then
    echo "NO_PENDING_EVENTS epic=#${EPIC}"
  else
    echo "PENDING_COUNT=${count} epic=#${EPIC}"
  fi
}

cmd_events() {
  require_epic
  local action="${1:-list}"
  pending() {
    while IFS= read -r line; do
      [[ -z "${line}" ]] && continue
      local id
      id="$(printf '%s' "${line}" | python3 -c 'import json,sys; print(json.loads(sys.stdin.read())["comment_id"])')"
      grep -Fxq "${id}" "${HANDLED_FILE}" 2>/dev/null || echo "${line}"
    done < "${EVENTS_FILE}"
  }
  case "${action}" in
    list) pending ;;
    count) pending | wc -l ;;
    mark-handled) [[ -n "${2:-}" ]] || die "mark-handled 需要 <comment_id>"; echo "$2" >> "${HANDLED_FILE}"; log "marked handled $2" ;;
    *) die "events 用法：list | count | mark-handled <comment_id>" ;;
  esac
}

cmd_scope() {
  require_epic
  echo "Epic #${EPIC} (${REPO})"
  echo "Sub-issues:"; discover_sub_issues | sed 's/^/  #/'
  echo "PRs:"; discover_prs | sed 's/^/  #/' || echo "  (none)"
}

cmd_stop() {
  require_epic
  if [[ -f "${PID_FILE}" ]] && kill -0 "$(cat "${PID_FILE}")" 2>/dev/null; then
    local pid; pid="$(cat "${PID_FILE}")"
    kill "${pid}" 2>/dev/null || true
    rm -f "${PID_FILE}"
    echo "poll-loop stopped for epic #${EPIC} (was pid ${pid})"
  else
    echo "poll-loop not running for epic #${EPIC}"
  fi
}

cmd_reply() {
  : "${PR:?reply 需要 --pr <n>}"
  : "${COMMENT_ID:?reply 需要 --comment-id <id>}"
  if [[ -n "${BODY_FILE:-}" ]]; then BODY="$(cat "${BODY_FILE}")"; fi
  : "${BODY:?reply 需要 --body 或 --body-file}"
  local owner="${REPO%%/*}" repo="${REPO##*/}"

  if gh api -X POST "repos/${owner}/${repo}/pulls/${PR}/comments/${COMMENT_ID}/replies" -f body="${BODY}" >/dev/null 2>&1; then
    echo "replied in review thread (comment ${COMMENT_ID})"; return 0
  fi
  if gh api -X POST "repos/${owner}/${repo}/issues/comments/${COMMENT_ID}/replies" -f body="${BODY}" >/dev/null 2>&1; then
    echo "replied in conversation thread (comment ${COMMENT_ID})"; return 0
  fi
  # conversation 评论无 thread reply API：fallback 为 quote reply（引用原文）
  local parent quoted full
  parent="$(gh api "repos/${owner}/${repo}/issues/comments/${COMMENT_ID}" --jq '.body' 2>/dev/null || true)"
  [[ -n "${parent}" ]] || die "评论 ${COMMENT_ID} 未找到"
  quoted="$(printf '%s' "${parent}" | python3 -c 'import sys; print("\n".join("> "+l for l in sys.stdin.read().strip().splitlines() or [""]))')"
  full="${quoted}

${BODY}"
  gh api -X POST "repos/${owner}/${repo}/issues/${PR}/comments" -f body="${full}" --jq '.html_url'
  echo "replied via quote-reply (conversation comment ${COMMENT_ID})"
}

# ------------------------------------------------------------------ dispatch
[[ $# -ge 1 ]] || { usage; exit 1; }
SUBCMD="$1"; shift

EPIC=""; REPO=""; STATE_DIR=""; INTERVAL=""; PR=""; COMMENT_ID=""; BODY=""; BODY_FILE=""; INIT=false
REST=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    --epic) EPIC="$2"; shift 2 ;;
    --repo) REPO="$2"; shift 2 ;;
    --state-dir) STATE_DIR="$2"; shift 2 ;;
    --interval) INTERVAL="$2"; shift 2 ;;
    --pr) PR="$2"; shift 2 ;;
    --comment-id) COMMENT_ID="$2"; shift 2 ;;
    --body) BODY="$2"; shift 2 ;;
    --body-file) BODY_FILE="$2"; shift 2 ;;
    --init) INIT=true; shift ;;
    -h|--help) usage; exit 0 ;;
    --) shift; REST+=("$@"); break ;;
    *) REST+=("$1"); shift ;;
  esac
done

load_env

case "${SUBCMD}" in
  poll)       cmd_poll ;;
  poll-once)  cmd_poll_once ;;
  triage)     cmd_triage ;;
  events)     cmd_events ${REST[@]+"${REST[@]}"} ;;
  scope)      cmd_scope ;;
  reply)      cmd_reply ;;
  stop)       cmd_stop ;;
  -h|--help|help) usage ;;
  *) die "未知子命令：${SUBCMD}（poll|poll-once|triage|events|scope|reply|stop）" ;;
esac
