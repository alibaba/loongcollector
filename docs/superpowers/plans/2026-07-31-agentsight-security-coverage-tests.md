# AgentSight Security Coverage Tests Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add behavior-level tests that raise AgentSight security-event diff coverage above the repository's 60% threshold.

**Architecture:** Extend the existing AgentSight manager and input fixtures. Manager tests exercise the real `CommonEvent -> ProcessQueueItem -> LogEvent` path; input tests exercise JSON configuration validation without exposing production helpers.

**Tech Stack:** C++17, GoogleTest-compatible `APSARA_TEST_*` assertions, LoongCollector `ProcessQueueManager`, JSONCPP.

## Global Constraints

- Keep production interfaces and runtime behavior unchanged.
- Use the existing fake AgentSight FFI table; do not require a live AgentSight library or enforcer.
- Isolate singleton process-queue state within each test.
- Assert observable output fields and validation results, not implementation call counts alone.

---

### Task 1: Security-event conversion behavior

**Files:**
- Modify: `core/unittest/ebpf/AgentsightManagerUnittest.cpp`

**Interfaces:**
- Consumes: `AgentsightManager::HandleEvent(const std::shared_ptr<CommonEvent>&)`, `AgentsightSecurityRecord`, `ProcessQueueManager::PopItem`.
- Produces: tests for valid and malformed security JSON conversion.

- [ ] **Step 1: Add queue cleanup and a registered-queue helper**

Include `QueueKeyManager.h` and `ProcessQueueManager.h`. Track created queue keys in the fixture, delete them in `TearDown`, and add a helper that creates/enables a count-bounded queue before registering the manager config.

```cpp
QueueKey registerConfigWithQueue(AgentsightManager& mgr, const char* configName) {
    const QueueKey key = QueueKeyManager::GetInstance()->GetKey(configName);
    CollectionPipelineContext ctx;
    ctx.SetConfigName(configName);
    ctx.SetProcessQueueKey(key);
    APSARA_TEST_TRUE(ProcessQueueManager::GetInstance()->CreateOrUpdateCountBoundedQueue(key, 0, ctx));
    ProcessQueueManager::GetInstance()->EnablePop(configName);
    APSARA_TEST_EQUAL(0, mgr.AddOrUpdateConfig(&ctx, 0, nullptr, asVariant()));
    mProcessQueueKeys.push_back(key);
    return key;
}
```

- [ ] **Step 2: Write the valid-event behavior test**

Create a `policy_decision` payload containing event identifiers, timestamps, full identity fields, nested strings, booleans, signed/unsigned integers, and a double. Send the record through `HandleEvent`, pop the queue item, and assert representative fields.

```cpp
APSARA_TEST_EQUAL("agentsight.security.policy_decision", log.GetContent("event.name").to_string());
APSARA_TEST_EQUAL("policy_decision", log.GetContent("event.type").to_string());
APSARA_TEST_EQUAL("agent-1", log.GetContent("agent.id").to_string());
APSARA_TEST_EQUAL("85", log.GetContent("security.risk_score").to_string());
APSARA_TEST_EQUAL("true", log.GetContent("security.blocked").to_string());
APSARA_TEST_EQUAL(payload, log.GetContent("event.original").to_string());
```

- [ ] **Step 3: Verify the valid-event test detects a mutation**

Temporarily change one asserted production mapping, such as `event.type`, to a different key. Build/run `agentsight_manager_unittest` and expect the new test to fail on the missing field. Restore the production line immediately and confirm `git diff` contains only test changes.

- [ ] **Step 4: Write the malformed-JSON fallback test**

Send `{not-json` through the same queue path and assert the event is still emitted with `event.name=agentsight.security`, `event.kind=event`, schema version, timestamp, and exact `event.original`, while parsed fields such as `event.id` remain empty.

- [ ] **Step 5: Run targeted manager tests**

Run the repository's Linux unit-test build for `agentsight_manager_unittest`, then execute the binary. Expected: both new tests and all existing manager tests pass.

- [ ] **Step 6: Commit manager tests**

```bash
git add core/unittest/ebpf/AgentsightManagerUnittest.cpp
git commit -m "test(agentsight): cover security event conversion"
```

### Task 2: SecurityAudit configuration validation

**Files:**
- Modify: `core/unittest/input/InputAgentSightUnittest.cpp`

**Interfaces:**
- Consumes: `InputAgentSight::Init(const Json::Value&, Json::Value&)`.
- Produces: tests for default, malformed, and empty-socket `SecurityAudit` configuration.

- [ ] **Step 1: Add the default-socket test**

Initialize with `"SecurityAudit":{"Enabled":true}` and assert initialization succeeds, security audit is enabled, and the socket remains `/run/agentsight/enforcer.sock`.

- [ ] **Step 2: Add invalid configuration tests**

Initialize separate inputs with `"SecurityAudit":true`, `"SecurityAudit":{"Enabled":"yes"}`, and `"SecurityAudit":{"EnforcerSocket":""}`. Assert each initialization fails.

- [ ] **Step 3: Verify a configuration test detects a mutation**

Temporarily change the default socket initialization in `SecurityOptions::Init` to a different path. Run `input_ebpf_agentsight_unittest` and expect the default-socket assertion to fail. Restore the production line immediately.

- [ ] **Step 4: Run targeted input tests**

Build and execute `input_ebpf_agentsight_unittest`. Expected: all existing and new cases pass.

- [ ] **Step 5: Commit configuration tests**

```bash
git add core/unittest/input/InputAgentSightUnittest.cpp
git commit -m "test(agentsight): cover security audit validation"
```

### Task 3: Verification and PR update

**Files:**
- Verify: all committed files in the branch.

**Interfaces:**
- Consumes: the two test commits.
- Produces: updated PR #2670 with passing coverage-oriented tests.

- [ ] **Step 1: Run local static verification**

Run `git diff --check`, clang-format dry-run for both modified C++ test files, `make check-license`, and the security-check push scan.

- [ ] **Step 2: Inspect the final diff**

Confirm only the two test files and the approved design/plan documents were added after the prior fix. Preserve unrelated untracked files.

- [ ] **Step 3: Publish the branch**

Update `codex/agentsight-unified-events` on the fork and verify the remote tree SHA equals the local tree SHA.

- [ ] **Step 4: Monitor CI**

Confirm `BuildCoreUT` diff coverage is at least 60%. Once the E2E workflow permits reruns, rerun the failed `input_static_file` job and verify whether the Docker Hub timeout clears.
