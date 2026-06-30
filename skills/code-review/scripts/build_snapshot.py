#!/usr/bin/env python3
import argparse
import hashlib
import json
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, List, Set, Tuple


def utc_now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat()


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


def run_git(repo_root: Path, args: List[str]) -> str:
    proc = subprocess.run(["git", *args], cwd=repo_root, text=True, capture_output=True, check=True)
    return proc.stdout


def run_git_no_check(repo_root: Path, args: List[str]) -> subprocess.CompletedProcess:
    return subprocess.run(["git", *args], cwd=repo_root, text=True, capture_output=True, check=False)


def normalize_file_content(text: str) -> str:
    lines = [re.sub(r"\s+", " ", line.strip()) for line in text.splitlines()]
    return "\n".join(lines)


def stable_hash(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def get_changed_files(repo_root: Path, base_sha: str, head_sha: str) -> List[str]:
    out = run_git(repo_root, ["diff", "--name-only", f"{base_sha}..{head_sha}"])
    return sorted({line.strip() for line in out.splitlines() if line.strip()})


def get_file_content_at_commit(repo_root: Path, commit_sha: str, path: str) -> str:
    proc = run_git_no_check(repo_root, ["show", f"{commit_sha}:{path}"])
    if proc.returncode != 0:
        return ""
    return proc.stdout


def main() -> None:
    parser = argparse.ArgumentParser(description="Build snapshot baseline for incremental review.")
    parser.add_argument("--repo-root", required=True)
    parser.add_argument("--target-type", choices=["pr", "branch"])
    parser.add_argument("--target-id")
    parser.add_argument("--pr-number", type=int, help="PR number (legacy compatible)")
    parser.add_argument("--branch-name", help="Branch name (legacy compatible)")
    parser.add_argument("--base", required=True, help="Base commit SHA")
    parser.add_argument("--head", required=True, help="Head commit SHA")
    parser.add_argument("--review-round", required=True, type=int)
    args = parser.parse_args()

    repo_root = Path(args.repo_root).resolve()
    target_type, target_id_raw = resolve_target(args)
    target_id_dir = sanitize_branch_name(target_id_raw) if target_type == "branch" else target_id_raw
    review_dir = repo_root / "code-review" / f"{target_type}-{target_id_dir}"

    snapshot_root = review_dir / "snapshot" / f"round-{args.review_round}"
    files_root = snapshot_root / "files"
    files_root.mkdir(parents=True, exist_ok=True)

    changed_files = get_changed_files(repo_root, args.base, args.head)
    manifest_files: List[Dict[str, object]] = []

    for rel_path in changed_files:
        content = get_file_content_at_commit(repo_root, args.head, rel_path)
        if content == "":
            # Deleted file at head; keep entry for audit but no content snapshot.
            manifest_files.append(
                {"path": rel_path, "exists_in_head": False, "raw_hash": "", "normalized_hash": "", "size": 0}
            )
            continue

        out_path = files_root / rel_path
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_text(content, encoding="utf-8")
        manifest_files.append(
            {
                "path": rel_path,
                "exists_in_head": True,
                "raw_hash": stable_hash(content),
                "normalized_hash": stable_hash(normalize_file_content(content)),
                "size": len(content.encode("utf-8")),
            }
        )

    manifest = {
        "version": "1.0",
        "review_target": {"type": target_type, "id": target_id_raw},
        "review_round": args.review_round,
        "base_sha": args.base,
        "head_sha": args.head,
        "generated_at": utc_now(),
        "files": manifest_files,
    }
    manifest_path = snapshot_root / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    latest = {
        "latest_round": args.review_round,
        "manifest": str(manifest_path.relative_to(review_dir)),
        "updated_at": utc_now(),
    }
    latest_path = review_dir / "snapshot" / "latest.json"
    latest_path.parent.mkdir(parents=True, exist_ok=True)
    latest_path.write_text(json.dumps(latest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    print(
        json.dumps(
            {
                "review_target": {"type": target_type, "id": target_id_raw},
                "review_round": args.review_round,
                "files": len(manifest_files),
                "manifest": str(manifest_path),
            },
            ensure_ascii=False,
        )
    )


if __name__ == "__main__":
    main()
