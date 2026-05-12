#!/usr/bin/env python3
import argparse
import json
import subprocess
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List, Tuple


def utc_now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat()


def run_cmd(args: List[str], cwd: Path) -> str:
    proc = subprocess.run(args, cwd=cwd, text=True, capture_output=True, check=False)
    if proc.returncode != 0:
        raise SystemExit(f"command failed: {' '.join(args)}\n{proc.stderr.strip()}")
    return proc.stdout


def sanitize_branch_name(branch_name: str) -> str:
    return branch_name.replace("/", "-")


def resolve_target(args: argparse.Namespace) -> Tuple[str, str]:
    if args.target_type and args.target_id:
        target_type = args.target_type
        target_id = args.target_id
    elif args.pr_number is not None:
        target_type = "pr"
        target_id = str(args.pr_number)
    elif args.branch_name:
        target_type = "branch"
        target_id = args.branch_name
    else:
        raise SystemExit("must provide either --target-type/--target-id or --pr-number or --branch-name")
    if target_type not in {"pr", "branch"}:
        raise SystemExit("target type must be pr or branch")
    return target_type, target_id


def parse_name_with_owner(repo_root: Path) -> Tuple[str, str]:
    out = run_cmd(["gh", "repo", "view", "--json", "nameWithOwner", "--jq", ".nameWithOwner"], repo_root).strip()
    if "/" not in out:
        raise SystemExit(f"invalid repository nameWithOwner: {out}")
    owner, name = out.split("/", 1)
    return owner, name


def get_viewer_login(repo_root: Path) -> str:
    out = run_cmd(["gh", "api", "user", "--jq", ".login"], repo_root).strip()
    return out


def run_graphql(repo_root: Path, owner: str, name: str, pr_number: int, cursor: str) -> Dict[str, Any]:
    # Query review threads instead of plain review comments so we can
    # persist thread-level resolution state deterministically.
    query = """
query($owner:String!, $name:String!, $number:Int!, $endCursor:String) {
  repository(owner:$owner, name:$name) {
    pullRequest(number:$number) {
      reviewThreads(first:100, after:$endCursor) {
        pageInfo { hasNextPage endCursor }
        nodes {
          isResolved
          comments(first:100) {
            nodes {
              databaseId
              body
              path
              line
              originalLine
              createdAt
              updatedAt
              author { login }
              originalCommit { oid }
              replyTo { databaseId }
            }
          }
        }
      }
    }
  }
}
"""
    cmd = [
        "gh",
        "api",
        "graphql",
        "-f",
        f"query={query}",
        "-F",
        f"owner={owner}",
        "-F",
        f"name={name}",
        "-F",
        f"number={pr_number}",
    ]
    if cursor:
        cmd.extend(["-F", f"endCursor={cursor}"])
    out = run_cmd(cmd, repo_root)
    return json.loads(out)


def main() -> None:
    parser = argparse.ArgumentParser(description="Fetch PR review comments to stable schema file.")
    parser.add_argument("--repo-root", required=True)
    parser.add_argument("--target-type", choices=["pr", "branch"])
    parser.add_argument("--target-id")
    parser.add_argument("--pr-number", type=int, help="PR number (legacy compatible)")
    parser.add_argument("--branch-name", help="Branch name (legacy compatible)")
    args = parser.parse_args()

    repo_root = Path(args.repo_root).resolve()
    target_type, target_id_raw = resolve_target(args)
    target_id_dir = sanitize_branch_name(target_id_raw) if target_type == "branch" else target_id_raw
    review_dir = repo_root / "code-review" / f"{target_type}-{target_id_dir}"
    comments_path = review_dir / "comments" / "review-comments.json"
    comments_path.parent.mkdir(parents=True, exist_ok=True)

    if target_type != "pr":
        payload = {
            "version": "1.0",
            "source": "branch_review_comments",
            "fetched_at": utc_now(),
            "review_target": {"type": target_type, "id": target_id_raw},
            "comments": [],
        }
        comments_path.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(json.dumps({"target": f"{target_type}-{target_id_raw}", "threads": 0, "comments": 0, "resolved_threads": 0}))
        return

    owner, name = parse_name_with_owner(repo_root)
    viewer_login = get_viewer_login(repo_root)
    pr_number = int(target_id_raw)

    cursor = ""
    has_next = True
    comments: List[Dict[str, Any]] = []
    total_threads = 0
    resolved_threads = 0

    while has_next:
        # Paginate until all review threads are collected.
        data = run_graphql(repo_root, owner, name, pr_number, cursor)
        threads_obj = data["data"]["repository"]["pullRequest"]["reviewThreads"]
        page_info = threads_obj["pageInfo"]
        threads = threads_obj["nodes"] or []
        total_threads += len(threads)
        for thread in threads:
            is_resolved = bool(thread.get("isResolved", False))
            if is_resolved:
                resolved_threads += 1
            thread_comments = thread.get("comments", {}).get("nodes", []) or []
            for c in thread_comments:
                author = (c.get("author") or {}).get("login", "")
                original_commit = (c.get("originalCommit") or {}).get("oid", "")
                reply_to = (c.get("replyTo") or {}).get("databaseId")
                # Use originalLine as a stable anchor because line can be null
                # after code evolves on newer commits.
                original_line = c.get("originalLine")
                line = original_line if isinstance(original_line, int) else 0
                comments.append(
                    {
                        "comment_id": c.get("databaseId"),
                        "author": author,
                        "created_at": c.get("createdAt", ""),
                        "updated_at": c.get("updatedAt", ""),
                        "path": c.get("path", ""),
                        "line": line,
                        "side": "RIGHT",
                        "commit_id": original_commit,
                        "in_reply_to_id": reply_to,
                        "body": c.get("body", ""),
                        "thread_resolved": is_resolved,
                    }
                )
        has_next = bool(page_info.get("hasNextPage"))
        cursor = page_info.get("endCursor") if has_next else ""

    payload = {
        "version": "1.0",
        "source": "github_pr_review_comments",
        "fetched_at": utc_now(),
        "review_target": {"type": "pr", "id": target_id_raw},
        "viewer_login": viewer_login,
        "comments": comments,
    }
    comments_path.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(
        json.dumps(
            {
                "target": f"pr-{target_id_raw}",
                "threads": total_threads,
                "comments": len(comments),
                "resolved_threads": resolved_threads,
            },
            ensure_ascii=False,
        )
    )


if __name__ == "__main__":
    main()
