@input
Feature: performance file to file filebeat
  Performance file to file filebeat

  @e2e-performance @docker-compose @filebeat-file
  Scenario: PerformanceFileToFileFilebeat
    Given {docker-compose} environment
    Given docker-compose boot type {benchmark}
    When start docker-compose {performance_file_to_file_filebeat}
    When start monitor {filebeat}, with timeout {6} min
    When generate random nginx logs to file, speed {10}MB/s, total {5}min, to file {./test_cases/performance_file_to_file_filebeat/a.log}
    When wait monitor until log processing finished