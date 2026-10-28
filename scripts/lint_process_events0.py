#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Forbid ProcessorV2.Process from treating Events[0] as a Log.

Discussion #1928 step B5 / Issue #2604.

A v2 Process method receives PipelineGroupEvents that may mix Log, Metric, and
Span. Indexing Events[0] and assuming *models.Log panics on other kinds and
silently drops the rest of the batch. Iterate every event (or call
pipeline.ProcessLogEventsOnly / CollectGroupEvents).

Compatible with Python 3.6+.

Usage (repo root):
  python3 scripts/lint_process_events0.py
  python3 scripts/lint_process_events0.py --root . --allowlist scripts/lint_process_events0_allowlist.txt
"""
from __future__ import print_function

import argparse
import re
import sys
from collections import namedtuple
from pathlib import Path

DEFAULT_SCAN_DIRS = ("plugins", "pluginmanager")
SKIP_DIR_NAMES = {
    ".git",
    "vendor",
    "node_modules",
    "external",
    "output",
    "build",
    "core",
}
SKIP_FILE_SUFFIX = "_test.go"

EVENTS0_RE = re.compile(r"\.Events\s*\[\s*0\s*\]")
ALIAS_ASSIGN_RE = re.compile(
    r"\b([A-Za-z_]\w*)\s*:?=\s*[A-Za-z_]\w*(?:\.[A-Za-z_]\w*)*\.Events\b"
)
IDENT_INDEX0_RE = re.compile(r"\b([A-Za-z_]\w*)\s*\[\s*0\s*\]")
LOG_ASSERT_RE = re.compile(r"\(\s*\*?\s*(?:[\w.]*\.)?Log\s*\)")
FUNC_RECV_RE = re.compile(r"\bfunc\s+\(")

_Violation = namedtuple("Violation", "path line snippet reason")


class Violation(_Violation):
    def key(self):
        return "{}:{}".format(self.path, self.line)


def mask_go_comments_and_strings(src):
    """Replace comments and string/rune contents with spaces; keep newlines."""
    out = []
    i = 0
    n = len(src)

    def keep_nl(ch):
        return "\n" if ch == "\n" else " "

    while i < n:
        ch = src[i]
        nxt = src[i + 1] if i + 1 < n else ""
        if ch == "/" and nxt == "/":
            while i < n and src[i] != "\n":
                out.append(" ")
                i += 1
            continue
        if ch == "/" and nxt == "*":
            out.extend((" ", " "))
            i += 2
            while i < n:
                if src[i] == "*" and i + 1 < n and src[i + 1] == "/":
                    out.extend((" ", " "))
                    i += 2
                    break
                out.append(keep_nl(src[i]))
                i += 1
            continue
        if ch == "`":
            out.append(" ")
            i += 1
            while i < n and src[i] != "`":
                out.append(keep_nl(src[i]))
                i += 1
            if i < n:
                out.append(" ")
                i += 1
            continue
        if ch in ('"', "'"):
            quote = ch
            out.append(" ")
            i += 1
            while i < n:
                if src[i] == "\\":
                    out.append(" ")
                    i += 1
                    if i < n:
                        out.append(keep_nl(src[i]))
                        i += 1
                    continue
                if src[i] == quote:
                    out.append(" ")
                    i += 1
                    break
                out.append(keep_nl(src[i]))
                i += 1
            continue
        out.append(ch)
        i += 1
    return "".join(out)


def _find_matching(src, open_idx, open_ch, close_ch):
    depth = 0
    for i in range(open_idx, len(src)):
        ch = src[i]
        if ch == open_ch:
            depth += 1
        elif ch == close_ch:
            depth -= 1
            if depth == 0:
                return i
    return -1


def _line_of(src, idx):
    return src.count("\n", 0, idx) + 1


def _snippet(src, idx):
    line_start = src.rfind("\n", 0, idx) + 1
    line_end = src.find("\n", idx)
    if line_end < 0:
        line_end = len(src)
    return src[line_start:line_end].strip()


def iter_process_v2_bodies(src):
    """Return (body_start, body_end, params) for Process(*PipelineGroupEvents)."""
    masked = mask_go_comments_and_strings(src)
    bodies = []
    i = 0
    while True:
        m = FUNC_RECV_RE.search(masked, i)
        if not m:
            break
        recv_open = m.end() - 1
        recv_close = _find_matching(masked, recv_open, "(", ")")
        if recv_close < 0:
            break
        after_recv = masked[recv_close + 1 :]
        name_m = re.match(r"\s*Process\s*\(", after_recv)
        if not name_m:
            i = recv_close + 1
            continue
        params_open = recv_close + 1 + name_m.end() - 1
        params_close = _find_matching(masked, params_open, "(", ")")
        if params_close < 0:
            i = recv_close + 1
            continue
        params = masked[params_open + 1 : params_close]
        if "PipelineGroupEvents" not in params:
            i = params_close + 1
            continue
        brace = masked.find("{", params_close)
        if brace < 0:
            i = params_close + 1
            continue
        body_end = _find_matching(masked, brace, "{", "}")
        if body_end < 0:
            i = brace + 1
            continue
        bodies.append((brace + 1, body_end, params))
        i = body_end + 1
    return bodies


def find_violations(src, path):
    """Find Events[0] (as Log) anti-patterns inside ProcessorV2.Process."""
    masked = mask_go_comments_and_strings(src)
    found = []
    seen = set()

    def add(idx, reason):
        line = _line_of(src, idx)
        key = (path, line)
        if key in seen:
            return
        seen.add(key)
        found.append(
            Violation(
                path=path,
                line=line,
                snippet=_snippet(src, idx),
                reason=reason,
            )
        )

    for body_start, body_end, _params in iter_process_v2_bodies(src):
        body = masked[body_start:body_end]
        for m in EVENTS0_RE.finditer(body):
            idx = body_start + m.start()
            reason = (
                "Process must not index Events[0]; iterate all events or use "
                "pipeline.ProcessLogEventsOnly (do not assume Events[0] is Log)"
            )
            window = body[m.start() : m.start() + 80]
            if LOG_ASSERT_RE.search(window) or "models.Log" in window:
                reason = (
                    "Process must not type-assert Events[0] as Log; iterate "
                    "all events and branch on event.GetType()"
                )
            add(idx, reason)

        aliases = set()
        for m in ALIAS_ASSIGN_RE.finditer(body):
            aliases.add(m.group(1))
        if not aliases:
            continue
        for m in IDENT_INDEX0_RE.finditer(body):
            ident = m.group(1)
            if ident not in aliases:
                continue
            idx = body_start + m.start()
            window = body[m.start() : m.start() + 80]
            reason = (
                "Process must not index the Events slice at [0]; iterate all "
                "events or use pipeline.ProcessLogEventsOnly"
            )
            if LOG_ASSERT_RE.search(window) or "models.Log" in window:
                reason = (
                    "Process must not type-assert Events[0] as Log; iterate "
                    "all events and branch on event.GetType()"
                )
            add(idx, reason)
    return found


def load_allowlist(path):
    """Load exemption keys: '<relpath>' or '<relpath>:<line>'."""
    keys = set()
    if not path.is_file():
        return keys
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        keys.add(line.replace("\\", "/"))
    return keys


def is_allowlisted(v, allow):
    rel = v.path.replace("\\", "/")
    return rel in allow or v.key() in allow


def iter_go_files(root, scan_dirs):
    files = []
    for rel in scan_dirs:
        base = root / rel
        if not base.is_dir():
            continue
        for path in base.rglob("*.go"):
            if any(part in SKIP_DIR_NAMES for part in path.parts):
                continue
            if path.name.endswith(SKIP_FILE_SUFFIX):
                continue
            files.append(path)
    return sorted(files)


def scan_repo(root, allow, scan_dirs=DEFAULT_SCAN_DIRS):
    violations = []
    for path in iter_go_files(root, scan_dirs):
        rel = path.relative_to(root).as_posix()
        text = path.read_text(encoding="utf-8")
        for v in find_violations(text, rel):
            if not is_allowlisted(v, allow):
                violations.append(v)
    return violations


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Forbid ProcessorV2.Process from assuming Events[0] is Log."
    )
    parser.add_argument(
        "--root",
        type=Path,
        default=None,
        help="Repository root (default: parent of scripts/)",
    )
    parser.add_argument(
        "--allowlist",
        type=Path,
        default=None,
        help="Allowlist file (default: scripts/lint_process_events0_allowlist.txt)",
    )
    args = parser.parse_args(argv)
    root = (args.root or Path(__file__).resolve().parents[1]).resolve()
    allow_path = args.allowlist or (root / "scripts" / "lint_process_events0_allowlist.txt")
    allow = load_allowlist(allow_path)
    violations = scan_repo(root, allow)
    if not violations:
        print("lint_process_events0: OK (no Events[0]-as-Log in Process)")
        return 0
    print(
        "lint_process_events0: ProcessorV2.Process must not assume Events[0] is Log:",
        file=sys.stderr,
    )
    for v in violations:
        print("  {}:{}: {}".format(v.path, v.line, v.reason), file=sys.stderr)
        print("    {}".format(v.snippet), file=sys.stderr)
    print(
        "Fix: iterate in.Events (or pipeline.ProcessLogEventsOnly). "
        "Exemptions go in {}.".format(allow_path.relative_to(root).as_posix()),
        file=sys.stderr,
    )
    return 1


if __name__ == "__main__":
    sys.exit(main())
