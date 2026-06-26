#!/usr/bin/env bash
#
# epic.sh — Epic 评论/状态轮询与编排的单一入口（与具体 Epic 解耦，仅传 --epic <Issue编号>）。
#
# 子命令：
#   poll        前台轮询循环（与编排 Agent 会话同生死；Ctrl-C 停止）
#               检测到新事件时默认自动 dispatch（可用 --no-auto-dispatch 关闭）
#   poll-once   单次扫描；--init 仅建立 baseline 不报事件
#   triage      读取 pending 事件并输出派发提示
#   dispatch    对 pending 事件执行 triage + DISPATCH_HOOK
#   inbox       编排 Agent 读取 pending（--json 含 prompt）；--wait 阻塞直到有新事件
#   events      list | count | mark-handled <comment_id>
#   scope       打印 Epic 范围（子 Issue + 关联 PR）
#   reply       在评论 thread 内回复（review 行评真 thread；conversation 用 quote reply）
#   stop        停止后台记录的 poll 循环
#   merge-followup  PR merge 后：勾选 checklist、清理 worktree、解锁/更新后续 Issue、标记开工并输出派工 JSON
#   label-sync    按 Issue 状态/PR 同步 agent-ready ↔ in-progress
#
# 全局参数：--epic <n>（多数子命令必填）、--repo owner/repo、--interval 秒、--state-dir 路径
# reply 专用：--pr <n> --comment-id <id> (--body 文本 | --body-file 路径)
#
# 限流优化：Epic 范围（子 Issue / PR）按 SCOPE_TTL（默认 600s）缓存到 state 目录，
#   轮询周期内复用；搜索 API（30/min）仅在缓存过期时调用。`scope` 子命令强制刷新。
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
  # scope（子 Issue / PR 列表）缓存有效期（秒）：TTL 内复用缓存，避免每个轮询周期都重算范围
  # 而打爆 GitHub 搜索 API（30/min）与触发二级限流。
  : "${SCOPE_TTL:=600}"
  : "${AUTO_DISPATCH:=false}"
  : "${DISPATCH_RETRY_PENDING:=true}"
  : "${AGENT_NOTIFY:=true}"
  if [[ -n "${EPIC:-}" ]]; then
    : "${STATE_DIR:=/tmp/epic-${EPIC}-poll}"
    EVENTS_FILE="${STATE_DIR}/events.jsonl"
    STATE_FILE="${STATE_DIR}/seen-ids.txt"
    HANDLED_FILE="${STATE_DIR}/handled-ids.txt"
    LOG_FILE="${STATE_DIR}/poll.log"
    PID_FILE="${STATE_DIR}/poll-loop.pid"
    SCOPE_SUBS_FILE="${STATE_DIR}/scope-subs.txt"
    SCOPE_PRS_FILE="${STATE_DIR}/scope-prs.txt"
    mkdir -p "${STATE_DIR}"
    touch "${EVENTS_FILE}" "${STATE_FILE}" "${HANDLED_FILE}" "${LOG_FILE}"
  fi
}

require_epic() { [[ -n "${EPIC:-}" ]] || die "--epic <Issue编号> 必填"; }

log() { echo "$(date -u +%Y-%m-%dT%H:%M:%SZ) [epic=${EPIC}] $*" >> "${LOG_FILE}"; }

# 从 MD 模版生成 dispatch_prompt（scripts/epic/dispatch-enrich.sh）
enrich_dispatch_prompt() {
  load_env
  wt_cfg 2>/dev/null || true
  export WT_ROOT_ABS="${WT_ROOT_ABS:-..}"
  "${SCRIPT_DIR}/dispatch-enrich.sh" "$@"
}

merge_inbox_enrich() {
  local event_line="$1" enrich_line
  enrich_line="$2"
  printf '%s' "${event_line}" | python3 -c '
import json, sys
ev = json.load(sys.stdin)
en = json.loads(sys.argv[1])
ev.update(en)
print(json.dumps(ev, ensure_ascii=False))
' "${enrich_line}"
}

enrich_inbox_event() {
  local line="$1" kind target cid url preview task steps
  kind="$(printf '%s' "${line}" | python3 -c 'import json,sys; print(json.load(sys.stdin).get("kind",""))')"
  if [[ "${kind}" == pr_state ]]; then
    target="$(printf '%s' "${line}" | python3 -c 'import json,sys; print(json.load(sys.stdin).get("target",""))')"
    preview="$(printf '%s' "${line}" | python3 -c 'import json,sys; print(json.load(sys.stdin).get("preview",""))')"
    cid="$(printf '%s' "${line}" | python3 -c 'import json,sys; print(json.load(sys.stdin).get("comment_id",""))')"
    url="$(printf '%s' "${line}" | python3 -c 'import json,sys; print(json.load(sys.stdin).get("url",""))')"
    task="PR #${target} 状态变更：${preview}"
    if [[ "${preview}" == *MERGED* ]]; then
      steps="1) merge-followup --epic ${EPIC} --pr ${target} --json 2) 对 unlocked[] 每条复制 dispatch_prompt 派 Task 执行 Agent 3) mark-handled ${cid}"
    else
      steps="确认 PR 关闭原因；mark-handled ${cid}"
    fi
    printf '%s' "${line}" | python3 -c '
import json, sys
ev = json.load(sys.stdin)
ev.update(task=sys.argv[1], steps=sys.argv[2], dispatch="orchestrator: merge-followup then Task sub-agents",
          prompt="{} · url={} · {}".format(sys.argv[1], ev.get("url",""), sys.argv[2]))
print(json.dumps(ev, ensure_ascii=False))
' "${task}" "${steps}"
    return 0
  fi
  target="$(printf '%s' "${line}" | python3 -c 'import json,sys; print(json.load(sys.stdin).get("target",""))')"
  cid="$(printf '%s' "${line}" | python3 -c 'import json,sys; print(json.load(sys.stdin).get("comment_id",""))')"
  url="$(printf '%s' "${line}" | python3 -c 'import json,sys; print(json.load(sys.stdin).get("url",""))')"
  preview="$(printf '%s' "${line}" | python3 -c 'import json,sys; print(json.load(sys.stdin).get("preview",""))')"
  local enrich_line
  enrich_line="$(enrich_dispatch_prompt feedback \
    --epic "${EPIC}" --repo "${REPO}" --kind "${kind}" --target "${target}" \
    --comment-id "${cid}" --url "${url}" --preview "${preview}" --json)"
  merge_inbox_enrich "${line}" "${enrich_line}"
}

# 仅看非引用行：避免人工评论「引用」了 Agent 带 footer 的回复而被误判为 Agent 信息
is_actionable_body() {
  local body="$1" non_quoted
  non_quoted="$(printf '%s\n' "${body}" | grep -v '^[[:space:]]*>' || true)"
  if [[ "${non_quoted}" == *'from=agent'* ]]; then
    if [[ "${non_quoted}" == *'action=none'* ]] || [[ "${non_quoted}" == *'action=fyi'* ]] \
      || [[ "${non_quoted}" == *'action=unlock-notify'* ]] \
      || [[ "${non_quoted}" == *'action=start-dispatch'* ]] \
      || [[ "${non_quoted}" == *'action=address-feedback'* ]] \
      || [[ "${non_quoted}" == *'action=pr-opened'* ]] \
      || [[ "${non_quoted}" == *'action=ready-to-merge'* ]]; then
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

# scope 缓存是否仍新鲜（TTL 内）。范围（子 Issue / PR）变动很慢，无需每周期重算。
scope_fresh() {
  [[ -f "${SCOPE_PRS_FILE}" ]] || return 1
  local mtime now age
  mtime="$(stat -c %Y "${SCOPE_PRS_FILE}" 2>/dev/null || echo 0)"
  now="$(date +%s)"
  age=$(( now - mtime ))
  [[ "${age}" -lt "${SCOPE_TTL}" ]]
}

# 重算 Epic 范围并写入缓存：子 Issue 来自 Epic body；PR 仅靠「Closes #<sub>」搜索确定。
# 搜索 API（30/min）只在此处调用，且受 TTL 节流，不再每个轮询周期触发。
refresh_scope() {
  discover_sub_issues > "${SCOPE_SUBS_FILE}.tmp" 2>/dev/null || true
  mv -f "${SCOPE_SUBS_FILE}.tmp" "${SCOPE_SUBS_FILE}"
  : > "${SCOPE_PRS_FILE}.tmp"
  local n
  while IFS= read -r n; do
    [[ -n "${n}" ]] || continue
    gh pr list --repo "${REPO}" --state all --limit 100 \
      --search "Closes #${n}" --json number --jq '.[].number' 2>/dev/null \
      >> "${SCOPE_PRS_FILE}.tmp" || true
  done < "${SCOPE_SUBS_FILE}"
  sort -un "${SCOPE_PRS_FILE}.tmp" -o "${SCOPE_PRS_FILE}.tmp"
  # 限流保护：搜索若因限流整体失败会得到空结果；此时**不要**用空覆盖已有非空缓存，
  # 否则轮询会误判「无 PR」而漏掉所有 PR 评论。仅在确有结果、或旧缓存本就为空时才替换。
  if [[ -s "${SCOPE_PRS_FILE}.tmp" ]] || [[ ! -s "${SCOPE_PRS_FILE}" ]]; then
    mv -f "${SCOPE_PRS_FILE}.tmp" "${SCOPE_PRS_FILE}"
  else
    rm -f "${SCOPE_PRS_FILE}.tmp"
    # 续上旧缓存的 mtime 已过期，touch 一下避免反复重试加剧限流
    touch "${SCOPE_PRS_FILE}"
    log "scope refresh got 0 prs (likely rate-limited); kept existing $(wc -l < "${SCOPE_PRS_FILE}" | tr -d ' ') prs"
    return 0
  fi
  log "scope refreshed: $(wc -l < "${SCOPE_SUBS_FILE}" | tr -d ' ') subs, $(wc -l < "${SCOPE_PRS_FILE}" | tr -d ' ') prs"
}

ensure_scope() { scope_fresh || refresh_scope; }

# stdout: JSON 行 {kind,target,id,author,created_at,url,body}
# kind: issue | pr | review | pr_state
# 只拉评论（issue 评论 + PR 评论/状态 + review 行评），范围用缓存，避免逐个 #数字 探测。
scan_all_comments() {
  ensure_scope
  python3 - "${REPO}" "${EPIC}" "${SCOPE_SUBS_FILE}" "${SCOPE_PRS_FILE}" <<'PY'
import json, subprocess, sys

repo, epic, subs_file, prs_file = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]

def gh_json(cmd):
    r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
    if r.returncode != 0 or not r.stdout.strip():
        return None
    return json.loads(r.stdout)

def read_lines(path):
    try:
        with open(path) as fh:
            return [x.strip() for x in fh if x.strip()]
    except FileNotFoundError:
        return []

subs = read_lines(subs_file)
prs = read_lines(prs_file)
targets = [epic] + subs

def emit(d):
    print(json.dumps(d, ensure_ascii=False))

for n in targets:
    data = gh_json(["gh", "issue", "view", str(n), "--repo", repo, "--json", "comments"])
    if not data:
        continue
    for c in data.get("comments", []):
        emit({"kind": "issue", "target": int(n), "id": c["id"], "author": c["author"]["login"],
              "created_at": c["createdAt"], "url": c["url"], "body": c["body"]})

for pr in prs:
    data = gh_json(["gh", "pr", "view", str(pr), "--repo", repo,
                    "--json", "comments,state,mergedAt,updatedAt"])
    if data:
        state = data.get("state", "")
        if state:
            emit({"kind": "pr_state", "target": int(pr), "id": "prstate-{}-{}".format(pr, state),
                  "author": "", "created_at": data.get("mergedAt") or data.get("updatedAt") or "",
                  "url": "https://github.com/{}/pull/{}".format(repo, pr),
                  "body": "PR #{} state={}".format(pr, state)})
        for c in data.get("comments", []):
            emit({"kind": "pr", "target": int(pr), "id": c["id"], "author": c["author"]["login"],
                  "created_at": c["createdAt"], "url": c["url"], "body": c["body"]})
    rc = subprocess.run(
        ["gh", "api", "repos/{}/pulls/{}/comments".format(repo, pr)],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
    if rc.returncode == 0 and rc.stdout.strip():
        for c in json.loads(rc.stdout):
            emit({"kind": "review", "target": int(pr), "id": str(c["id"]), "author": c["user"]["login"],
                  "created_at": c["created_at"], "url": c["html_url"], "body": c["body"]})
PY
}

discover_prs() {
  ensure_scope
  cat "${SCOPE_PRS_FILE}" 2>/dev/null || true
}

# 未 mark-handled 的 pending 事件（stdout 每行一条 JSON）
pending_events() {
  while IFS= read -r line; do
    [[ -z "${line}" ]] && continue
    local id
    id="$(printf '%s' "${line}" | python3 -c 'import json,sys; print(json.loads(sys.stdin.read())["comment_id"])')"
    grep -Fxq "${id}" "${HANDLED_FILE}" 2>/dev/null || echo "${line}"
  done < "${EVENTS_FILE}"
}

count_pending() {
  pending_events | wc -l | tr -d ' '
}

# 对单条 pending 事件打印 triage 提示（stdout）
print_triage_for_event() {
  local line="$1"
  printf '%s' "${line}" | python3 -c '
import json, sys
e = json.loads(sys.stdin.read())
repo, epic = sys.argv[1], sys.argv[2]
kind, target = e["kind"], e["target"]
if kind == "pr_state":
    preview = e.get("preview", "")
    print("DISPATCH PR #{} STATE-CHANGE: {} (epic #{})".format(target, preview, epic))
    if "MERGED" in preview:
        print("  action: merge-followup → 勾选 checklist、清理 wt、更新/解锁后续 Issue、标记开工、Task 派执行 Agent")
        print("  cmd: ./scripts/epic/epic.sh merge-followup --epic {} --pr {} --json".format(epic, target))
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
}

resolve_dispatch_hook() {
  if [[ -n "${DISPATCH_HOOK:-}" ]]; then
    echo "${DISPATCH_HOOK}"
    return 0
  fi
  if [[ -x "${SCRIPT_DIR}/dispatch-hook.sh" ]]; then
    echo "${SCRIPT_DIR}/dispatch-hook.sh"
    return 0
  fi
  echo ""
}

# 对单条事件注入环境变量并调用 DISPATCH_HOOK
run_dispatch_hook() {
  local line="$1" hook
  hook="$(resolve_dispatch_hook)"
  [[ -n "${hook}" ]] || return 2

  eval "$(printf '%s' "${line}" | python3 -c '
import json, shlex, sys
e = json.loads(sys.stdin.read())
fields = {
    "COMMENT_ID": e["comment_id"],
    "KIND": e["kind"],
    "TARGET": str(e["target"]),
    "ACTION": e.get("action", ""),
    "URL": e.get("url", ""),
    "AUTHOR": e.get("author", ""),
    "PREVIEW": e.get("preview", "")[:500],
    "EVENT_JSON": json.dumps(e, ensure_ascii=False),
}
for k, v in fields.items():
    print("export {}={}".format(k, shlex.quote(v)))
')"
  "${hook}"
}

# 编排 Agent 唤醒信号（Cursor 等 IDE 可对 stdout 做 notify_on_output 匹配 ^AGENT_TRIGGER_EPIC）
notify_agent_trigger() {
  local new_count="${1:-0}" pending_count="${2:-0}"
  [[ "${AGENT_NOTIFY:-true}" != false ]] || return 0
  local payload
  payload="$(NEW="${new_count}" PENDING="${pending_count}" EPIC="${EPIC}" REPO="${REPO}" python3 -c '
import json, os
epic, repo = os.environ["EPIC"], os.environ["REPO"]
new, pending = int(os.environ["NEW"]), int(os.environ["PENDING"])
print(json.dumps({
    "epic": int(epic),
    "repo": repo,
    "new": new,
    "pending": pending,
    "prompt": (
        "Epic #{} 有 {} 条待处理 GitHub 事件（本轮新增 {}）。"
        "编排 Agent：inbox --json 后 Task 派执行 Agent 做 AddressFeedback（勿亲自改代码）；"
        "子 Agent 完成后 mark-handled。".format(epic, pending, new, epic)
    ),
    "cmd": "./scripts/epic/epic.sh inbox --epic {} --json".format(epic),
}, ensure_ascii=False))')"
  echo "AGENT_TRIGGER_EPIC${EPIC} ${payload}"
  log "agent trigger new=${new_count} pending=${pending_count}"
}

# inbox：为编排 Agent 输出 pending 事件（含可执行 prompt）
cmd_inbox() {
  require_epic
  local wait_sec=0 json_mode=false
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --json) json_mode=true; shift ;;
      --wait) wait_sec="${2:-60}"; shift 2 ;;
      *) die "inbox 用法：inbox [--json] [--wait <秒>]" ;;
    esac
  done

  emit_inbox() {
    local line
    while IFS= read -r line; do
      [[ -z "${line}" ]] && continue
      if [[ "${json_mode}" == true ]]; then
        enrich_inbox_event "${line}"
      else
        print_triage_for_event "${line}"
      fi
    done < <(pending_events)
  }

  if [[ "${wait_sec}" -gt 0 ]]; then
    local deadline now pending
    deadline=$(( $(date +%s) + wait_sec ))
    while true; do
      now="$(date +%s)"
      if [[ "${now}" -ge "${deadline}" ]]; then
        echo "INBOX_TIMEOUT epic=#${EPIC} wait=${wait_sec}s" >&2
        return 1
      fi
      QUIET=true cmd_poll_once >/dev/null 2>&1 || true
      pending="$(count_pending)"
      if [[ "${pending}" -gt 0 ]]; then
        emit_inbox
        notify_agent_trigger 0 "${pending}"
        return 0
      fi
      sleep 5
    done
  fi

  pending="$(count_pending)"
  if [[ "${pending}" -eq 0 ]]; then
    echo "NO_PENDING_EVENTS epic=#${EPIC}"
  else
    emit_inbox
    echo "INBOX_COUNT=${pending} epic=#${EPIC}"
  fi
}

cmd_dispatch() {
  require_epic
  local hook handled=0 deferred=0 failed=0 total=0
  hook="$(resolve_dispatch_hook)"
  local line id rc
  while IFS= read -r line; do
    [[ -z "${line}" ]] && continue
    total=$((total + 1))
    id="$(printf '%s' "${line}" | python3 -c 'import json,sys; print(json.loads(sys.stdin.read())["comment_id"])')"
    echo "--- dispatch #${total} (comment ${id}) ---"
    print_triage_for_event "${line}"
    if [[ -z "${hook}" ]]; then
      echo "  (无 DISPATCH_HOOK，仅 triage；配置 epic.env 的 AGENT_CMD 或 DISPATCH_HOOK 以自动处理)"
      deferred=$((deferred + 1))
      continue
    fi
    set +e
    run_dispatch_hook "${line}"
    rc=$?
    set -e
    case "${rc}" in
      0)
        echo "${id}" >> "${HANDLED_FILE}"
        log "dispatch handled comment_id=${id}"
        echo "  ✓ handled (mark-handled ${id})"
        handled=$((handled + 1))
        ;;
      2)
        log "dispatch deferred comment_id=${id} (needs agent)"
        echo "  → deferred (需编排/执行 Agent 处理)"
        deferred=$((deferred + 1))
        ;;
      *)
        log "dispatch failed comment_id=${id} exit=${rc}"
        echo "  ✗ dispatch hook failed (exit ${rc})，下轮重试"
        failed=$((failed + 1))
        ;;
    esac
  done < <(pending_events)

  if [[ "${total}" -eq 0 ]]; then
    echo "NO_PENDING_EVENTS epic=#${EPIC}"
    return 0
  fi
  echo "DISPATCH_SUMMARY epic=#${EPIC} total=${total} handled=${handled} deferred=${deferred} failed=${failed}"
  [[ "${failed}" -eq 0 ]]
}

# 输出 dispatch_prompt（编排 Agent 复制整段派 Task）
#   dispatch-prompt [comment_id]           — pending 事件
#   dispatch-prompt --issue <n>            — 新 Issue Develop（并行派发 / merge-followup 同款）
cmd_dispatch_prompt() {
  require_epic
  if [[ -n "${ISSUE:-}" ]]; then
    local args=(develop --epic "${EPIC}" --repo "${REPO}" --issue "${ISSUE}")
    [[ -n "${PREREQ_ISSUE:-}" ]] && args+=(--prerequisite-issue "${PREREQ_ISSUE}")
    [[ -n "${PREREQ_PR:-}" ]] && args+=(--prerequisite-pr "${PREREQ_PR}")
    [[ -n "${PREREQ_TITLE:-}" ]] && args+=(--prerequisite-title "${PREREQ_TITLE}")
    enrich_dispatch_prompt "${args[@]}"
    return 0
  fi
  local want_id="${1:-}"
  local line cid
  while IFS= read -r line; do
    [[ -z "${line}" ]] && continue
    if [[ -n "${want_id}" ]]; then
      cid="$(printf '%s' "${line}" | python3 -c 'import json,sys; print(json.loads(sys.stdin.read())["comment_id"])')"
      [[ "${cid}" == "${want_id}" ]] || continue
    fi
    load_env
    wt_cfg 2>/dev/null || true
    export WT_ROOT_ABS="${WT_ROOT_ABS:-..}"
    local kind target cid url preview
    kind="$(printf '%s' "${line}" | python3 -c 'import json,sys; print(json.load(sys.stdin).get("kind",""))')"
    target="$(printf '%s' "${line}" | python3 -c 'import json,sys; print(json.load(sys.stdin).get("target",""))')"
    cid="$(printf '%s' "${line}" | python3 -c 'import json,sys; print(json.load(sys.stdin).get("comment_id",""))')"
    url="$(printf '%s' "${line}" | python3 -c 'import json,sys; print(json.load(sys.stdin).get("url",""))')"
    preview="$(printf '%s' "${line}" | python3 -c 'import json,sys; print(json.load(sys.stdin).get("preview",""))')"
    enrich_dispatch_prompt feedback \
      --epic "${EPIC}" --repo "${REPO}" --kind "${kind}" --target "${target}" \
      --comment-id "${cid}" --url "${url}" --preview "${preview}"
    return 0
  done < <(pending_events)
  die "无匹配 pending 事件（comment_id=${want_id:-任意}）"
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
    event="$(printf '%s' "${row}" | python3 -c 'import json,sys
r = json.loads(sys.stdin.read())
d = {k: r[k] for k in ("kind", "target", "id", "author", "created_at", "url")}
d["comment_id"] = r["id"]
d["preview"] = r["body"].replace(chr(10), " ")[:200]
d["action"] = "AddressFeedback"
d["epic"] = '"${EPIC}"'
print(json.dumps(d, ensure_ascii=False))')"
    if [[ -z "${event}" ]]; then
      log "WARN event build failed id=${id} (kept unseen for retry)"
      sed -i "/^${id}\$/d" "${STATE_FILE}" 2>/dev/null || true
      continue
    fi
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
    [[ "${QUIET:-false}" == true ]] || echo "{\"poll\":true,\"epic\":${EPIC},\"new_events\":${new_count}}" >&2
  fi
  return 0
}

cmd_poll() {
  require_epic
  # 单实例守卫：已有活跃 poll-loop 时拒绝再起一个，避免多个轮询并发全量扫描打爆 GitHub 限流。
  if [[ -f "${PID_FILE}" ]]; then
    local existing; existing="$(cat "${PID_FILE}" 2>/dev/null || true)"
    if [[ -n "${existing}" ]] && kill -0 "${existing}" 2>/dev/null; then
      die "已有 poll-loop 在运行（pid ${existing}, epic #${EPIC}）；先 ./scripts/epic/epic.sh stop --epic ${EPIC} 再启动"
    fi
    rm -f "${PID_FILE}"  # 陈旧 pid 文件（进程已死），清理后继续
  fi
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
    local new count pending ts
    new="$(cmd_poll_once 2>/dev/null || true)"
    count="$(printf '%s\n' "${new}" | grep -c '"comment_id"' || true)"
    pending="$(count_pending)"
    ts="[$(date -u +%H:%M:%S)]"
    if [[ "${count}" -gt 0 ]]; then
      echo "${ts} ACTIONABLE x${count} (pending total=${pending})"
      printf '%s\n' "${new}" | grep '"comment_id"' | sed 's/^/    /'
      notify_agent_trigger "${count}" "${pending}"
      if [[ "${AUTO_DISPATCH}" != false ]]; then
        echo "${ts} auto-dispatch ..."
        cmd_dispatch || true
      fi
    elif [[ "${pending}" -gt 0 ]]; then
      notify_agent_trigger 0 "${pending}"
      if [[ "${DISPATCH_RETRY_PENDING}" != false && "${AUTO_DISPATCH}" != false ]]; then
        echo "${ts} retry dispatch (${pending} pending unhandled)"
        cmd_dispatch || true
      elif [[ "${pending}" -gt 0 ]]; then
        echo "${ts} pending=${pending} (agent notified; run inbox --json to handle)"
      fi
    else
      echo "${ts} idle (no new, no pending)"
    fi
    sleep "${INTERVAL}"
  done
}

cmd_triage() {
  require_epic
  local count=0 line
  while IFS= read -r line; do
    [[ -z "${line}" ]] && continue
    count=$((count + 1))
    print_triage_for_event "${line}"
  done < <(pending_events)
  if [[ "${count}" -eq 0 ]]; then
    echo "NO_PENDING_EVENTS epic=#${EPIC}"
  else
    echo "PENDING_COUNT=${count} epic=#${EPIC}"
  fi
}

cmd_events() {
  require_epic
  local action="${1:-list}"
  case "${action}" in
    list) pending_events ;;
    count) count_pending ;;
    mark-handled) [[ -n "${2:-}" ]] || die "mark-handled 需要 <comment_id>"; echo "$2" >> "${HANDLED_FILE}"; log "marked handled $2" ;;
    *) die "events 用法：list | count | mark-handled <comment_id>" ;;
  esac
}

cmd_scope() {
  require_epic
  refresh_scope   # scope 子命令强制刷新缓存（其余路径走 TTL 缓存）
  echo "Epic #${EPIC} (${REPO})"
  echo "Sub-issues:"; sed 's/^/  #/' "${SCOPE_SUBS_FILE}"
  echo "PRs:"; if [[ -s "${SCOPE_PRS_FILE}" ]]; then sed 's/^/  #/' "${SCOPE_PRS_FILE}"; else echo "  (none)"; fi
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

cmd_label_sync() {
  require_epic
  ensure_scope
  gh label create in-progress --repo "${REPO}" --color fbca04 --description "执行 Agent 已接单开发" 2>/dev/null || true
  local n state labels has_open_pr
  while IFS= read -r n; do
    [[ -n "${n}" ]] || continue
    state="$(gh issue view "${n}" --repo "${REPO}" --json state --jq '.state')"
    labels="$(gh issue view "${n}" --repo "${REPO}" --json labels --jq '[.labels[].name] | join("\n")')"
    if [[ "${state}" == "CLOSED" ]]; then
      if [[ "${labels}" == *agent-ready* ]]; then
        gh issue edit "${n}" --repo "${REPO}" --remove-label agent-ready
        echo "#${n} CLOSED → removed agent-ready"
      fi
      if [[ "${labels}" == *in-progress* ]]; then
        gh issue edit "${n}" --repo "${REPO}" --remove-label in-progress
        echo "#${n} CLOSED → removed in-progress"
      fi
    elif [[ "${state}" == "OPEN" ]]; then
      has_open_pr="$(gh pr list --repo "${REPO}" --state open --search "Closes #${n}" --json number --jq 'length' 2>/dev/null || echo 0)"
      if [[ "${has_open_pr:-0}" -gt 0 ]]; then
        [[ "${labels}" == *agent-ready* ]] && gh issue edit "${n}" --repo "${REPO}" --remove-label agent-ready
        [[ "${labels}" != *in-progress* ]] && gh issue edit "${n}" --repo "${REPO}" --add-label in-progress
        echo "#${n} OPEN+PR → in-progress"
      fi
    fi
  done < "${SCOPE_SUBS_FILE}"
}

cmd_reply() {
  : "${PR:?reply 需要 --pr <n>}"
  : "${COMMENT_ID:?reply 需要 --comment-id <id>}"
  if [[ -n "${BODY_FILE:-}" ]]; then BODY="$(cat "${BODY_FILE}")"; fi
  : "${BODY:?reply 需要 --body 或 --body-file}"
  local owner="${REPO%%/*}" repo="${REPO##*/}"
  local rest_id="${COMMENT_ID}" parent quoted full

  # GraphQL node id（IC_...）须先解析为 REST numeric id；否则 issues/comments API 返回 404 JSON
  if [[ "${COMMENT_ID}" == IC_* ]]; then
    rest_id="$(gh api graphql -f query='query($id:ID!){node(id:$id){... on IssueComment{databaseId} ... on PullRequestReviewComment{databaseId}}}' \
      -f id="${COMMENT_ID}" --jq '.data.node.databaseId // empty' 2>/dev/null || true)"
    [[ -n "${rest_id}" ]] || die "无法解析 comment_id ${COMMENT_ID}（GraphQL node → databaseId）"
  fi

  # 统一 footer（子 Agent 漏写时自动补全）
  if [[ "${BODY}" != *'from=agent'* ]]; then
    BODY="${BODY}

---
\`[epic-delivery]\` from=agent role=feedback-handler action=none"
  fi

  if gh api -X POST "repos/${owner}/${repo}/pulls/${PR}/comments/${rest_id}/replies" -f body="${BODY}" >/dev/null 2>&1; then
    echo "replied in review thread (comment ${rest_id})"; return 0
  fi
  if gh api -X POST "repos/${owner}/${repo}/issues/comments/${rest_id}/replies" -f body="${BODY}" >/dev/null 2>&1; then
    echo "replied in conversation thread (comment ${rest_id})"; return 0
  fi

  # conversation 评论无 thread reply API：fallback 为 quote reply（引用原文）
  if [[ "${COMMENT_ID}" == IC_* ]]; then
    parent="$(gh api graphql -f query='query($id:ID!){node(id:$id){... on IssueComment{body} ... on PullRequestReviewComment{body}}}' \
      -f id="${COMMENT_ID}" --jq '.data.node.body // empty' 2>/dev/null || true)"
  else
    parent="$(gh api "repos/${owner}/${repo}/issues/comments/${rest_id}" --jq '.body // empty' 2>/dev/null || true)"
    if [[ -z "${parent}" ]]; then
      parent="$(gh api "repos/${owner}/${repo}/pulls/comments/${rest_id}" --jq '.body // empty' 2>/dev/null || true)"
    fi
  fi
  if [[ -z "${parent}" || "${parent}" == *'"message":"Not Found"'* ]]; then
    die "评论 ${COMMENT_ID} 未找到（勿把 API 404 当引用正文）"
  fi
  quoted="$(printf '%s' "${parent}" | python3 -c 'import sys; print("\n".join("> "+l for l in sys.stdin.read().strip().splitlines() or [""]))')"
  full="${quoted}

${BODY}"
  gh api -X POST "repos/${owner}/${repo}/issues/${PR}/comments" -f body="${full}" --jq '.html_url'
  echo "replied via quote-reply (conversation comment ${COMMENT_ID})"
}

# PR merge 后：勾选 checklist、清理 worktree、解锁后续 Issue、更新上下文、标记开工、输出派工
cmd_merge_followup() {
  require_epic
  [[ -n "${PR:-}" ]] || die "merge-followup 需要 --pr <n>"
  local dry_run=false
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --json) shift ;;
      --dry-run) dry_run=true; shift ;;
      *) die "merge-followup 用法：merge-followup --pr <n> [--json] [--dry-run]" ;;
    esac
  done

  load_env
  wt_cfg 2>/dev/null || true
  export FORK_REMOTE="${FORK_REMOTE:-origin}" UPSTREAM_REMOTE="${UPSTREAM_REMOTE:-upstream}" WT_ROOT_ABS="${WT_ROOT_ABS:-..}"
  export SCRIPT_DIR="${SCRIPT_DIR}"

  EPIC="${EPIC}" REPO="${REPO}" PR="${PR}" DRY_RUN="${dry_run}" python3 - <<'PY'
import json, os, re, subprocess, sys, tempfile, glob

repo = os.environ["REPO"]
epic = int(os.environ["EPIC"])
pr = int(os.environ["PR"])
dry_run = os.environ.get("DRY_RUN", "false") == "true"
fork_remote = os.environ.get("FORK_REMOTE", "origin")
upstream_remote = os.environ.get("UPSTREAM_REMOTE", "upstream")
wt_root = os.environ.get("WT_ROOT_ABS", "..")

def gh_json(cmd):
    r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
    if r.returncode != 0 or not r.stdout.strip():
        return None
    return json.loads(r.stdout)

def run(cmd, check=True):
    if dry_run:
        print("[dry-run] " + " ".join(cmd), file=sys.stderr)
        return ""
    r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
    if check and r.returncode != 0:
        sys.stderr.write(r.stderr or r.stdout)
        sys.exit(1)
    return r.stdout

pr_data = gh_json(["gh", "pr", "view", str(pr), "--repo", repo,
                   "--json", "state,title,body,mergedAt,url,headRefName"])
if not pr_data:
    sys.exit("PR #{} not found".format(pr))
if pr_data.get("state") != "MERGED":
    sys.exit("PR #{} state={} (need MERGED)".format(pr, pr_data.get("state")))

closes = re.findall(r"(?i)closes\s+#(\d+)", pr_data.get("body") or "")
if not closes:
    sys.exit("PR #{} body has no Closes #issue".format(pr))
closed_issue = int(closes[0])

issue_data = gh_json(["gh", "issue", "view", str(closed_issue), "--repo", repo,
                      "--json", "title,body,state,labels"])
if not issue_data:
    sys.exit("Issue #{} not found".format(closed_issue))

step_m = re.search(r"\[(\d+)-([A-Za-z0-9]+)\]", issue_data.get("title") or "")
wt_epic = step_m.group(1) if step_m else None
step_code = step_m.group(2) if step_m else None
step_lower = step_code.lower() if step_code else None
wt_id = "{}-{}".format(wt_epic, step_lower) if wt_epic and step_lower else None
head_branch = pr_data.get("headRefName") or ""

def repo_root():
    return subprocess.run(["git", "rev-parse", "--show-toplevel"],
                          stdout=subprocess.PIPE, universal_newlines=True).stdout.strip()

def git_cmd(args, check=False, cwd=None):
    if dry_run:
        print("[dry-run] git " + " ".join(args), file=sys.stderr)
        return subprocess.CompletedProcess(args, 0, stdout="", stderr="")
    return subprocess.run(["git"] + args, cwd=cwd, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, universal_newlines=True, check=check)

def cleanup_merged_resources(wt_id, head_branch, closed_issue):
    """merge 后立即释放 worktree / 分支 / label 等占用资源。"""
    report = {
        "worktrees_removed": [],
        "branches_local_deleted": [],
        "branches_remote_deleted": [],
        "labels_removed": [],
        "worktree_prune": False,
        "errors": [],
    }
    if dry_run:
        report["dry_run"] = True
        return report

    root = repo_root()
    wt_dirs = set()
    if wt_id:
        for d in glob.glob(os.path.join(wt_root, "wt-{}-*".format(wt_id))):
            if os.path.isdir(d):
                wt_dirs.add(os.path.abspath(d))
    if head_branch:
        r = git_cmd(["worktree", "list", "--porcelain"], cwd=root)
        lines = r.stdout.splitlines()
        i = 0
        while i < len(lines):
            if lines[i].startswith("worktree "):
                wpath = lines[i].split(" ", 1)[1].strip()
                branch = ""
                j = i + 1
                while j < len(lines) and not lines[j].startswith("worktree "):
                    if lines[j].startswith("branch "):
                        branch = lines[j].split(" ", 1)[1].strip().replace("refs/heads/", "")
                    j += 1
                if branch == head_branch:
                    wt_dirs.add(os.path.abspath(wpath))
                i = j
            else:
                i += 1

    for d in sorted(wt_dirs):
        if not os.path.isdir(d):
            continue
        git_cmd(["clean", "-fd"], cwd=d, check=False)
        git_cmd(["clean", "-fdX"], cwd=d, check=False)
        rm = git_cmd(["worktree", "remove", d], cwd=root, check=False)
        if rm.returncode != 0:
            rm = git_cmd(["worktree", "remove", "--force", d], cwd=root, check=False)
        if rm.returncode == 0:
            report["worktrees_removed"].append(d)
        else:
            report["errors"].append("worktree remove {}: {}".format(d, rm.stderr.strip()))

    prn = git_cmd(["worktree", "prune"], cwd=root, check=False)
    report["worktree_prune"] = prn.returncode == 0

    branches = set()
    if head_branch:
        branches.add(head_branch)
    if wt_id:
        br = git_cmd(["branch", "--list", "feat/{}-*".format(wt_id), "--format=%(refname:short)"], cwd=root)
        for b in br.stdout.splitlines():
            b = b.strip()
            if b:
                branches.add(b)

    for b in sorted(branches):
        delb = git_cmd(["branch", "-D", b], cwd=root, check=False)
        if delb.returncode == 0:
            report["branches_local_deleted"].append(b)
        for remote in (fork_remote, upstream_remote):
            p = git_cmd(["push", remote, "--delete", b], cwd=root, check=False)
            if p.returncode == 0:
                report["branches_remote_deleted"].append("{}/{}".format(remote, b))

    labels = [lb["name"] for lb in (issue_data.get("labels") or [])]
    if "in-progress" in labels:
        run(["gh", "issue", "edit", str(closed_issue), "--repo", repo, "--remove-label", "in-progress"])
        report["labels_removed"].append("in-progress")
    if "agent-ready" in labels:
        run(["gh", "issue", "edit", str(closed_issue), "--repo", repo, "--remove-label", "agent-ready"])
        report["labels_removed"].append("agent-ready")

    return report

cleanup_report = cleanup_merged_resources(wt_id, head_branch, closed_issue)

epic_data = gh_json(["gh", "issue", "view", str(epic), "--repo", repo, "--json", "body"])
epic_body = epic_data.get("body") or ""
pat = re.compile(r"- \[ \] #{}(\b)".format(closed_issue))
new_epic_body, n = pat.subn(r"- [x] #{}\1".format(closed_issue), epic_body, count=1)
if n and not dry_run:
    with tempfile.NamedTemporaryFile("w", suffix=".md", delete=False) as fh:
        fh.write(new_epic_body)
        tmp = fh.name
    run(["gh", "issue", "edit", str(epic), "--repo", repo, "--body-file", tmp])
    os.unlink(tmp)

# （worktree / 分支 / label 清理见 cleanup_merged_resources，merge 后立即执行）

subs = [int(x) for x in re.findall(r"- \[[ xX]\] #(\d+)", epic_body)]
sub_meta = {}
for sub in subs:
    sd = gh_json(["gh", "issue", "view", str(sub), "--repo", repo,
                  "--json", "title,body,state,labels"])
    if sd:
        sub_meta[sub] = sd

def parse_blockers(blocked_text):
    parts = re.split(r"[、,/\s]+|\band\b", blocked_text)
    out = []
    for p in parts:
        p = p.strip()
        if not p:
            continue
        m = re.match(r"#?(\d+)\b", p)
        if m:
            out.append(("issue", int(m.group(1))))
            continue
        m = re.search(r"\b([A-C]\d+)\b", p, re.I)
        if m:
            out.append(("step", m.group(1).upper()))
    return out

def step_to_issue(step_code):
    for num, sd in sub_meta.items():
        t = sd.get("title") or ""
        if re.search(r"\[\d+-{}\]".format(re.escape(step_code)), t, re.I):
            return num
    return None

def blocker_satisfied(blocker):
    if blocker[0] == "issue":
        num = blocker[1]
        return sub_meta.get(num, {}).get("state") == "CLOSED"
    num = step_to_issue(blocker[1])
    if num is None:
        return False
    return sub_meta.get(num, {}).get("state") == "CLOSED"

unlocked = []
for sub in subs:
    if sub == closed_issue:
        continue
    sub_data = sub_meta.get(sub)
    if not sub_data or sub_data.get("state") != "OPEN":
        continue
    body = sub_data.get("body") or ""
    blk = re.search(r"(?i)blocked by[:\s]*([^\n]+)", body)
    if not blk:
        continue
    blockers = parse_blockers(blk.group(1))
    if not blockers:
        continue
    if not all(blocker_satisfied(b) for b in blockers):
        continue
    open_prs = gh_json(["gh", "pr", "list", "--repo", repo, "--state", "open",
                        "--search", "Closes #{}".format(sub), "--json", "number"])
    if open_prs:
        continue
    labels = [lb["name"] for lb in sub_data.get("labels") or []]
    if "in-progress" in labels:
        continue
    sub_step = re.search(r"\[(\d+)-([A-Za-z0-9]+)\]", sub_data.get("title") or "")
    u_wt_epic = sub_step.group(1) if sub_step else wt_epic
    u_step = sub_step.group(2).lower() if sub_step else "step{}".format(sub)
    u_wt_id = "{}-{}".format(u_wt_epic, u_step) if u_wt_epic else None
    unlocked.append({
        "issue": sub,
        "title": sub_data.get("title", ""),
        "wt_id": u_wt_id,
        "wt_epic": u_wt_epic,
        "step": u_step,
    })

pre_summary = (
    "前置 **{}**（Issue #{}, PR #{}）已 merge。\n\n"
    "**交付摘要**：{}\n\n"
    "**PR**：{}\n\n"
    "依赖已满足，编排 Agent 已标记开工并派发执行 Agent。"
).format(
    issue_data.get("title", ""),
    closed_issue,
    pr,
    (pr_data.get("title") or "").strip(),
    pr_data.get("url", ""),
)

for u in unlocked:
    comment = (
        "## 前置完成 · 自动解锁\n\n"
        + pre_summary
        + "\n\n---\n`[epic-delivery]` from=agent role=orchestrator action=start-dispatch\n"
    )
    if not dry_run:
        with tempfile.NamedTemporaryFile("w", suffix=".md", delete=False) as fh:
            fh.write(comment)
            tmp = fh.name
        run(["gh", "issue", "comment", str(u["issue"]), "--repo", repo, "--body-file", tmp])
        os.unlink(tmp)
        subprocess.run(["gh", "label", "create", "in-progress", "--repo", repo,
                        "--color", "fbca04",
                        "--description", "执行 Agent 已接单开发"],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        run(["gh", "issue", "edit", str(u["issue"]), "--repo", repo, "--add-label", "in-progress"])
        if "agent-ready" in labels:
            run(["gh", "issue", "edit", str(u["issue"]), "--repo", repo, "--remove-label", "agent-ready"])
    enrich_script = os.path.join(os.environ.get("SCRIPT_DIR", "scripts/epic"), "dispatch-enrich.sh")
    enrich_cmd = [
        enrich_script, "develop",
        "--issue", str(u["issue"]),
        "--epic", str(epic),
        "--repo", repo,
        "--prerequisite-issue", str(closed_issue),
        "--prerequisite-pr", str(pr),
        "--prerequisite-title", (pr_data.get("title") or "").strip(),
        "--json",
    ]
    er = subprocess.run(enrich_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
    if er.returncode == 0 and er.stdout.strip():
        enriched = json.loads(er.stdout)
        u["dispatch_prompt"] = enriched.get("dispatch_prompt", "")
    else:
        u["dispatch_prompt"] = ""
        u["enrich_error"] = (er.stderr or er.stdout or "dispatch-enrich failed")[:300]
    u["dispatch"] = {
        "task": "Issue #{} · Epic #{} · 依赖 #{} 已 merge，立即开工".format(u["issue"], epic, closed_issue),
        "wt_new": "./scripts/epic/epic.sh wt new --id {} --slug <short-desc>".format(u["wt_id"] or "EPIC-STEP"),
        "dispatch_prompt": u.get("dispatch_prompt", ""),
    }

out = {
    "epic": epic,
    "merged_pr": pr,
    "closed_issue": closed_issue,
    "closed_title": issue_data.get("title", ""),
    "head_branch": head_branch,
    "wt_id_cleaned": wt_id,
    "cleanup": cleanup_report,
    "checklist_updated": bool(n),
    "unlocked": unlocked,
    "dispatch_count": len(unlocked),
}
print(json.dumps(out, ensure_ascii=False, indent=2))
PY
}

# ------------------------------------------------------------------ wt (fork-based worktree)
wt_cfg() {
  UPSTREAM_REMOTE="${UPSTREAM_REMOTE:-upstream}"
  FORK_REMOTE="${FORK_REMOTE:-origin}"
  WT_ROOT="${WT_ROOT:-..}"
  [[ -n "${FORK_OWNER:-}" ]] || die "wt 需要 FORK_OWNER（在 scripts/epic/epic.env 设置，如 Takuka0311）"
  git rev-parse --is-inside-work-tree >/dev/null 2>&1 || die "请在主仓 checkout 内执行 wt 命令"
  git remote get-url "${UPSTREAM_REMOTE}" >/dev/null 2>&1 || die "缺少远端 ${UPSTREAM_REMOTE}（主仓）"
  git remote get-url "${FORK_REMOTE}" >/dev/null 2>&1 || die "缺少远端 ${FORK_REMOTE}（fork）"
  REPO_ROOT="$(git rev-parse --show-toplevel)"
  WT_ROOT_ABS="$(cd "${REPO_ROOT}/${WT_ROOT}" 2>/dev/null && pwd || echo "${WT_ROOT}")"
  if [[ -z "${BASE}" ]]; then
    BASE="$(git symbolic-ref --quiet --short "refs/remotes/${UPSTREAM_REMOTE}/HEAD" 2>/dev/null | sed "s#^${UPSTREAM_REMOTE}/##")"
    BASE="${BASE:-main}"
  fi
}

wt_resolve_id() {
  if [[ -n "${ID}" ]]; then echo "${ID}"; return; fi
  [[ -n "${EPIC}" && -n "${STEP}" ]] || die "wt 需要 --id <epic>-<step>，或 --epic + --step"
  echo "${EPIC}-${STEP}"
}

wt_dir_for() {  # 按 id 找已存在 worktree 目录
  local id="$1"
  ls -d "${WT_ROOT_ABS}"/wt-"${id}"-* 2>/dev/null | head -1
}

wt_branch_for() {  # 按 id 找已存在本地分支
  local id="$1"
  git branch --list "feat/${id}-*" --format='%(refname:short)' 2>/dev/null | head -1
}

wt_new() {
  local id branch dir
  id="$(wt_resolve_id)"
  [[ -n "${SLUG}" ]] || die "wt new 需要 --slug <短描述>"
  branch="feat/${id}-${SLUG}"
  dir="${WT_ROOT_ABS}/wt-${id}-${SLUG}"
  [[ -e "${dir}" ]] && die "目录已存在：${dir}"
  git show-ref --verify --quiet "refs/heads/${branch}" && die "分支已存在：${branch}"
  echo "[wt new] git fetch ${UPSTREAM_REMOTE} ..."
  git fetch "${UPSTREAM_REMOTE}" --quiet
  git worktree add "${dir}" -b "${branch}" "${UPSTREAM_REMOTE}/${BASE}"
  echo "✓ worktree : ${dir}"
  echo "✓ branch   : ${branch}（base=${UPSTREAM_REMOTE}/${BASE}）"
  echo "  检索键   : ${id}"
  echo "  下一步   : cd ${dir} 开发；完成后 ./scripts/epic/epic.sh wt push --id ${id}"
}

wt_push() {
  local id dir branch local_head fork_head
  id="$(wt_resolve_id)"
  dir="$(wt_dir_for "${id}")"; [[ -n "${dir}" ]] || die "未找到 worktree（id=${id}）"
  branch="$(git -C "${dir}" rev-parse --abbrev-ref HEAD)"
  echo "[wt push] ${branch} -> ${FORK_REMOTE}（fork）"
  git -C "${dir}" push -u "${FORK_REMOTE}" "${branch}"
  local_head="$(git -C "${dir}" rev-parse HEAD)"
  fork_head="$(git -C "${dir}" rev-parse "${FORK_REMOTE}/${branch}" 2>/dev/null || true)"
  [[ "${local_head}" == "${fork_head}" ]] \
    || die "推送后 ${FORK_REMOTE}/${branch} head 与本地 HEAD 不一致，请检查"
  echo "✓ ${FORK_REMOTE}/${branch} head == 本地 HEAD (${local_head:0:12})"
}

wt_pr() {
  local id dir branch
  id="$(wt_resolve_id)"
  dir="$(wt_dir_for "${id}")"; [[ -n "${dir}" ]] || die "未找到 worktree（id=${id}）"
  branch="$(git -C "${dir}" rev-parse --abbrev-ref HEAD)"
  git -C "${dir}" rev-parse "${FORK_REMOTE}/${branch}" >/dev/null 2>&1 \
    || die "分支未推送到 fork，请先：./scripts/epic/epic.sh wt push --id ${id}"
  local args=(--repo "${REPO}" --head "${FORK_OWNER}:${branch}" --base "${BASE}")
  [[ "${DRAFT}" == true ]] && args+=(--draft)
  [[ -n "${TITLE}" ]] && args+=(--title "${TITLE}")
  [[ -n "${BODY_FILE:-}" ]] && args+=(--body-file "${BODY_FILE}")
  echo "[wt pr] head=${FORK_OWNER}:${branch} base=${BASE} repo=${REPO}"
  ( cd "${dir}" && gh pr create "${args[@]}" )
}

wt_rm() {
  local id dir branch
  id="$(wt_resolve_id)"
  dir="$(wt_dir_for "${id}")"
  branch="$(wt_branch_for "${id}")"
  [[ -n "${dir}" || -n "${branch}" ]] || die "未找到 worktree/分支（id=${id}）"
  if [[ -n "${dir}" ]]; then
    git -C "${dir}" clean -fd .tmp 2>/dev/null || true
    git -C "${dir}" clean -fd 2>/dev/null || true
    if git worktree remove "${dir}" 2>/dev/null; then
      echo "✓ removed worktree ${dir}"
    elif git worktree remove --force "${dir}"; then
      echo "✓ removed worktree ${dir} (force)"
    else
      die "无法移除 worktree：${dir}"
    fi
    git worktree prune && echo "✓ worktree prune"
  fi
  if [[ -n "${branch}" ]]; then
    git branch -D "${branch}" && echo "✓ deleted local branch ${branch}"
    for remote in "${FORK_REMOTE}" "${UPSTREAM_REMOTE}"; do
      if git push "${remote}" --delete "${branch}" 2>/dev/null; then
        echo "✓ deleted ${remote} branch ${branch}"
      else
        echo "  (${remote} 无该分支或已删)"
      fi
    done
  fi
}

wt_ls() {
  echo "工作单元（${WT_ROOT_ABS}/wt-*）："
  local d b
  for d in "${WT_ROOT_ABS}"/wt-*; do
    [[ -d "${d}" ]] || continue
    b="$(git -C "${d}" rev-parse --abbrev-ref HEAD 2>/dev/null || echo '?')"
    printf "  %-34s %s\n" "$(basename "${d}")" "${b}"
  done
}

cmd_wt() {
  local action="${1:-}"
  wt_cfg
  case "${action}" in
    new)  wt_new ;;
    push) wt_push ;;
    pr)   wt_pr ;;
    rm)   wt_rm ;;
    ls)   wt_ls ;;
    *)    die "wt 用法：new|push|pr|rm|ls（详见脚本头注释）" ;;
  esac
}

# ------------------------------------------------------------------ dispatch
[[ $# -ge 1 ]] || { usage; exit 1; }
SUBCMD="$1"; shift

EPIC=""; REPO=""; STATE_DIR=""; INTERVAL=""; PR=""; COMMENT_ID=""; BODY=""; BODY_FILE=""; INIT=false
STEP=""; SLUG=""; ID=""; BASE=""; DRAFT=false; TITLE=""; NO_AUTO_DISPATCH=false
ISSUE=""; PREREQ_ISSUE=""; PREREQ_PR=""; PREREQ_TITLE=""
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
    --no-auto-dispatch) NO_AUTO_DISPATCH=true; shift ;;
    --step) STEP="$2"; shift 2 ;;
    --slug) SLUG="$2"; shift 2 ;;
    --id) ID="$2"; shift 2 ;;
    --base) BASE="$2"; shift 2 ;;
    --draft) DRAFT=true; shift ;;
    --title) TITLE="$2"; shift 2 ;;
    --issue) ISSUE="$2"; shift 2 ;;
    --prerequisite-issue) PREREQ_ISSUE="$2"; shift 2 ;;
    --prerequisite-pr) PREREQ_PR="$2"; shift 2 ;;
    --prerequisite-title) PREREQ_TITLE="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    --) shift; REST+=("$@"); break ;;
    *) REST+=("$1"); shift ;;
  esac
done

load_env
[[ "${NO_AUTO_DISPATCH}" == true ]] && AUTO_DISPATCH=false

case "${SUBCMD}" in
  poll)       cmd_poll ;;
  poll-once)  cmd_poll_once ;;
  triage)     cmd_triage ;;
  dispatch)   cmd_dispatch ;;
  inbox)      cmd_inbox ${REST[@]+"${REST[@]}"} ;;
  events)     cmd_events ${REST[@]+"${REST[@]}"} ;;
  scope)      cmd_scope ;;
  reply)      cmd_reply ;;
  stop)       cmd_stop ;;
  merge-followup) cmd_merge_followup ${REST[@]+"${REST[@]}"} ;;
  label-sync)   cmd_label_sync ;;
  dispatch-prompt) cmd_dispatch_prompt ${REST[@]+"${REST[@]}"} ;;
  wt)         cmd_wt ${REST[@]+"${REST[@]}"} ;;
  -h|--help|help) usage ;;
  *) die "未知子命令：${SUBCMD}（poll|poll-once|triage|dispatch|inbox|events|scope|reply|stop|merge-followup|label-sync|dispatch-prompt|wt）" ;;
esac
