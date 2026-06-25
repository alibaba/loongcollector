#!/usr/bin/env bash
# Shared helpers for Epic comment polling.
set -euo pipefail

epic_die() {
  echo "epic-poll: $*" >&2
  exit 1
}

epic_usage() {
  cat <<EOF
Usage: $(basename "$0") --epic <issue_number> [--repo owner/repo] [--interval seconds] [--state-dir path]

  --epic        Epic Issue 编号（必填；脚本据此自动发现子 Issue 与关联 PR）
  --repo        仓库，默认 epic.env 或 gh repo view
  --interval    轮询间隔秒数，默认 60
  --state-dir   状态目录，默认 /tmp/epic-<epic>-poll
EOF
}

# Parse global flags shared by all epic scripts. Sets EPIC_ARGV to remaining args.
epic_parse_global_args() {
  EPIC=""
  REPO=""
  STATE_DIR=""
  INTERVAL=""
  EPIC_ARGV=()
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --epic) EPIC="$2"; shift 2 ;;
      --repo) REPO="$2"; shift 2 ;;
      --state-dir) STATE_DIR="$2"; shift 2 ;;
      --interval) INTERVAL="$2"; shift 2 ;;
      -h|--help) epic_usage; exit 0 ;;
      --) shift; EPIC_ARGV=("$@"); return 0 ;;
      -*) epic_die "unknown option: $1 (try --help)" ;;
      *) EPIC_ARGV+=("$1"); shift ;;
    esac
  done
  epic_load_env
}

epic_load_env() {
  local script_dir
  script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
  if [[ -f "${script_dir}/epic.env" ]]; then
    # shellcheck disable=SC1091
    source "${script_dir}/epic.env"
  fi
  if [[ -z "${REPO:-}" ]]; then
    REPO="$(gh repo view --json nameWithOwner --jq '.nameWithOwner' 2>/dev/null || true)"
  fi
  : "${REPO:?REPO must be set via --repo, epic.env, or gh repo view}"
  : "${EPIC:?--epic <issue_number> is required}"
  : "${STATE_DIR:=/tmp/epic-${EPIC}-poll}"
  : "${EVENTS_FILE:=${STATE_DIR}/events.jsonl}"
  : "${STATE_FILE:=${STATE_DIR}/seen-ids.txt}"
  : "${LOG_FILE:=${STATE_DIR}/poll.log}"
  : "${INTERVAL:=60}"
  : "${PID_FILE:=${STATE_DIR}/poll-loop.pid}"
  mkdir -p "${STATE_DIR}"
  touch "${EVENTS_FILE}" "${LOG_FILE}" "${STATE_FILE}"
}

epic_log() {
  echo "$(date -u +%Y-%m-%dT%H:%M:%SZ) [epic=${EPIC}] $*" >> "${LOG_FILE}"
}

epic_is_actionable_body() {
  local body="$1"
  local non_quoted
  # 仅看非引用行：避免人工评论「引用」了 Agent 带 footer 的回复而被误判为 Agent 信息
  non_quoted="$(printf '%s\n' "${body}" | grep -v '^[[:space:]]*>' || true)"
  if [[ "${non_quoted}" == *'from=agent'* ]]; then
    if [[ "${non_quoted}" == *'action=none'* ]] || [[ "${non_quoted}" == *'action=fyi'* ]]; then
      return 1
    fi
  fi
  return 0
}

epic_seen_id() {
  local id="$1"
  grep -Fxq "${id}" "${STATE_FILE}" 2>/dev/null
}

epic_mark_seen() {
  local id="$1"
  echo "${id}" >> "${STATE_FILE}"
}

epic_discover_sub_issues() {
  local body
  body="$(gh issue view "${EPIC}" --repo "${REPO}" --json body --jq '.body')"
  printf '%s\n' "${body}" | grep -E '^- \[[ xX]\] #[0-9]+' | grep -oE '#[0-9]+' | tr -d '#' | sort -un
}

epic_discover_prs() {
  python3 - "${REPO}" "${EPIC}" <<'PY'
import json, re, subprocess, sys

repo, epic = sys.argv[1], sys.argv[2]
body = subprocess.check_output(
    ["gh", "issue", "view", epic, "--repo", repo, "--json", "body", "--jq", ".body"],
    universal_newlines=True,
)
issues = sorted(set(re.findall(r"^- \[[ xX]\] #(\d+)", body, re.M)))
prs = set()
for n in issues:
    out = subprocess.run(
        ["gh", "pr", "list", "--repo", repo, "--state", "all", "--limit", "100",
         "--search", "Closes #{}".format(n), "--json", "number", "--jq", ".[].number"],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True,
    )
    for line in out.stdout.splitlines():
        line = line.strip()
        if line.isdigit():
            prs.add(int(line))
    icom = subprocess.run(
        ["gh", "issue", "view", str(n), "--repo", repo, "--json", "comments", "--jq", ".comments[].body"],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True,
    )
    for m in re.finditer(r"#(\d+)", icom.stdout):
        cand = m.group(1)
        chk = subprocess.run(
            ["gh", "pr", "view", cand, "--repo", repo, "--json", "number", "--jq", ".number"],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True,
        )
        if chk.returncode == 0 and chk.stdout.strip().isdigit():
            prs.add(int(chk.stdout.strip()))
for pr in sorted(prs):
    print(pr)
PY
}

# stdout: JSON lines {kind,target,id,author,created_at,url,body}
epic_scan_all_comments() {
  python3 - "${REPO}" "${EPIC}" <<'PY'
import json, re, subprocess, sys

repo, epic = sys.argv[1], sys.argv[2]

def gh_json(cmd):
    r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
    if r.returncode != 0:
        return None
    return json.loads(r.stdout) if r.stdout.strip() else None

body = subprocess.check_output(
    ["gh", "issue", "view", epic, "--repo", repo, "--json", "body", "--jq", ".body"],
    universal_newlines=True,
)
issues = sorted(set(re.findall(r"^- \[[ xX]\] #(\d+)", body, re.M)))
targets = [int(epic)] + [int(x) for x in issues]

for n in targets:
    data = gh_json(["gh", "issue", "view", str(n), "--repo", repo, "--json", "comments"])
    if not data:
        continue
    for c in data.get("comments", []):
        print(json.dumps({
            "kind": "issue",
            "target": n,
            "id": c["id"],
            "author": c["author"]["login"],
            "created_at": c["createdAt"],
            "url": c["url"],
            "body": c["body"],
        }, ensure_ascii=False))

prs = set()
for n in issues:
    out = subprocess.run(
        ["gh", "pr", "list", "--repo", repo, "--state", "all", "--limit", "100",
         "--search", "Closes #{}".format(n), "--json", "number"],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True,
    )
    if out.returncode == 0 and out.stdout.strip():
        for row in json.loads(out.stdout):
            prs.add(row["number"])
    icom = subprocess.run(
        ["gh", "issue", "view", str(n), "--repo", repo, "--json", "comments"],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True,
    )
    if icom.returncode == 0:
        data = json.loads(icom.stdout)
        for c in data.get("comments", []):
            for m in re.finditer(r"#(\d+)", c.get("body", "")):
                cand = m.group(1)
                chk = subprocess.run(
                    ["gh", "pr", "view", cand, "--repo", repo, "--json", "number"],
                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True,
                )
                if chk.returncode == 0:
                    prs.add(json.loads(chk.stdout)["number"])

for pr in sorted(prs):
    data = gh_json(["gh", "pr", "view", str(pr), "--repo", repo, "--json", "comments"])
    if data:
        for c in data.get("comments", []):
            print(json.dumps({
                "kind": "pr",
                "target": pr,
                "id": c["id"],
                "author": c["author"]["login"],
                "created_at": c["createdAt"],
                "url": c["url"],
                "body": c["body"],
            }, ensure_ascii=False))
    rc = subprocess.run(
        ["gh", "api", "repos/{}/pulls/{}/comments".format(repo, pr)],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True,
    )
    if rc.returncode == 0 and rc.stdout.strip():
        for c in json.loads(rc.stdout):
            print(json.dumps({
                "kind": "review",
                "target": pr,
                "id": str(c["id"]),
                "author": c["user"]["login"],
                "created_at": c["created_at"],
                "url": c["html_url"],
                "body": c["body"],
            }, ensure_ascii=False))
PY
}

epic_poll_loop_running() {
  [[ -f "${PID_FILE}" ]] || return 1
  local pid
  pid="$(cat "${PID_FILE}")"
  [[ -n "${pid}" ]] && kill -0 "${pid}" 2>/dev/null
}
