@input
Feature: performance file to blackhole vector
  Performance file to blackhole vector

  @e2e-performance @docker-compose @vector
  Scenario: PerformanceFileToBlackholeVector
    Given {docker-compose} environment
    Given docker-compose boot type {benchmark}
    When start docker-compose {performance_file_to_blackhole_vector}
    When start monitor {vector}, with timeout {6} min
    When generate random nginx logs to file, speed {10}MB/s, total {5}min, to file {./test_cases/performance_file_to_blackhole_vector/a.log}
    When wait monitor until log processing finished
