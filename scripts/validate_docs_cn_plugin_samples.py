#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Validate ```json code fences under docs/cn/plugins (and optional ```yaml).

Supports:
  - single JSON value (object/array)
  - JSON array of objects
  - multiple objects separated by blank lines
  - NDJSON: one compact JSON object per non-empty line

YAML fences are validated when PyYAML is installed (`pip install pyyaml`).

Usage (repo root):
  python3 scripts/validate_docs_cn_plugin_samples.py
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLUGINS_DOC = ROOT / "docs" / "cn" / "plugins"
FENCE_RE = re.compile(r"^```(\w+)$", re.MULTILINE)

try:
    import yaml  # type: ignore
except ImportError:
    yaml = None  # type: ignore


def validate_json_body(body: str, src: Path) -> list[str]:
    errs: list[str] = []
    s = body.strip()
    if not s:
        return errs

    if s.startswith("["):
        try:
            json.loads(s)
        except json.JSONDecodeError as e:
            errs.append(f"{src}: JSON array: {e}")
        return errs

    try:
        json.loads(s)
        return errs
    except json.JSONDecodeError:
        pass

    lines = [ln.strip() for ln in s.splitlines() if ln.strip()]
    if lines and all(ln[0] in "{[" for ln in lines):
        for i, ln in enumerate(lines):
            try:
                json.loads(ln)
            except json.JSONDecodeError as e:
                errs.append(f"{src}: NDJSON line {i + 1}: {e}")
        return errs

    chunks = re.split(r"\n\s*\n", s)
    for i, ch in enumerate(chunks):
        ch = ch.strip()
        if not ch:
            continue
        try:
            json.loads(ch)
        except json.JSONDecodeError as e:
            errs.append(f"{src}: JSON chunk {i + 1}: {e}")
    return errs


def validate_yaml_body(body: str, src: Path) -> list[str]:
    if yaml is None:
        return []
    errs: list[str] = []
    try:
        yaml.safe_load(body)
    except Exception as e:
        errs.append(f"{src}: YAML: {e}")
    return errs


def iter_fences(text: str) -> list[tuple[str, str]]:
    """Return list of (lang, body) for fenced blocks."""
    out: list[tuple[str, str]] = []
    lines = text.splitlines(keepends=True)
    i = 0
    while i < len(lines):
        m = re.match(r"^```(\w+)\s*$", lines[i])
        if not m:
            i += 1
            continue
        lang = m.group(1)
        i += 1
        start = i
        while i < len(lines) and not re.match(r"^```\s*$", lines[i]):
            i += 1
        body = "".join(lines[start:i])
        out.append((lang, body))
        i += 1
    return out


def main() -> int:
    all_errs: list[str] = []
    for md in sorted(PLUGINS_DOC.rglob("*.md")):
        if md.name in ("README.md", "overview.md"):
            continue
        text = md.read_text(encoding="utf-8")
        for lang, body in iter_fences(text):
            if lang == "json":
                all_errs.extend(validate_json_body(body, md))
            elif lang == "yaml" and yaml is not None:
                all_errs.extend(validate_yaml_body(body, md))

    if yaml is None:
        print("NOTE: PyYAML not installed; only JSON fences were validated.", file=sys.stderr)

    if all_errs:
        for e in all_errs:
            print(e, file=sys.stderr)
        return 1
    rel = PLUGINS_DOC.relative_to(ROOT)
    print(f"OK: validated plugin doc samples under {rel} ({'JSON+YAML' if yaml else 'JSON only'})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
