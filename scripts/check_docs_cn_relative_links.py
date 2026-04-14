#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Scan docs/cn for broken relative markdown links (.md / anchors).

Usage (from repo root):
  python3 scripts/check_docs_cn_relative_links.py

Exit 1 if any link target is missing. Skips http(s):// and mailto:.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / "docs" / "cn"
# [text](path) or [text](path "title")
LINK_RE = re.compile(r"\[[^\]]*\]\(([^)#\s]+)(?:#[^)\s]*)?\s*(?:\"[^\"]*\")?\)")


def main() -> int:
    bad: list[tuple[Path, str, Path]] = []
    for md in sorted(DOCS.rglob("*.md")):
        text = md.read_text(encoding="utf-8")
        for m in LINK_RE.finditer(text):
            target = m.group(1).strip()
            if not target or target.startswith(("http://", "https://", "mailto:")):
                continue
            # Skip pure anchors / empty
            if target.startswith("#"):
                continue
            # Normalize path (no query in local docs)
            path_part = target.split("?", 1)[0]
            if not path_part.endswith(".md"):
                continue
            raw = path_part.strip()
            if raw.startswith("<") and raw.endswith(">"):
                raw = raw[1:-1]
            resolved = (md.parent / raw).resolve()
            try:
                resolved.relative_to(ROOT.resolve())
            except ValueError:
                bad.append((md, target, resolved))
                continue
            if not resolved.is_file():
                bad.append((md, target, resolved))

    if bad:
        for src, tgt, abs_path in bad:
            print(f"MISSING: {src.relative_to(ROOT)} -> {tgt} (resolved {abs_path})")
        return 1
    print(f"OK: checked relative .md links under {DOCS.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
