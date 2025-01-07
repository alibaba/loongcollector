@input
Feature: performance file to blackhole iLogtail
  Performance file to blackhole iLogtail

  @e2e-performance @docker-compose
  Scenario: PerformanceFileToBlackholeiLogtail
    Given {docker-compose} environment
    Given docker-compose boot type {benchmark}
    When start docker-compose {performance_file_to_blackhole_ilogtail}
    When start monitor {ilogtailC}
    When generate random nginx logs to file, speed {10}MB/s, total {3}min, to file {./test_cases/performance_file_to_blackhole_ilogtail/a.log}
    When wait monitor until log processing finished
