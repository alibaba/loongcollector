#!/usr/bin/env python3
"""Scan processor/flusher plugin event capability matrix.

This script scans plugin registrations and method signatures under:
- plugins/processor
- plugins/flusher

It outputs a markdown matrix for plugin × {Log, Metric, Span} with status:
- v1 only
- dual
- v2 only
- passthrough unknown
"""

import argparse
import datetime as dt
import re
import sys
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Set


PROCESSOR_REGISTRATION_PATTERN = re.compile(
    r"pipeline\.Processors\[(?P<expr>[^\]]+)\]\s*=",
    re.MULTILINE,
)
FLUSHER_REGISTRATION_PATTERN = re.compile(
    r"pipeline\.Flushers\[(?P<expr>[^\]]+)\]\s*=",
    re.MULTILINE,
)

PROCESSOR_V1_METHOD_PATTERN = re.compile(
    r"func\s*\(\s*[^)]*\)\s*ProcessLogs\s*\(\s*[^)]*\[\]\*protocol\.Log[^)]*\)",
    re.MULTILINE | re.DOTALL,
)
PROCESSOR_V2_METHOD_PATTERN = re.compile(
    r"func\s*\(\s*[^)]*\)\s*Process\s*\(\s*in\s+\*models\.PipelineGroupEvents\s*,\s*context\s+pipeline\.PipelineContext\s*\)",
    re.MULTILINE,
)
FLUSHER_V1_METHOD_PATTERN = re.compile(
    r"func\s*\(\s*[^)]*\)\s*Flush\s*\(\s*[^)]*\[\]\*protocol\.LogGroup[^)]*\)\s*error",
    re.MULTILINE | re.DOTALL,
)
FLUSHER_V2_METHOD_PATTERN = re.compile(
    r"func\s*\(\s*[^)]*\)\s*Export\s*\(\s*[^)]*\[\]\*models\.PipelineGroupEvents[^)]*\)\s*error",
    re.MULTILINE | re.DOTALL,
)

LOG_SIGNAL_PATTERNS = [
    re.compile(r"models\.EventTypeLogging"),
    re.compile(r"\*models\.Log\b"),
    re.compile(r"\.\(\*models\.Log\)"),
]
METRIC_SIGNAL_PATTERNS = [
    re.compile(r"models\.EventTypeMetric"),
    re.compile(r"\*models\.Metric\b"),
    re.compile(r"\.\(\*models\.Metric\)"),
]
SPAN_SIGNAL_PATTERNS = [
    re.compile(r"models\.EventTypeSpan"),
    re.compile(r"\*models\.Span\b"),
    re.compile(r"\.\(\*models\.Span\)"),
]

EVENT_ORDER = ("log", "metric", "span")


@dataclass
class PluginRecord:
    name: str
    kind: str
    directories: Set[Path]
    has_v1: bool
    has_v2: bool
    event_signals: Dict[str, bool]

    @property
    def interface_class(self) -> str:
        if self.has_v1 and self.has_v2:
            return "dual-interface"
        if self.has_v1:
            return "v1-only"
        if self.has_v2:
            return "v2-only"
        return "unknown"

    def status_for_event(self, event: str) -> str:
        if self.interface_class == "v1-only":
            if event == "log":
                return "v1 only"
            return "passthrough unknown"

        if self.interface_class == "dual-interface":
            if event == "log":
                return "dual"
            if self.event_signals.get(event, False):
                return "v2 only"
            return "passthrough unknown"

        if self.interface_class == "v2-only":
            if self.event_signals.get(event, False):
                return "v2 only"
            return "passthrough unknown"

        return "passthrough unknown"


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="ignore")


def list_go_files(directory: Path) -> Iterable[Path]:
    if not directory.exists():
        return []
    return sorted(
        p for p in directory.rglob("*.go") if p.is_file() and not p.name.endswith("_test.go")
    )


def parse_symbol_strings(content: str) -> Dict[str, str]:
    symbols: Dict[str, str] = {}

    single_line_pattern = re.compile(
        r'^\s*(?:const|var)\s+([A-Za-z_]\w*)\s*=\s*"([^"]+)"\s*$',
        re.MULTILINE,
    )
    for match in single_line_pattern.finditer(content):
        symbols[match.group(1)] = match.group(2)

    in_const_block = False
    for line in content.splitlines():
        stripped = line.strip()
        if stripped.startswith("const ("):
            in_const_block = True
            continue
        if in_const_block:
            if stripped == ")":
                in_const_block = False
                continue
            match = re.match(r'^([A-Za-z_]\w*)\s*=\s*"([^"]+)"', stripped)
            if match:
                symbols[match.group(1)] = match.group(2)

    return symbols


def resolve_registration_expr(expr: str, symbols: Dict[str, str]) -> str:
    trimmed = expr.strip()
    if trimmed.startswith('"') and trimmed.endswith('"'):
        return trimmed[1:-1]
    return symbols.get(trimmed, trimmed)


def collect_registered_plugins(repo_root: Path, kind: str) -> Dict[str, Set[Path]]:
    target_dir = repo_root / "plugins" / kind
    registrations: Dict[str, Set[Path]] = defaultdict(set)

    pattern = (
        PROCESSOR_REGISTRATION_PATTERN if kind == "processor" else FLUSHER_REGISTRATION_PATTERN
    )
    for go_file in list_go_files(target_dir):
        content = read_text(go_file)
        symbols = parse_symbol_strings(content)
        for match in pattern.finditer(content):
            plugin_name = resolve_registration_expr(match.group("expr"), symbols)
            if plugin_name:
                registrations[plugin_name].add(go_file.parent)

    return registrations


def detect_event_signals(content: str) -> Dict[str, bool]:
    return {
        "log": any(pattern.search(content) for pattern in LOG_SIGNAL_PATTERNS),
        "metric": any(pattern.search(content) for pattern in METRIC_SIGNAL_PATTERNS),
        "span": any(pattern.search(content) for pattern in SPAN_SIGNAL_PATTERNS),
    }


def analyze_plugin(repo_root: Path, kind: str, name: str, directories: Set[Path]) -> PluginRecord:
    has_v1 = False
    has_v2 = False
    event_signals = {event: False for event in EVENT_ORDER}

    for directory in directories:
        for go_file in list_go_files(directory):
            content = read_text(go_file)
            if kind == "processor":
                has_v1 = has_v1 or bool(PROCESSOR_V1_METHOD_PATTERN.search(content))
                has_v2 = has_v2 or bool(PROCESSOR_V2_METHOD_PATTERN.search(content))
            else:
                has_v1 = has_v1 or bool(FLUSHER_V1_METHOD_PATTERN.search(content))
                has_v2 = has_v2 or bool(FLUSHER_V2_METHOD_PATTERN.search(content))

            signals = detect_event_signals(content)
            for event in EVENT_ORDER:
                event_signals[event] = event_signals[event] or signals[event]

    return PluginRecord(
        name=name,
        kind=kind,
        directories=directories,
        has_v1=has_v1,
        has_v2=has_v2,
        event_signals=event_signals,
    )


def build_records(repo_root: Path, kind: str) -> List[PluginRecord]:
    registrations = collect_registered_plugins(repo_root, kind)
    records = [
        analyze_plugin(repo_root=repo_root, kind=kind, name=name, directories=directories)
        for name, directories in registrations.items()
    ]
    records.sort(key=lambda record: record.name)
    return records


def render_plugin_table(records: List[PluginRecord], repo_root: Path) -> str:
    lines = [
        "| Plugin | Interface class | Log | Metric | Span | Source directories |",
        "|---|---|---|---|---|---|",
    ]
    for record in records:
        directories = ", ".join(
            sorted(str(directory.relative_to(repo_root)) for directory in record.directories)
        )
        lines.append(
            "| {name} | {iface} | {log} | {metric} | {span} | `{dirs}` |".format(
                name=record.name,
                iface=record.interface_class,
                log=record.status_for_event("log"),
                metric=record.status_for_event("metric"),
                span=record.status_for_event("span"),
                dirs=directories,
            )
        )
    return "\n".join(lines)


def render_class_summary(records: List[PluginRecord]) -> str:
    by_class: Dict[str, List[str]] = defaultdict(list)
    for record in records:
        by_class[record.interface_class].append(record.name)

    for names in by_class.values():
        names.sort()

    lines = []
    for class_name in ("v1-only", "dual-interface", "v2-only", "unknown"):
        names = by_class.get(class_name, [])
        if names:
            lines.append(f"- `{class_name}` ({len(names)}): {', '.join(f'`{name}`' for name in names)}")
        else:
            lines.append(f"- `{class_name}` (0): -")
    return "\n".join(lines)


def render_markdown(repo_root: Path, processor_records: List[PluginRecord], flusher_records: List[PluginRecord]) -> str:
    now = dt.datetime.now(dt.timezone.utc).strftime("%Y-%m-%d %H:%M:%SZ")
    processor_count = Counter(record.interface_class for record in processor_records)
    flusher_count = Counter(record.interface_class for record in flusher_records)

    return f"""# [1928-B2] Plugin Event Matrix (Processor/Flusher × Log/Metric/Span)

Generated at: `{now}` (UTC)  
Repository root: `{repo_root}`

## Scope & Method

- Read-only scan of `plugins/processor` and `plugins/flusher`; no plugin implementation changes.
- Interface detection:
  - Processor v1: `ProcessLogs([]*protocol.Log)`
  - Processor v2: `Process(*models.PipelineGroupEvents, pipeline.PipelineContext)`
  - Flusher v1: `Flush(..., []*protocol.LogGroup)`
  - Flusher v2: `Export([]*models.PipelineGroupEvents, pipeline.PipelineContext)`
- Event support values in the table:
  - `v1 only`, `dual`, `v2 only`, `passthrough unknown`
- `passthrough unknown` means the plugin has no explicit type-specific signal in code for that event type.

## Processor Summary

- Total registered processors: **{len(processor_records)}**
- v1-only: **{processor_count.get("v1-only", 0)}**
- dual-interface: **{processor_count.get("dual-interface", 0)}**
- v2-only: **{processor_count.get("v2-only", 0)}**

{render_class_summary(processor_records)}

## Processor Matrix

{render_plugin_table(processor_records, repo_root)}

## Flusher Summary

- Total registered flushers: **{len(flusher_records)}**
- v1-only: **{flusher_count.get("v1-only", 0)}**
- dual-interface: **{flusher_count.get("dual-interface", 0)}**
- v2-only: **{flusher_count.get("v2-only", 0)}**

{render_class_summary(flusher_records)}

## Flusher Matrix

{render_plugin_table(flusher_records, repo_root)}
"""


def parse_args(argv: List[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Scan Processor/Flusher plugin event capability matrix.",
    )
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[2],
        help="Repository root path (default: auto-detected from script path).",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help="Output markdown file path. If omitted, print to stdout.",
    )
    return parser.parse_args(argv)


def main(argv: List[str]) -> int:
    args = parse_args(argv)
    repo_root = args.repo_root.resolve()

    processor_records = build_records(repo_root, "processor")
    flusher_records = build_records(repo_root, "flusher")
    markdown = render_markdown(repo_root, processor_records, flusher_records)

    if args.output:
        output_path = args.output
        if not output_path.is_absolute():
            output_path = repo_root / output_path
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(markdown, encoding="utf-8")
        print(f"Wrote matrix report to: {output_path}")
    else:
        print(markdown)

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
