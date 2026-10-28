#!/usr/bin/env python3
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path
from dataclasses import dataclass
from datetime import datetime, timezone
from typing import Dict, List, Optional, Set, Tuple


def run_git(repo_root: Path, args: List[str]) -> str:
    result = subprocess.run(
        ["git", *args],
        cwd=repo_root,
        text=True,
        capture_output=True,
        check=True,
    )
    return result.stdout


def run_git_no_check(repo_root: Path, args: List[str]) -> subprocess.CompletedProcess:
    return subprocess.run(
        ["git", *args],
        cwd=repo_root,
        text=True,
        capture_output=True,
        check=False,
    )


def utc_now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat()


def normalize_code_line(line: str) -> str:
    return re.sub(r"\s+", " ", line.strip())


def stable_hash(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def normalize_file_content(text: str) -> str:
    # Keep line boundaries but normalize whitespace noise for robust matching.
    lines = [re.sub(r"\s+", " ", line.strip()) for line in text.splitlines()]
    return "\n".join(lines)


def compute_patch_id(repo_root: Path, commit_sha: str) -> str:
    patch_text = run_git(repo_root, ["show", "--pretty=format:", "--no-color", commit_sha])
    proc = subprocess.run(
        ["git", "patch-id", "--stable"],
        cwd=repo_root,
        text=True,
        input=patch_text,
        capture_output=True,
        check=True,
    )
    output = proc.stdout.strip()
    return output.split()[0] if output else ""


def get_commit_files(repo_root: Path, commit_sha: str) -> List[str]:
    out = run_git(repo_root, ["show", "--pretty=format:", "--name-only", "--no-color", commit_sha])
    return sorted({line.strip() for line in out.splitlines() if line.strip()})


def get_file_content_at_commit(repo_root: Path, commit_sha: str, path: str) -> Optional[str]:
    proc = run_git_no_check(repo_root, ["show", f"{commit_sha}:{path}"])
    if proc.returncode != 0:
        return None
    return proc.stdout


def load_latest_snapshot_map(review_dir: Path) -> Dict[str, str]:
    latest_path = review_dir / "snapshot" / "latest.json"
    if not latest_path.exists():
        return {}
    try:
        latest = json.loads(latest_path.read_text(encoding="utf-8"))
    except Exception:
        return {}
    manifest_rel = latest.get("manifest")
    if not isinstance(manifest_rel, str) or not manifest_rel:
        return {}
    manifest_path = review_dir / manifest_rel
    if not manifest_path.exists():
        return {}
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except Exception:
        return {}
    mapping: Dict[str, str] = {}
    for item in manifest.get("files", []):
        path = item.get("path")
        n_hash = item.get("normalized_hash")
        if isinstance(path, str) and isinstance(n_hash, str):
            mapping[path] = n_hash
    return mapping


def compute_snapshot_match_rate(
    repo_root: Path, head_sha: str, changed_files: Set[str], snapshot_map: Dict[str, str]
) -> Optional[float]:
    if not snapshot_map or not changed_files:
        return None
    overlap = [p for p in changed_files if p in snapshot_map]
    if not overlap:
        return None
    matched = 0
    for path in overlap:
        content = get_file_content_at_commit(repo_root, head_sha, path)
        if content is None:
            continue
        current_hash = stable_hash(normalize_file_content(content))
        if current_hash == snapshot_map[path]:
            matched += 1
    return matched / len(overlap)


def parse_hunk_fingerprints(repo_root: Path, commit_sha: str) -> List[str]:
    patch = run_git(repo_root, ["show", "--pretty=format:", "--no-color", "-U3", commit_sha])
    lines = patch.splitlines()
    file_path = ""
    hunk_header = ""
    hunk_lines: List[str] = []
    fps: List[str] = []

    def flush() -> None:
        nonlocal hunk_lines, hunk_header
        if not hunk_lines:
            return
        key = file_path + "\n" + hunk_header + "\n" + "\n".join(hunk_lines)
        fps.append(stable_hash(key))
        hunk_lines = []
        hunk_header = ""

    for line in lines:
        if line.startswith("diff --git "):
            flush()
            m = re.search(r" b/(.+)$", line)
            file_path = m.group(1) if m else ""
            continue
        if line.startswith("@@"):
            flush()
            hunk_header = line
            continue
        if line.startswith("+") or line.startswith("-"):
            if line.startswith("+++") or line.startswith("---"):
                continue
            hunk_lines.append(normalize_code_line(line[1:]))

    flush()
    return sorted(set(fps))


def jaccard(a: Set[str], b: Set[str]) -> float:
    if not a and not b:
        return 1.0
    if not a or not b:
        return 0.0
    return len(a & b) / len(a | b)


@dataclass
class CommitRecord:
    commit_sha: str
    patch_id: str
    hunk_fingerprints: List[str]
    review_round: int
    reviewed_at: str
    mapping: Dict[str, object]


def load_json(path: Path) -> Dict:
    if not path.exists():
        return {}
    return json.loads(path.read_text(encoding="utf-8"))


def save_json(path: Path, payload: Dict) -> None:
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


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


def ensure_commit_exists(repo_root: Path, sha: str, target_type: str, target_id: str) -> bool:
    exists = run_git_no_check(repo_root, ["cat-file", "-e", f"{sha}^{{commit}}"])
    if exists.returncode == 0:
        return True

    # First generic fetch to cover normal branch updates.
    run_git_no_check(repo_root, ["fetch", "--all", "--prune", "--tags"])
    exists = run_git_no_check(repo_root, ["cat-file", "-e", f"{sha}^{{commit}}"])
    if exists.returncode == 0:
        return True

    # Then PR-specific fetch for detached PR heads.
    if target_type == "pr":
        run_git_no_check(repo_root, ["fetch", "origin", f"pull/{target_id}/head"])
        exists = run_git_no_check(repo_root, ["cat-file", "-e", f"{sha}^{{commit}}"])
        if exists.returncode == 0:
            return True
    return False


def main() -> None:
    parser = argparse.ArgumentParser(description="Map reviewed commits for incremental PR/branch review.")
    parser.add_argument("--repo-root", required=True)
    parser.add_argument("--target-type", choices=["pr", "branch"])
    parser.add_argument("--target-id")
    parser.add_argument("--pr-number", type=int, help="PR number (legacy compatible)")
    parser.add_argument("--branch-name", help="Branch name (legacy compatible)")
    parser.add_argument("--base", required=True, help="Base commit SHA for comparison")
    parser.add_argument("--head", required=True, help="Head commit SHA for comparison")
    parser.add_argument("--review-round", required=True, type=int)
    parser.add_argument("--commit-map-threshold", type=float, default=0.9)
    parser.add_argument("--hunk-match-threshold", type=float, default=0.8)
    parser.add_argument("--snapshot-match-threshold", type=float, default=0.9)
    args = parser.parse_args()

    repo_root = Path(args.repo_root).resolve()
    target_type, target_id_raw = resolve_target(args)
    target_id_dir = sanitize_branch_name(target_id_raw) if target_type == "branch" else target_id_raw
    review_dir = repo_root / "code-review" / f"{target_type}-{target_id_dir}"
    reviewed_path = review_dir / "reviewed_commits.json"
    reviewed = load_json(reviewed_path) or {"version": "1.0", "review_rounds": [], "commits": []}
    old_commits = reviewed.get("commits", [])
    snapshot_map = load_latest_snapshot_map(review_dir)

    if not ensure_commit_exists(repo_root, args.base, target_type, target_id_raw):
        raise SystemExit(
            f"missing base commit object: {args.base}. "
            "Please fetch the base branch history, then retry."
        )
    if not ensure_commit_exists(repo_root, args.head, target_type, target_id_raw):
        raise SystemExit(
            f"missing head commit object: {args.head}. "
            "For PR review, try: git fetch origin pull/<pr-number>/head"
        )

    try:
        rev_list_output = run_git(repo_root, ["rev-list", "--reverse", f"{args.base}..{args.head}"])
    except subprocess.CalledProcessError as e:
        stderr = (e.stderr or "").strip()
        raise SystemExit(
            f"failed to build commit range {args.base}..{args.head}: {stderr or 'unknown git error'}"
        )

    current_commits = [sha for sha in rev_list_output.splitlines() if sha]
    current_set = set(current_commits)
    commit_files_map: Dict[str, List[str]] = {}
    current_changed_files: Set[str] = set()
    for sha in current_commits:
        files = get_commit_files(repo_root, sha)
        commit_files_map[sha] = files
        current_changed_files.update(files)

    old_by_sha = {c.get("commit_sha"): c for c in old_commits if c.get("commit_sha")}
    old_by_patch_id: Dict[str, Dict] = {}
    for c in old_commits:
        pid = c.get("patch_id")
        if pid and pid not in old_by_patch_id:
            old_by_patch_id[pid] = c

    mapped: Dict[str, CommitRecord] = {}
    unchanged_by_sha = 0
    for sha in current_commits:
        if sha in old_by_sha:
            oc = old_by_sha[sha]
            mapped[sha] = CommitRecord(
                commit_sha=sha,
                patch_id=oc.get("patch_id", ""),
                hunk_fingerprints=oc.get("hunk_fingerprints", []),
                review_round=oc.get("review_round", args.review_round),
                reviewed_at=oc.get("reviewed_at", utc_now()),
                mapping={"method": "direct", "mapped_from_commit": sha, "confidence": 1.0},
            )
            unchanged_by_sha += 1

    for sha in current_commits:
        if sha in mapped:
            continue
        pid = compute_patch_id(repo_root, sha)
        if pid and pid in old_by_patch_id:
            oc = old_by_patch_id[pid]
            mapped[sha] = CommitRecord(
                commit_sha=sha,
                patch_id=pid,
                hunk_fingerprints=oc.get("hunk_fingerprints", []),
                review_round=oc.get("review_round", args.review_round),
                reviewed_at=oc.get("reviewed_at", utc_now()),
                mapping={"method": "patch-id", "mapped_from_commit": oc.get("commit_sha", ""), "confidence": 0.98},
            )

    old_unmapped = [c for c in old_commits if c.get("commit_sha") not in current_set]
    old_hunk_sets = {
        c.get("commit_sha", ""): set(c.get("hunk_fingerprints", [])) for c in old_unmapped if c.get("commit_sha")
    }

    for sha in current_commits:
        if sha in mapped:
            continue
        new_hunks = set(parse_hunk_fingerprints(repo_root, sha))
        best_score = 0.0
        best_old = ""
        for old_sha, old_hunks in old_hunk_sets.items():
            score = jaccard(new_hunks, old_hunks)
            if score > best_score:
                best_score = score
                best_old = old_sha
        if best_old and best_score >= args.hunk_match_threshold:
            mapped[sha] = CommitRecord(
                commit_sha=sha,
                patch_id=compute_patch_id(repo_root, sha),
                hunk_fingerprints=sorted(new_hunks),
                review_round=args.review_round,
                reviewed_at=utc_now(),
                mapping={"method": "hunk-similarity", "mapped_from_commit": best_old, "confidence": round(best_score, 4)},
            )

    need_review: List[str] = [sha for sha in current_commits if sha not in mapped]

    # commit_map_rate measures "how many OLD commits are accounted for in the
    # new commit set", NOT "what fraction of current commits are mapped".
    # Denominator = old commit count (the baseline we reviewed before).
    # This way appending new commits doesn't penalise the rate, while rebase
    # that loses old commits correctly lowers it.
    old_commits_covered: Set[str] = set()
    for rec in mapped.values():
        from_sha = rec.mapping.get("mapped_from_commit", "")
        if from_sha:
            old_commits_covered.add(from_sha)
    old_commit_count = len(old_commits)
    commit_map_rate = (len(old_commits_covered) / old_commit_count) if old_commit_count > 0 else 1.0

    if need_review:
        hunk_scores: List[float] = []
        for sha in need_review:
            new_hunks = set(parse_hunk_fingerprints(repo_root, sha))
            best = 0.0
            for old_hunks in old_hunk_sets.values():
                best = max(best, jaccard(new_hunks, old_hunks))
            hunk_scores.append(best)
        hunk_match_rate = (sum(hunk_scores) / len(hunk_scores)) if hunk_scores else 1.0
    else:
        hunk_match_rate = 1.0

    if commit_map_rate >= args.commit_map_threshold:
        recommendation = "incremental"
    elif hunk_match_rate >= args.hunk_match_threshold:
        recommendation = "partial"
    else:
        recommendation = "full"

    snapshot_match_rate = compute_snapshot_match_rate(repo_root, args.head, current_changed_files, snapshot_map)
    if recommendation == "full" and snapshot_match_rate is not None and snapshot_match_rate >= args.snapshot_match_threshold:
        # For squash/rebase-conflict scenarios, snapshot evidence can safely
        # downgrade from full to partial.
        recommendation = "partial"

    round_record = {
        "review_round": args.review_round,
        "generated_at": utc_now(),
        "base": args.base,
        "head": args.head,
        "stats": {
            "total_commits": len(current_commits),
            "mapped_commits": len(mapped),
            "direct_sha_hits": unchanged_by_sha,
            "commit_map_rate": round(commit_map_rate, 4),
            "hunk_match_rate": round(hunk_match_rate, 4),
            "snapshot_match_rate": round(snapshot_match_rate, 4) if snapshot_match_rate is not None else None,
            "recommendation": recommendation,
        },
        "need_review_commits": need_review,
    }

    merged_commits = [c for c in old_commits if c.get("commit_sha") not in current_set]
    for sha in current_commits:
        if sha in mapped:
            c = mapped[sha]
            merged_commits.append(
                {
                    "commit_sha": c.commit_sha,
                    "patch_id": c.patch_id,
                    "review_round": c.review_round,
                    "reviewed_at": c.reviewed_at,
                    "hunk_fingerprints": c.hunk_fingerprints,
                    "files": commit_files_map.get(sha, []),
                    "mapping": c.mapping,
                }
            )
        else:
            merged_commits.append(
                {
                    "commit_sha": sha,
                    "patch_id": compute_patch_id(repo_root, sha),
                    "review_round": args.review_round,
                    "reviewed_at": "",
                    "hunk_fingerprints": parse_hunk_fingerprints(repo_root, sha),
                    "files": commit_files_map.get(sha, []),
                    "mapping": {"method": "none", "mapped_from_commit": "", "confidence": 0.0},
                }
            )

    reviewed["commits"] = merged_commits
    reviewed.setdefault("review_rounds", []).append(round_record)
    save_json(reviewed_path, reviewed)

    print(
        json.dumps(
            {
                "review_target": {"type": target_type, "id": target_id_raw},
                "total_commits": len(current_commits),
                "need_review_commits": need_review,
                "commit_map_rate": round(commit_map_rate, 4),
                "hunk_match_rate": round(hunk_match_rate, 4),
                "snapshot_match_rate": round(snapshot_match_rate, 4) if snapshot_match_rate is not None else None,
                "recommendation": recommendation,
            },
            ensure_ascii=False,
        )
    )


if __name__ == "__main__":
    main()
