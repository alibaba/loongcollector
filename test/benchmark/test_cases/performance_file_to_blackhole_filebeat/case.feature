@input
Feature: performance file to blackhole filebeat
  Performance file to blackhole filebeat

  @e2e-performance @docker-compose
  Scenario: PerformanceFileToBlackholeFilebeat
    Given {docker-compose} environment
    Given docker-compose boot type {benchmark}
    When start docker-compose {performance_file_to_blackhole_filebeat}
    When start monitor {filebeat}
    When generate random nginx logs to file, speed {10}MB/s, total {3}min, to file {./test_cases/performance_file_to_blackhole_filebeat/a.log}
    When wait monitor until log processing finished
