#!/usr/bin/env python3
import argparse
import json
from pathlib import Path
from typing import Dict, List, Tuple


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


def read_json(path: Path) -> Dict:
    if not path.exists():
        raise SystemExit(f"missing file: {path}")
    return json.loads(path.read_text(encoding="utf-8"))


def esc_cell(text: str) -> str:
    return (text or "").replace("\n", " ").replace("|", "\\|").replace("`", "").strip()


def build_comment_meta_map(review_comments_payload: Dict) -> Dict[int, Dict]:
    comments = review_comments_payload.get("comments", [])
    meta_map: Dict[int, Dict] = {}
    if not isinstance(comments, list):
        return meta_map
    for c in comments:
        cid = c.get("comment_id")
        if isinstance(cid, int):
            meta_map[cid] = c
    return meta_map


def build_markdown(target_type: str, target_id: str, items: List[Dict], comment_meta: Dict[int, Dict]) -> str:
    lines = []
    lines.append(f"# Comment Status Report ({target_type}-{target_id})")
    lines.append("")
    lines.append(f"- Total: {len(items)}")
    lines.append("")
    lines.append("| 评论时间 | File | Line | 作者 | Comment | Flow | Tech |")
    lines.append("|---|---|---:|---|---|---|---|")
    for item in items:
        cid = item.get("comment_id", "")
        meta = comment_meta.get(cid, {})
        created_at = esc_cell(str(meta.get("created_at", "")))
        author = esc_cell(str(meta.get("author", "")))
        path = esc_cell(str(item.get("path", "")))
        line = item.get("line", 0)
        body = esc_cell(str(item.get("body", "")))
        if len(body) > 160:
            body = body[:157] + "..."
        status_flow = esc_cell(str(item.get("status_flow", "")))
        status_tech = esc_cell(str(item.get("status_tech", "")))
        lines.append(
            f"| {created_at} | `{path}` | {line} | {author} | {body} | {status_flow} | {status_tech} |"
        )
    lines.append("")
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate markdown report from comment-status.json.")
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

    status_path = review_dir / "comments" / "comment-status.json"
    review_comments_path = review_dir / "comments" / "review-comments.json"
    report_path = review_dir / "comments" / "comment-status.md"

    payload = read_json(status_path)
    if not isinstance(payload, dict) or not isinstance(payload.get("status"), list):
        raise SystemExit("invalid comment-status.json: root must be object and `status` must be list")
    review_comments_payload = read_json(review_comments_path)
    if not isinstance(review_comments_payload, dict):
        raise SystemExit("invalid review-comments.json: root must be object")

    comment_meta = build_comment_meta_map(review_comments_payload)
    markdown = build_markdown(target_type, target_id_raw, payload["status"], comment_meta)
    report_path.write_text(markdown + "\n", encoding="utf-8")
    print(str(report_path))


if __name__ == "__main__":
    main()
