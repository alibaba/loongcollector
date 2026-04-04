---
name: e2e-manual
description: E2E testing standards guide. Use when writing or executing E2E tests, including naming conventions, test workflows, and behavior templates.
---
# E2E Manual Testing Guide

## Test Framework

- E2E (BDD Godog)

## Strict Rules

- **Do not change** the behavior of the method being tested
- **Do not modify** existing test behaviors in engine
- If logging is needed, **always start the trigger** `When begin trigger` BEFORE generating logs
- Templates are registered in `test/engine/steps.go`. Please only use registered test behaviors. Do NOT generate your own. Ensure behavior type matches (e.g., `wait {10} seconds` is a `Then` behavior, so it must be `Then wait {10} seconds`, not `When`)

## Test Naming Conventions

- Each case must be marked with `@e2e` annotation
- Must annotate environment: `@host` (host environment), `@k8s` (K8s environment), `@docker-compose` (Docker Compose environment)
- Format: `Test${FunctionName}${CaseBriefDescription}`
- Examples: `TestInputFileWithBlackListDir`, `TestInputFileWithRegexSingle`

## Test Best Practices

### Test Flow

1. **Create test environment**: `Given {host} environment`
   - `host` = build host environment
   - `daemonset` = build K8s environment
   - `docker-compose` = container environment
   - Only these three values, do not use others

2. **Start iLogtail**: `Given subcribe data from {sls} with config`
   - Subscribe plugin receives data
   - `sls` means output to logstore
   - `Given {regex_single} local config as below` adds local config, starts input/processor plugins
   - Config must contain `enable: true`, otherwise it won't execute

3. **Trigger log generation**: `When begin trigger`
   - Use existing framework to generate events, e.g., `When generate {100} regex logs to file {/tmp/loongcollector/regex_single.log}, with interval {100}ms`

4. **Verify log content**: Framework fetches collected logs, queries them, then validates with `Then` behaviors
   - Query example: `When query through {* | select * from e2e where call_name = 'tcp_connect' and arguments like '%http://www.baidu.com%'}`

### Test Focus

- Design query statements according to feature requirements
- Focus on query return values to ensure functionality meets expectations
- Keep tests concise and maintainable
