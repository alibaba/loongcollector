#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Unit tests for lint_process_events0.py (Issue #2604)."""
from __future__ import print_function

import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lint_process_events0 as lint  # noqa: E402

PROCESS_HEADER = (
    "func (p *ProcessorFoo) Process("
    "in *models.PipelineGroupEvents, ctx pipeline.PipelineContext) {\n"
)


def _wrap(body):
    return (
        "package foo\n\n"
        "import (\n"
        '\t"github.com/alibaba/ilogtail/pkg/models"\n'
        '\t"github.com/alibaba/ilogtail/pkg/pipeline"\n'
        ")\n\n"
        + PROCESS_HEADER
        + body
        + "}\n"
    )


class FindViolationsTest(unittest.TestCase):
    def test_flags_events0_type_assert_log(self):
        src = _wrap("\tlog := in.Events[0].(*models.Log)\n\t_ = log\n")
        vs = lint.find_violations(src, "plugins/processor/foo/foo.go")
        self.assertEqual(len(vs), 1)
        self.assertEqual(vs[0].line, 9)
        self.assertIn("type-assert Events[0] as Log", vs[0].reason)

    def test_flags_events0_index_without_assert(self):
        src = _wrap("\t_ = in.Events[0]\n")
        vs = lint.find_violations(src, "plugins/processor/foo/foo.go")
        self.assertEqual(len(vs), 1)
        self.assertIn("must not index Events[0]", vs[0].reason)

    def test_flags_aliased_events_index0_as_log(self):
        src = _wrap(
            "\tevents := in.Events\n"
            "\tlog := events[0].(*models.Log)\n"
            "\t_ = log\n"
        )
        vs = lint.find_violations(src, "plugins/processor/foo/foo.go")
        self.assertTrue(vs, "aliased Events[0] as Log must be flagged")
        self.assertTrue(any("Log" in v.reason or "index" in v.reason for v in vs))

    def test_allows_range_over_events(self):
        src = _wrap(
            "\tfor _, event := range in.Events {\n"
            "\t\tif log, ok := event.(*models.Log); ok {\n"
            "\t\t\t_ = log\n"
            "\t\t}\n"
            "\t}\n"
        )
        self.assertEqual(lint.find_violations(src, "plugins/processor/foo/foo.go"), [])

    def test_allows_process_log_events_only(self):
        src = _wrap("\tpipeline.ProcessLogEventsOnly(in, ctx, p.processLogEvent)\n")
        self.assertEqual(lint.find_violations(src, "plugins/processor/foo/foo.go"), [])

    def test_allows_collect_group_events(self):
        src = _wrap("\tpipeline.CollectGroupEvents(ctx, in)\n")
        self.assertEqual(lint.find_violations(src, "plugins/processor/foo/foo.go"), [])

    def test_allows_empty_events_guard(self):
        src = _wrap(
            "\tif in == nil || len(in.Events) == 0 {\n"
            "\t\treturn\n"
            "\t}\n"
            "\tfor _, event := range in.Events {\n"
            "\t\t_ = event\n"
            "\t}\n"
        )
        self.assertEqual(lint.find_violations(src, "plugins/processor/foo/foo.go"), [])

    def test_ignores_events0_in_comment_and_string(self):
        src = _wrap(
            "\t// in.Events[0].(*models.Log)\n"
            '\t_ = "in.Events[0].(*models.Log)"\n'
            "\tfor _, event := range in.Events {\n"
            "\t\t_ = event\n"
            "\t}\n"
        )
        self.assertEqual(lint.find_violations(src, "plugins/processor/foo/foo.go"), [])

    def test_ignores_non_pipeline_group_process(self):
        src = (
            "package foo\n\n"
            "func (p *Processor) Process(fileBlock []byte, d time.Duration) int {\n"
            "\t_ = errors.New(\"Events[0]\")\n"
            "\treturn 0\n"
            "}\n"
        )
        self.assertEqual(lint.find_violations(src, "plugins/input/foo.go"), [])

    def test_ignores_processlogs(self):
        src = (
            "package foo\n\n"
            "func (p *ProcessorFoo) ProcessLogs(logArray []*protocol.Log) []*protocol.Log {\n"
            "\t_ = logArray[0]\n"
            "\treturn logArray\n"
            "}\n"
        )
        self.assertEqual(lint.find_violations(src, "plugins/processor/foo/foo.go"), [])

    def test_multiline_signature(self):
        src = (
            "package foo\n\n"
            "func (p *ProcessorFoo) Process(\n"
            "\tin *models.PipelineGroupEvents,\n"
            "\tctx pipeline.PipelineContext,\n"
            ") {\n"
            "\tlog := in.Events[0].(*models.Log)\n"
            "\t_ = log\n"
            "}\n"
        )
        vs = lint.find_violations(src, "plugins/processor/foo/foo.go")
        self.assertEqual(len(vs), 1)
        self.assertIn("Log", vs[0].reason)


class AllowlistAndScanTest(unittest.TestCase):
    def test_load_allowlist_skips_comments(self):
        tmp = tempfile.mkdtemp()
        try:
            path = Path(tmp) / "allow.txt"
            path.write_text(
                "# comment\n\nplugins/processor/foo/foo.go\n"
                "plugins/processor/bar/bar.go:12\n",
                encoding="utf-8",
            )
            keys = lint.load_allowlist(path)
            self.assertEqual(
                keys,
                {
                    "plugins/processor/foo/foo.go",
                    "plugins/processor/bar/bar.go:12",
                },
            )
        finally:
            # Path.unlink / rmtree not required; process tmp is cleaned by OS.
            pass

    def test_file_allowlist_suppresses_violation(self):
        v = lint.Violation(
            path="plugins/processor/foo/foo.go",
            line=9,
            snippet="log := in.Events[0].(*models.Log)",
            reason="x",
        )
        self.assertTrue(lint.is_allowlisted(v, {"plugins/processor/foo/foo.go"}))
        self.assertTrue(lint.is_allowlisted(v, {"plugins/processor/foo/foo.go:9"}))
        self.assertFalse(lint.is_allowlisted(v, {"plugins/processor/foo/foo.go:10"}))
        self.assertFalse(lint.is_allowlisted(v, set()))

    def test_scan_repo_reports_and_skips_tests(self):
        tmp = tempfile.mkdtemp()
        root = Path(tmp)
        prod = root / "plugins" / "processor" / "foo"
        prod.mkdir(parents=True)
        (prod / "foo.go").write_text(
            _wrap("\tlog := in.Events[0].(*models.Log)\n\t_ = log\n"),
            encoding="utf-8",
        )
        (prod / "foo_test.go").write_text(
            _wrap("\t_ = in.Events[0].(*models.Log)\n"),
            encoding="utf-8",
        )
        ok = root / "plugins" / "processor" / "bar"
        ok.mkdir(parents=True)
        (ok / "bar.go").write_text(
            _wrap("\tpipeline.ProcessLogEventsOnly(in, ctx, func(l *models.Log) {})\n"),
            encoding="utf-8",
        )
        vs = lint.scan_repo(root, set())
        self.assertEqual(len(vs), 1)
        self.assertEqual(vs[0].path, "plugins/processor/foo/foo.go")

        vs_allow = lint.scan_repo(root, {"plugins/processor/foo/foo.go"})
        self.assertEqual(vs_allow, [])

    def test_main_ok_on_clean_tree(self):
        tmp = tempfile.mkdtemp()
        root = Path(tmp)
        (root / "plugins" / "processor" / "ok").mkdir(parents=True)
        (root / "plugins" / "processor" / "ok" / "ok.go").write_text(
            _wrap("\tfor _, e := range in.Events {\n\t\t_ = e\n\t}\n"),
            encoding="utf-8",
        )
        allow = root / "allow.txt"
        allow.write_text("# empty\n", encoding="utf-8")
        rc = lint.main(["--root", str(root), "--allowlist", str(allow)])
        self.assertEqual(rc, 0)

    def test_main_fails_on_violation(self):
        tmp = tempfile.mkdtemp()
        root = Path(tmp)
        d = root / "plugins" / "processor" / "bad"
        d.mkdir(parents=True)
        (d / "bad.go").write_text(
            _wrap("\t_ = in.Events[0].(*models.Log)\n"),
            encoding="utf-8",
        )
        allow = root / "allow.txt"
        allow.write_text("# empty\n", encoding="utf-8")
        with open(os.devnull, "w") as devnull:
            old_err = sys.stderr
            sys.stderr = devnull
            try:
                rc = lint.main(["--root", str(root), "--allowlist", str(allow)])
            finally:
                sys.stderr = old_err
        self.assertEqual(rc, 1)


class RepoBaselineTest(unittest.TestCase):
    def test_in_tree_process_methods_are_clean(self):
        """B4 left no Events[0]-as-Log Process methods; allowlist stays empty."""
        root = Path(__file__).resolve().parents[1]
        allow = lint.load_allowlist(root / "scripts" / "lint_process_events0_allowlist.txt")
        vs = lint.scan_repo(root, allow)
        self.assertEqual(
            vs,
            [],
            msg="unexpected Process Events[0] findings: "
            + ", ".join("{}:{}".format(v.path, v.line) for v in vs),
        )


if __name__ == "__main__":
    unittest.main()
