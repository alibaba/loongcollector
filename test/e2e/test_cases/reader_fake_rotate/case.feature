@input
Feature: reader fake rotate inode reuse
  Scheme C (PR #2646) regression gate: different-path inode reuse must not
  re-collect after config reload. Same-path + same-inode recreation is a
  known limitation of scheme C and is intentionally not asserted here.

  Two watched directories share one config. The sidecar writes new/sub/app.log
  (inode X), waits for CloseTimeoutFilePtr, then recreates inode X at
  old/app.log. A real rotate (old/app.log -> old/rotated.log) plus a new
  old/app.log moves the active reader into the rotator map.

  DumpAllHandlersMeta writes rotators first, then actives. Last-writer-wins
  therefore keeps the stale new/sub checkpoint on a pre-#2646 baseline
  (duplicates after reload). Scheme C keeps the checkpoint whose file still
  exists (rotated.log), so the new lines are not re-collected.

  @e2e @docker-compose
  Scenario: TestReaderFakeRotateInodeReuseDifferentPath
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given loongcollector container environment {ALIYUN_LOGTAIL_CHECK_HANDLER_TIMEOUT_INTERVAL} {2}
    Given loongcollector container environment {LOONG_CHECK_HANDLER_TIMEOUT_INTERVAL} {2}
    Given loongcollector container environment {ALIYUN_LOGTAIL_CONFIG_SCAN_INTERVAL} {3}
    Given loongcollector container environment {LOONG_CONFIG_SCAN_INTERVAL} {3}
    Given {reader-fake-rotate-inode-reuse} local config as below
    """
    enable: true
    inputs:
      - Type: input_file
        FilePaths:
          - /root/test/new/sub/*.log
          - /root/test/old/*.log
        FlushTimeoutSecs: 1
        CloseUnusedReaderIntervalSec: 2
    """
    Given loongcollector container mount {./volume} to {/root/test}
    Given run command on datasource {mkdir -p test_cases/reader_fake_rotate/volume/old test_cases/reader_fake_rotate/volume/new/sub && rm -f test_cases/reader_fake_rotate/volume/inode_* test_cases/reader_fake_rotate/volume/scene_ready test_cases/reader_fake_rotate/volume/old/*.log test_cases/reader_fake_rotate/volume/new/sub/*.log}
    When start docker-compose {reader_fake_rotate}
    Given run command on datasource {sh test_cases/reader_fake_rotate/wait_scene_ready.sh}
    Then wait {5} seconds
    Given {reader-fake-rotate-inode-reuse} local config as below
    """
    enable: true
    inputs:
      - Type: input_file
        FilePaths:
          - /root/test/new/sub/*.log
          - /root/test/old/*.log
        FlushTimeoutSecs: 2
        CloseUnusedReaderIntervalSec: 2
    """
    Then wait {20} seconds
    Then there is {11} logs
    Then the log fields match kv
    """
    content: "^(old-[1-5]|new-[1-5]|extra-1)$"
    """
    Then the log fields have exact kv
    """
    - content: "old-1"
    - content: "old-2"
    - content: "old-3"
    - content: "old-4"
    - content: "old-5"
    - content: "new-1"
    - content: "new-2"
    - content: "new-3"
    - content: "new-4"
    - content: "new-5"
    - content: "extra-1"
    """
    Then collected log field {content} has no duplicates
