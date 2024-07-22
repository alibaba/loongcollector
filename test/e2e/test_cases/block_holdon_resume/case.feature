@input
Feature: block holdon resume
  Test block holdon resume

  @e2e-core @docker-compose
  Scenario: TestBlockHoldonResume
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    When start docker-compose {block_holdon_resume}
    Given {block-case} local config as below
    """
    enable: true
    global:
      InputIntervalMs: 10000
      AggregatIntervalMs: 1000
      FlushIntervalMs: 300
      DefaultLogQueueSize: 1000
      DefaultLogGroupQueueSize: 10
    inputs:
      - Type: service_mock
        LogsPerSecond: 100000
        MaxLogCount: 200000
        Fields:
          content: >
            time:2017.09.12 20:55:36 json:{"array" : [1, 2, 3, 4], "key1" : "xx",
            "key2": false, "key3":123.456, "key4" : { "inner1" : 1, "inner2" :
            {"xxxx" : "yyyy", "zzzz" : "中文"}}}
    processors:
      - Type: processor_anchor
        SourceKey: content
        NoAnchorError: true
        Anchors:
          - Start: time
            Stop: ' '
            FieldName: time
            FieldType: string
            ExpondJson: false
          - Start: 'json:'
            Stop: |+
            FieldName: val
            FieldType: json
            ExpondJson: true
            MaxExpondDepth: 2
            ExpondConnecter: '#'
    flushers:
      - Type: flusher_checker
        Block: true
    """
    Then wait {10} seconds
    Given remove http config {block-case/1}
    Then wait {10} seconds
    # Resume
    Then the logtail log contains {1} times of {[Resume] Resume:start}
    Then the logtail log contains {1} times of {[Resume] checkpoint:Resume}
    Then the logtail log contains {1} times of {[Resume] Resume:success}
    Then the logtail log contains {1} times of {[Start] [shennong_log_profile,logtail_plugin_profile]	config start:success}
    Then the logtail log contains {1} times of {[Start] [logtail_alarm,logtail_alarm]	config start:success}
    Then the logtail log contains {1} times of {[Start] [block-case/1]	config start:success}
    # Holdon
    Then the logtail log contains {1} times of {[HoldOn] Hold on:start	flag:0}
    Then the logtail log contains {1} times of {[HoldOn] Hold on:start	flag:1}
    Then the logtail log contains {1} times of {[HoldOn] checkpoint:HoldOn}
    Then the logtail log contains {2} times of {[HoldOn] Hold on:success}
    Then the logtail log contains {1} times of {[Stop] [shennong_log_profile,logtail_plugin_profile]	config stop:begin	exit:true}
    Then the logtail log contains {1} times of {[Stop] [shennong_log_profile,logtail_plugin_profile]	config stop:success}
    Then the logtail log contains {1} times of {[Stop] [logtail_alarm,logtail_alarm]	config stop:begin	exit:true}
    Then the logtail log contains {1} times of {[Stop] [logtail_alarm,logtail_alarm]	config stop:success}
    Then the logtail log contains {1} times of {[block-case/1]	Stop config in goroutine:begin}
    Then the logtail log contains {1} times of {[Stop] [block-case/1]	config stop:begin	exit:true}
    Then the logtail log contains {1} times of {[block-case/1]	AlarmType:CONFIG_STOP_TIMEOUT_ALARM	timeout when stop config, goroutine might leak:}
