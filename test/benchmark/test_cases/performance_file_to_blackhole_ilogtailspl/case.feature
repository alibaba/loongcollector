@input
Feature: performance file to blackhole iLogtail
  Performance file to blackhole iLogtail

  @e2e-performance @docker-compose
  Scenario: PerformanceFileToBlackholeiLogtailSPL
    Given {docker-compose} environment
    Given docker-compose boot type {benchmark}
    When start docker-compose {performance_file_to_blackhole_ilogtailspl}
    When start monitor {ilogtailC}
    When generate random nginx logs to file, speed {10}MB/s, total {3}min, to file {./test_cases/performance_file_to_blackhole_ilogtailspl/a.log}
    When wait monitor until log processing finished
