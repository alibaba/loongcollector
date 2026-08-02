# AgentSight Security Correlation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expose stable AgentSight security identity fields that let AgentLoop require matching application and system evidence before creating a leakage incident.

**Architecture:** Extend the existing `input_agentsight` security-event conversion with additive canonical aliases. Keep raw collection in LoongCollector and perform stateful cross-Logstore matching in SLS Scheduled SQL; records with incomplete identity remain evidence and cannot create a formal leakage incident.

**Tech Stack:** C++17, JSONCPP, LoongCollector `PipelineEventGroup`, GoogleTest-compatible `APSARA_TEST_*`, SLS Scheduled SQL.

## Global Constraints

- Base the branch on PR #2670 and do not modify that PR's branch.
- Do not change the AgentSight FFI ABI or create a second input plugin.
- Do not cache or correlate application events inside LoongCollector.
- Do not synthesize missing session, conversation, tool-call, process, or binding identities.
- Preserve `event.original` exactly.
- Require application and system evidence for a formal leakage incident.

---

### Task 1: Canonical correlation identity

**Files:**
- Modify: `core/unittest/ebpf/AgentsightManagerUnittest.cpp`
- Modify: `core/ebpf/plugin/agentsight/AgentsightManager.cpp`

**Interfaces:**
- Consumes: AgentSight security JSON `identity` object.
- Produces: `gen_ai.conversation.id`, `gen_ai.tool.call.id`, `process.start_time`, `process.parent.pid`, `container.cgroup.id`, and `agentsight.binding.id` log fields.

- [ ] **Step 1: Extend the real queue-path test with complete identity**

Change `TestSecurityEventProducesSearchableLog` so its payload contains:

```json
{
  "identity": {
    "binding_id": "10000000-0000-0000-0000-000000000001",
    "agent_id": "agent-1",
    "agent_name": "claude",
    "session_id": "session-1",
    "conversation_id": "conversation-1",
    "tool_call_id": "tool-call-1",
    "pid": 42,
    "process_start_time": 101,
    "ppid": 7,
    "cgroup_id": 9001
  }
}
```

Assert the exact canonical field values while retaining the existing
`gen_ai.turn.id` assertion for compatibility.

- [ ] **Step 2: Run the targeted test and verify RED**

Run on the Linux test host:

```bash
./scripts/run_core_ut.sh \
  --gtest_filter=AgentsightManagerUnittest.TestSecurityEventProducesSearchableLog \
  unittest/ebpf/agentsight_manager_unittest
```

Expected: FAIL because `gen_ai.conversation.id` and the other new canonical
fields are empty.

- [ ] **Step 3: Add the minimal typed mappings**

Inside the existing `identity.isObject()` block in
`AgentsightManager::HandleSecurityEvent`, add only these mappings:

```cpp
if (identity["binding_id"].isString()) {
    log->SetContent("agentsight.binding.id", identity["binding_id"].asString());
}
if (identity["conversation_id"].isString()) {
    log->SetContent("gen_ai.conversation.id", identity["conversation_id"].asString());
}
if (identity["tool_call_id"].isString()) {
    log->SetContent("gen_ai.tool.call.id", identity["tool_call_id"].asString());
}
if (identity["process_start_time"].isUInt64()) {
    log->SetContent("process.start_time", std::to_string(identity["process_start_time"].asUInt64()));
}
if (identity["ppid"].isInt()) {
    log->SetContent("process.parent.pid", std::to_string(identity["ppid"].asInt()));
}
if (identity["cgroup_id"].isUInt64()) {
    log->SetContent("container.cgroup.id", std::to_string(identity["cgroup_id"].asUInt64()));
}
```

- [ ] **Step 4: Run the targeted test and verify GREEN**

Run the command from Step 2. Expected: PASS with one selected test and zero
failures.

- [ ] **Step 5: Run the complete AgentSight manager test binary**

```bash
./scripts/run_core_ut.sh unittest/ebpf/agentsight_manager_unittest
```

Expected: all existing and new AgentSight manager cases pass.

### Task 2: Correlation contract documentation

**Files:**
- Modify: `docs/cn/plugins/input/native/input_agentsight.md`

**Interfaces:**
- Consumes: the canonical fields from Task 1.
- Produces: operator-facing field contract and strict-correlation requirements.

- [ ] **Step 1: Expand the system-audit field list**

Document the six new canonical fields and state that optional producer values
remain absent rather than being guessed.

- [ ] **Step 2: Document the strict evidence gate**

Add a short section stating that a formal leakage incident requires:

```text
application secret/PII evidence
AND matching agent.id + gen_ai.session.id
AND matching gen_ai.tool.call.id for tool-originated exposure
AND AgentSight source -> outbound sink -> policy_decision chain
AND a bounded 120-second causal window
```

Explain that missing keys produce evidence-only records and that `blocked`
describes enforcement outcome, not confirmed transfer.

- [ ] **Step 3: Run documentation and license checks**

```bash
make check-license
git diff --check
```

Expected: both commands exit 0.

### Task 3: SLS strict dual-evidence validation

**Files:**
- Verify externally: AgentLoop application `security-event` Scheduled SQL.
- Verify externally: AgentSight system `ebpf-event` Logstore.
- Verify externally: AgentLoop `incident-event` Logstore.

**Interfaces:**
- Consumes: application `secret_leak` / `pii_exposure` evidence and Task 1 canonical system fields.
- Produces: high-confidence incident records only when both evidence layers match.

- [ ] **Step 1: Create four bounded test fixtures**

Use unique non-secret fixture identifiers:

```text
match-session / match-tool      -> both layers match
mismatch-session / match-tool   -> session mismatch
match-session / mismatch-tool   -> tool-call mismatch
system-only / system-only-tool  -> no application evidence
```

- [ ] **Step 2: Apply the strict query predicates**

The query must require non-empty equality for `agent.id`,
`gen_ai.session.id`, and tool-call ID when applicable; require the AgentSight
`policy_decision` source/sink chain; and constrain the system sink to
`[application_time, application_time + 120 seconds]`.

- [ ] **Step 3: Preview before saving**

Expected preview: only `match-session / match-tool` produces an incident.
The three negative fixtures produce zero incident rows.

- [ ] **Step 4: Validate enforcement wording**

Confirm `security.blocked=true` is rendered as prevented exposure and
`security.blocked=false` as observed high-risk outbound action. Neither branch
claims confirmed byte transfer.

### Task 4: Final verification and publication

**Files:**
- Verify: all branch changes.

**Interfaces:**
- Consumes: Tasks 1-3.
- Produces: one pushed stacked branch and an independent pull request targeting PR #2670's branch until that PR merges.

- [ ] **Step 1: Run static verification**

```bash
git diff --check
make check-license
```

Run clang-format dry-run for the modified C++ files using the repository CI
version.

- [ ] **Step 2: Run security scans**

```bash
bash skills/security-check/scripts/security_check.sh commit
bash skills/security-check/scripts/security_check.sh push
```

Expected: `staging area is clear` and `all commits are clear`.

- [ ] **Step 3: Inspect scope**

```bash
git status --short
git diff --stat fork/codex/agentsight-unified-events...HEAD
git diff fork/codex/agentsight-unified-events...HEAD
```

Expected: only correlation mapping, its behavior test, documentation, and the
approved design/plan documents differ from PR #2670.

- [ ] **Step 4: Push and open the stacked PR**

Push `codex/agentsight-security-correlation` to the fork and open a PR against
`viron-xiao:codex/agentsight-unified-events`. State that the base will be
changed to `main` after PR #2670 merges.

- [ ] **Step 5: Monitor CI**

Verify all required GitHub Actions workflows complete successfully. Diagnose
any failure from its logs before changing code.
