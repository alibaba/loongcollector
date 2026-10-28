#!/usr/bin/env python3
import argparse
import hashlib
import json
import re
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, List, Tuple


def utc_now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat()


def stable_hash(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def normalize_text(text: str) -> str:
    text = re.sub(r"\s+", " ", text.strip())
    return text


def infer_flow_status_from_comment(comment: Dict) -> str:
    # Single deterministic rule from upstream schema.
    return "resolved" if comment.get("thread_resolved") is True else "open"


def read_json(path: Path) -> Dict:
    if not path.exists():
        return {}
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, payload: Dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def default_status(comment: Dict) -> Dict:
    body = comment.get("body", "")
    snippet = normalize_text(body)[:300]
    fingerprint_seed = "|".join(
        [
            str(comment.get("path", "")),
            str(comment.get("line", 0)),
            str(comment.get("side", "RIGHT")),
            snippet,
        ]
    )
    return {
        "comment_id": comment.get("comment_id"),
        "path": comment.get("path", ""),
        "line": comment.get("line", 0),
        "side": comment.get("side", "RIGHT"),
        "body": body,
        "snippet": snippet,
        "snippet_fingerprint": stable_hash(fingerprint_seed),
        "status_flow": infer_flow_status_from_comment(comment),
        # status_tech is owned by model review in later phase.
        "status_tech": "not-fixed",
        "mapped_finding_id": "",
        "notes": "",
    }


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


def validate_payload(raw: Dict) -> List[Dict]:
    if not isinstance(raw, dict):
        raise SystemExit("invalid review-comments.json: root must be object")
    comments = raw.get("comments")
    if not isinstance(comments, list):
        raise SystemExit("invalid review-comments.json: `comments` must be list")
    return comments


def validate_comment(comment: Dict) -> None:
    required = ["comment_id", "path", "line", "side", "body", "thread_resolved"]
    missing = [k for k in required if k not in comment]
    if missing:
        raise SystemExit(
            "invalid review comment record: missing required fields "
            + ",".join(missing)
        )


def infer_manual_tech_override(replies: List[Dict], viewer_login: str) -> str:
    if not viewer_login:
        return ""
    # Prefer the latest explicit override from current reviewer account.
    for reply in reversed(replies):
        if str(reply.get("author", "")).lower() != viewer_login.lower():
            continue
        text = normalize_text(str(reply.get("body", ""))).lower()
        if "false-positive" in text or "false positive" in text or "假阳性" in text or "误判" in text:
            return "false-positive"
        if re.search(r"\bfixed\b", text) or "已修复" in text:
            return "fixed"
    return ""


def main() -> None:
    parser = argparse.ArgumentParser(description="Build comment-status.json from review comments.")
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
    status_path = review_dir / "comments" / "comment-status.json"

    comments_payload = read_json(comments_path)
    if not comments_payload:
        raise SystemExit(f"missing comments file: {comments_path}")
    comments = validate_payload(comments_payload)
    viewer_login = str(comments_payload.get("viewer_login", "")).strip()

    replies_by_parent: Dict[int, List[Dict]] = {}
    root_comments: List[Dict] = []
    for c in comments:
        parent = c.get("in_reply_to_id")
        if parent is None:
            root_comments.append(c)
        else:
            replies_by_parent.setdefault(parent, []).append(c)

    previous = read_json(status_path)
    previous_map = {item.get("comment_id"): item for item in previous.get("status", [])}

    status: List[Dict] = []
    seen_fp = set()
    for comment in root_comments:
        validate_comment(comment)
        cid = comment.get("comment_id")
        if cid in previous_map:
            item = previous_map[cid]
            # Preserve manual/model edits on status_tech/notes, always sync flow status from source.
            item["status_flow"] = infer_flow_status_from_comment(comment)
        else:
            item = default_status(comment)

        fp = item.get("snippet_fingerprint", "")
        if fp and fp in seen_fp:
            item["notes"] = (item.get("notes", "") + " duplicate-fingerprint").strip()
        manual_override = infer_manual_tech_override(replies_by_parent.get(cid, []), viewer_login)
        if manual_override:
            item["status_tech"] = manual_override
            item["notes"] = (item.get("notes", "") + f" manual-tech-override:{manual_override}").strip()
        seen_fp.add(fp)
        status.append(item)

    payload = {
        "version": "1.0",
        "generated_at": utc_now(),
        "review_target": {"type": target_type, "id": target_id_raw},
        "status": status,
    }
    write_json(status_path, payload)

    print(
        json.dumps(
            {"review_target": {"type": target_type, "id": target_id_raw}, "status_count": len(status)},
            ensure_ascii=False,
        )
    )


if __name__ == "__main__":
    main()
