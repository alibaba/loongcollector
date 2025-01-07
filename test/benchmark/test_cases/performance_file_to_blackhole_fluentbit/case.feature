@input
Feature: performance file to blackhole fluentbit
  Performance file to blackhole fluentbit

  @e2e-performance @docker-compose
  Scenario: PerformanceFileToBlackholeFluentbit
    Given {docker-compose} environment
    Given docker-compose boot type {benchmark}
    When start docker-compose {performance_file_to_blackhole_fluentbit}
    When start monitor {fluent-bit}
    When generate random nginx logs to file, speed {10}MB/s, total {3}min, to file {./test_cases/performance_file_to_blackhole_fluentbit/a.log}
    When wait monitor until log processing finished
