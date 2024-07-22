@input
Feature: normal holdon resume
  Test normal holdon resume

  @e2e-core @docker-compose
  Scenario: TestNormalHoldonResume
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    When start docker-compose {normal_holdon_resume}
    Given {mock-metric-case} local config as below
    """
    enable: true
    inputs:
      - Type: metric_mock
        IntervalMs: 1000
        Tags:
          tag1: aaaa
          tag2: bbb
        Fields:
          content: xxxxx
          time: '2017.09.12 20:55:36'
    """
    Then wait {15} seconds
    Given remove http config {mock-metric-case/1}
    Then there is at least {5} logs
    # Resume
    Then the logtail log contains {1} times of {[Resume] Resume:start}
    Then the logtail log contains {1} times of {[Resume] checkpoint:Resume}
    Then the logtail log contains {1} times of {[Resume] Resume:success}
    Then the logtail log contains {1} times of {[Start] [shennong_log_profile,logtail_plugin_profile]	config start:success}
    Then the logtail log contains {1} times of {[Start] [logtail_alarm,logtail_alarm]	config start:success}
    Then the logtail log contains {1} times of {[Start] [mock-metric-case/1]	config start:success}
    # Holdon
    Then the logtail log contains {1} times of {[HoldOn] Hold on:start	flag:0}
    Then the logtail log contains {1} times of {[HoldOn] Hold on:start	flag:1}
    Then the logtail log contains {1} times of {[HoldOn] checkpoint:HoldOn}
    Then the logtail log contains {2} times of {[HoldOn] Hold on:success}
    Then the logtail log contains {1} times of {[Stop] [shennong_log_profile,logtail_plugin_profile]	config stop:begin	exit:true}
    Then the logtail log contains {1} times of {[Stop] [shennong_log_profile,logtail_plugin_profile]	config stop:success}
    Then the logtail log contains {1} times of {[Stop] [logtail_alarm,logtail_alarm]	config stop:begin	exit:true}
    Then the logtail log contains {1} times of {[Stop] [logtail_alarm,logtail_alarm]	config stop:success}
    Then the logtail log contains {1} times of {[mock-metric-case/1]	Stop config in goroutine:begin}
    Then the logtail log contains {1} times of {[Stop] [mock-metric-case/1]	config stop:begin	exit:true}
    Then the logtail log contains {1} times of {[Stop] [mock-metric-case/1]	config stop:success}
