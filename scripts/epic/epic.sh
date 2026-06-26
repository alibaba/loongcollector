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
    echo "{\"poll\":true,\"epic\":${EPIC},\"new_events\":${new_count}}" >&2
  fi
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
    local new count ts
    new="$(cmd_poll_once 2>/dev/null || true)"
    count="$(printf '%s\n' "${new}" | grep -c '"comment_id"' || true)"
    ts="[$(date -u +%H:%M:%S)]"
    if [[ "${count}" -gt 0 ]]; then
      echo "${ts} ACTIONABLE x${count} -> run: ./scripts/epic/epic.sh triage --epic ${EPIC}"
      printf '%s\n' "${new}" | grep '"comment_id"' | sed 's/^/    /'
    else
      echo "${ts} idle, no pending (epic #${EPIC})"
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

cmd_reply() {
  : "${PR:?reply 需要 --pr <n>}"
  : "${COMMENT_ID:?reply 需要 --comment-id <id>}"
  # 安全：先把 --body-file 内容读进 BODY，下面统一用 -f body="${BODY}"（字面值，已是内容）。
  # 切勿对外暴露 -f body=@file 用法——gh 的 -f 会把 @路径 当字面字符串原样发送，
  # 正文会变成路径而非内容；需要直接传文件时必须用 -F body=@file。
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
  if [[ -n "${dir}" ]]; then git worktree remove "${dir}" && echo "✓ removed worktree ${dir}"; fi
  if [[ -n "${branch}" ]]; then
    git branch -D "${branch}" && echo "✓ deleted local branch ${branch}"
    if git push "${FORK_REMOTE}" --delete "${branch}" 2>/dev/null; then
      echo "✓ deleted ${FORK_REMOTE} branch ${branch}"
    else
      echo "  (${FORK_REMOTE} 无该分支或已删)"
    fi
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
STEP=""; SLUG=""; ID=""; BASE=""; DRAFT=false; TITLE=""
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
    --step) STEP="$2"; shift 2 ;;
    --slug) SLUG="$2"; shift 2 ;;
    --id) ID="$2"; shift 2 ;;
    --base) BASE="$2"; shift 2 ;;
    --draft) DRAFT=true; shift ;;
    --title) TITLE="$2"; shift 2 ;;
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
  wt)         cmd_wt ${REST[@]+"${REST[@]}"} ;;
  -h|--help|help) usage ;;
  *) die "未知子命令：${SUBCMD}（poll|poll-once|triage|events|scope|reply|stop|wt）" ;;
esac
