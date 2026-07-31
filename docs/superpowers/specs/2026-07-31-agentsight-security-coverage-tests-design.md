# AgentSight Security Coverage Tests Design

## Context

PR #2670 successfully builds and passes its C++ unit tests, but the CI diff-coverage gate reports 30.9%, below the required 60%. The uncovered additions are concentrated in AgentSight security-event conversion and `SecurityAudit` configuration validation.

## Goal

Raise meaningful diff coverage above the repository threshold by testing externally observable behavior. The tests must validate emitted log contents and configuration outcomes rather than merely execute lines.

## Selected approach

Extend the existing `AgentsightManagerUnittest` and `InputAgentSightUnittest` fixtures.

1. Feed an `AgentsightSecurityRecord` through `AgentsightManager::HandleEvent` after registering a real count-bounded process queue.
2. Pop the resulting `ProcessQueueItem` and assert the generated `LogEvent` fields, including the event envelope, identity fields, flattened security payload, schema version, original JSON, and timestamps.
3. Add a malformed-JSON case that verifies the raw event is still emitted with its stable envelope fields.
4. Add input configuration cases for an invalid `SecurityAudit` value, an empty enforcer socket, and the default socket path.

This keeps production interfaces unchanged and exercises the same queue and event-model path used at runtime.

## Alternatives considered

- Expose JSON conversion helpers directly to tests. This would simplify unit setup but enlarge the production/test API and bypass queue integration.
- Exclude the new lines from coverage or lower the threshold. This would silence CI without validating behavior and is rejected.

## Test boundaries

- Reuse existing fake AgentSight FFI symbols; no live AgentSight library or enforcer is required.
- Create and remove queue state within each test so singleton queue state cannot leak between cases.
- Assert representative scalar types and nested fields without duplicating every JSON key.
- Do not change production behavior unless a test uncovers an actual defect.

## Success criteria

- New behavior tests pass in the targeted C++ unit-test binaries.
- Existing AgentSight manager and input tests remain green.
- CI diff coverage reaches at least 60%.
- Linux build, compatibility, and static checks remain green.
