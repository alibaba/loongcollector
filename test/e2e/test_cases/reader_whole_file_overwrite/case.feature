@input
Feature: reader whole file overwrite
  Test whole_file collection mode with overwrite file write mode (host environment)

  # whole_file: the whole file is collected as ONE log record.
  # FileWriteMode/MaxWholeFileBytes MUST be nested inside the Multiline block
  # (same level as Mode), because InputFile passes the Multiline sub-object to
  # MultilineOptions::Init and those params are read from that node. Putting them
  # at the same level as Multiline makes parsing fall back to append mode.
  #
  # Behaviors covered:
  #   1. whole file == one record: a small multi-line file is collected as a
  #      SINGLE log whose content equals the whole file content (the trailing
  #      newline is trimmed by the reader).
  #   2. overwrite triggers a full re-read: the same file is rewritten in place
  #      with different (larger) content, so mtime/size/signature change and the
  #      reader re-reads the WHOLE new file (not just an appended tail).
  #
  # Host-mode flow: the agent is already running on the host; the config is
  # delivered into the local config dir, then the file under test is seeded with
  # the "alpha" content (3 lines) and later overwritten IN PLACE with the "beta"
  # content (4 lines) via `run command on loongcollector`. Hence exactly 2
  # records are expected, and each record must equal one of the two whole-file
  # contents (proving each file collapses to a single record).
  #
  # TODO(optional, heavy): large file (>512KB) chunked drain with line/char
  # alignment is covered by core unit tests (WholeFileOverwriteLargeUnittest). It
  # is intentionally not reproduced here because constructing and asserting the
  # per-chunk boundaries through the subscriber is not supported by the current
  # E2E step set.

  @e2e @host
  Scenario: TestReaderWholeFileOverwrite
    Given {host} environment
    Given subcribe data from {sls} with config
    """
    """
    Given {reader-whole-file-overwrite-case} local config as below
    """
    enable: true
    inputs:
      - Type: input_file
        FilePaths:
          - /tmp/loongcollector/whole_file_overwrite/*.log
        FlushTimeoutSecs: 2
        Multiline:
          Mode: whole_file
          FileWriteMode: overwrite
          MaxWholeFileBytes: 10485760
    """
    Given run command on loongcollector {mkdir -p /tmp/loongcollector/whole_file_overwrite && rm -f /tmp/loongcollector/whole_file_overwrite/a.log}
    When begin trigger
    Given run command on loongcollector {printf 'alpha-line-1\nalpha-line-2\nalpha-line-3\n' > /tmp/loongcollector/whole_file_overwrite/a.log}
    Then wait {25} seconds
    Given run command on loongcollector {printf 'beta-line-1\nbeta-line-2\nbeta-line-3\nbeta-line-4\n' > /tmp/loongcollector/whole_file_overwrite/a.log}
    Then wait {15} seconds
    Then there is {2} logs
    Then the log fields match kv
    """
    content: "^(alpha-line-1\nalpha-line-2\nalpha-line-3|beta-line-1\nbeta-line-2\nbeta-line-3\nbeta-line-4)\n?$"
    """
