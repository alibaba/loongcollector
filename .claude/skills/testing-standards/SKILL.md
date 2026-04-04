---
name: testing-standards
description: Testing standards for LoongCollector: unit tests, e2e tests, benchmarks. Reference when writing or reviewing tests.
---
# LoongCollector Testing Standards

## Test Categories

### 1. Unit Tests (C++)
- Use Google Test (GTest) / Google Mock
- Place in `core/unittest/`
- Cover success and failure paths
- Core logic must have 100% coverage
- Test boundary conditions explicitly
- Test naming: accurately describe behavior being tested
- Each `core/unittest/*/` directory produces one executable
- Build and run tests from inside `build/` to ensure relative paths and temp files work correctly
- See `.claude/skills/compile/SKILL.md` for build & run instructions

### 2. Unit Tests (Go)
- Use standard `testing` package
- Table-driven tests for function coverage
- Integration tests via E2E framework

### 3. E2E Tests
- BDD Godog framework
- Configuration-driven via `.feature` files
- See `.claude/skills/e2e-develop-guide/` and `.claude/skills/e2e-manual/` for details

### 4. Benchmarks
- Required for performance-sensitive code paths
- Compare against baseline versions
- Measure throughput, latency, CPU/Memory usage

## E2E Test Quick Reference

### Feature File Structure
```
@input
Feature: input file
  Test input file

  @e2e @host
  Scenario: TestInputFileWithRegexSingle
    Given {host} environment
    Given subcribe data from {sls} with config
    """
    enable: true
    inputs:
      - Type: input_file
    """
    When generate {100} regex logs to file {/tmp/loongcollector/regex_single.log}, with interval {100}ms
    Then there is {100} logs
```

### Behavior Types
| Type | Purpose |
|------|---------|
| `Given` | Setup/prepare test conditions |
| `When` | Trigger test actions (e.g., log generation) |
| `Then` | Verify test results |

### Environment Tags
- `@host` - Host environment
- `@k8s` - Kubernetes environment
- `@docker-compose` - Docker Compose environment
- `@e2e` - E2E test marker
- `@regression` - Regression test marker

### Adding New Test Behaviors
1. Write the Go function in the appropriate directory:
   - `cleanup/` - Post-test cleanup (auto-executed)
   - `control/` - Control operations (init, config)
   - `setup/` - Environment setup
   - `trigger/` - Data generation
   - `verify/` - Result verification
2. Function signature: `func Name(ctx context.Context, params...) (context.Context, error)`
3. Register in `test/e2e_enterprise/main_test.go` via `scenarioInitializer`
4. Use in feature files with `{param}` syntax

### Strict Rules
- Do NOT change behavior of the method being tested
- Do NOT modify existing test behaviors in engine
- Always start trigger `When begin trigger` BEFORE generating logs
- Only use registered behaviors from `test/engine/steps.go`
- Verify behavior type matches (Given/When/Then)

### Test Naming
- Format: `Test${FunctionName}${CaseBriefDescription}`
- Examples: `TestInputFileWithBlackListDir`, `TestInputFileWithRegexSingle`
- Must include `@e2e` and environment tags

## Benchmark Testing

For performance-sensitive code:
1. Provide baseline comparison
2. Measure: throughput, latency, CPU profile, memory profile
3. Run under realistic load conditions
4. Document methodology and results
