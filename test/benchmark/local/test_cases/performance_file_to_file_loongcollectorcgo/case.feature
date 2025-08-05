@input
Feature: performance file to file LoongCollector-CGo
  Performance file to file LoongCollector-CGo

  @e2e-performance @docker-compose @loongcollector-cgo-file
  Scenario: PerformanceFileToFileLoongCollectorCGo
    Given {docker-compose} environment
    Given docker-compose boot type {benchmark}
    When start docker-compose {performance_file_to_file_loongcollectorcgo}
    When start monitor {LoongCollectorCGo}, with timeout {6} min
    When generate random nginx logs to file, speed {10}MB/s, total {5}min, to file {./test_cases/performance_file_to_file_loongcollectorcgo/a.log}
    When wait monitor until log processing finished
