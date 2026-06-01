#!/usr/bin/env python3
import argparse
import json
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, Tuple


SCRIPT_DIR = Path(__file__).resolve().parent
SKILL_DIR = SCRIPT_DIR.parent
REF_DIR = SKILL_DIR / "references"


def utc_now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat()


def read_json(path: Path) -> Dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, data: Dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def ensure_file_from_template(target: Path, template: Path, mutate=None) -> None:
    if target.exists():
        return
    payload = read_json(template)
    if mutate:
        mutate(payload)
    write_json(target, payload)


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


def main() -> None:
    parser = argparse.ArgumentParser(description="Initialize code-review workspace for PR or branch.")
    parser.add_argument("--repo-root", required=True, help="Repository root path")
    parser.add_argument("--target-type", choices=["pr", "branch"], help="Review target type")
    parser.add_argument("--target-id", help="Review target id (PR number or branch name)")
    parser.add_argument("--pr-number", type=int, help="PR number (legacy compatible)")
    parser.add_argument("--branch-name", help="Branch name (legacy compatible)")
    parser.add_argument("--base-ref", default="", help="PR base ref")
    parser.add_argument("--head-ref", default="", help="PR head ref")
    parser.add_argument("--base-sha", default="", help="PR base sha")
    parser.add_argument("--head-sha", default="", help="PR head sha")
    args = parser.parse_args()

    repo_root = Path(args.repo_root).resolve()
    target_type, target_id_raw = resolve_target(args)
    target_id_dir = sanitize_branch_name(target_id_raw) if target_type == "branch" else target_id_raw
    review_dir = repo_root / "code-review" / f"{target_type}-{target_id_dir}"
    comments_dir = review_dir / "comments"

    comments_dir.mkdir(parents=True, exist_ok=True)

    meta_path = review_dir / "meta.json"
    reviewed_commits_path = review_dir / "reviewed_commits.json"
    review_comments_path = comments_dir / "review-comments.json"
    comment_status_path = comments_dir / "comment-status.json"

    def mutate_meta(payload: Dict[str, Any]) -> None:
        payload["repo"] = str(repo_root)
        payload["review_target"]["type"] = target_type
        payload["review_target"]["id"] = target_id_raw
        payload["review_target"]["base_ref"] = args.base_ref
        payload["review_target"]["head_ref"] = args.head_ref
        payload["review_target"]["base_sha"] = args.base_sha
        payload["review_target"]["head_sha"] = args.head_sha
        payload["generated_at"] = utc_now()

    def create_review_comments_payload() -> Dict[str, Any]:
        return {
            "version": "1.0",
            "source": "github_pr_review_comments" if target_type == "pr" else "branch_review_comments",
            "fetched_at": utc_now(),
            "review_target": {"type": target_type, "id": target_id_raw},
            "viewer_login": "",
            "comments": [],
        }

    def mutate_comment_status(payload: Dict[str, Any]) -> None:
        payload["review_target"]["type"] = target_type
        payload["review_target"]["id"] = target_id_raw
        payload["generated_at"] = utc_now()
        payload["status"] = []

    def mutate_reviewed_commits(payload: Dict[str, Any]) -> None:
        payload["review_rounds"] = []
        payload["commits"] = []

    ensure_file_from_template(meta_path, REF_DIR / "meta.template.json", mutate_meta)
    ensure_file_from_template(
        reviewed_commits_path, REF_DIR / "reviewed_commits.template.json", mutate_reviewed_commits
    )
    if not review_comments_path.exists():
        write_json(review_comments_path, create_review_comments_payload())
    ensure_file_from_template(
        comment_status_path, REF_DIR / "comment-status.template.json", mutate_comment_status
    )

    print(str(review_dir))


if __name__ == "__main__":
    main()
