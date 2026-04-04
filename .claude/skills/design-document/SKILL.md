---
name: design-document
description: Design document writing conventions. Use when writing or reviewing technical design documents.
---
# Design Document Conventions

## 1. Background / Problem Statement

### 1.1 Background and Pain Points
- Describe current system/module limitations and deficiencies
- List specific scenarios, metrics, or incident cases that triggered this design

### 1.2 Impact Scope
- Affected modules, microservices, APIs, data stores, third-party dependencies
- Potential impact on performance, reliability, cost, maintainability
- Forward/backward compatibility analysis

### 1.3 Constraints
- Compliance/security/performance/resource restrictions
- External system or infrastructure dependencies

---

## 2. Design Goals

### 2.1 Functional Goals
- List Must/Should/Could core capabilities by priority

### 2.2 Non-Functional Goals
- Performance (throughput, latency, concurrency, resource usage)
- Scalability, maintainability, testability, observability
- Reliability (fault tolerance, HA, degradation, rollback strategies)

### 2.3 Constraint Goals
- Backward compatibility, API stability
- Security and compliance requirements

---

## 3. Technical Design

### 3.1 Architecture Diagram
- Use Mermaid for high-level component diagrams with data/control flow

### 3.2 Detailed Flowcharts
- Key business flows, exception flows, retry/compensation with timing and triggers

### 3.3 Thread/Concurrency Model
- Thread lifecycle, inter-thread communication (locks, condition variables, queues, Actor patterns)
- Sequence diagrams for concurrency interactions

### 3.4 Core Classes and Data Structures
- Class diagrams showing main classes, interfaces, inheritance/composition relationships
- Key data structure fields, lifecycle, thread-safety strategy

### 3.5 Key Algorithms or Protocols
- Pseudocode or flow for pub/sub, load balancing, retry backoff, etc.
- State machine / protocol state transition diagrams

### 3.6 Error Handling and Recovery
- Error classification, exception stack, retry strategies, degradation plans
- Monitoring metrics, alert trigger conditions and levels

### 3.7 Deployment and Operations
- Configuration items, hot-update mechanisms, canary and rollback strategies
- CI/CD, container, Service Mesh, Kubernetes resource considerations

---

## 4. Unit Testing

### 4.1 Test Scope and Goals
- Cover core logic, boundary conditions, concurrency scenarios, exception paths

### 4.2 Test Environment and Tools
- Google Test/Mock version, necessary third-party stubs/fakes

### 4.3 Test Scenarios and Cases
| Case ID | Scenario | Input | Expected Output/Behavior | Mock Dependencies |
|---------|----------|-------|--------------------------|-------------------|
| TC-01   | Normal single log push | Single valid LogRecord | Returns SUCCESS, buffer size +1 | None |
| TC-02   | Buffer full | capacity=N filled | Throws BufferOverflowException | None |
| TC-03   | Concurrent push | Multi-thread simultaneous push | No data loss, order/final consistency matches design | MutexMock |
| TC-04   | flush clears | M items exist, then flush | Returns M items, buffer size=0 | TimeProviderMock |

### 4.4 Boundary and Exception Testing
- Empty input, invalid input, extreme capacity, network/disk fault injection

### 4.5 Performance Benchmarking (optional)
- Throughput, latency, CPU/Memory profile; comparison with baseline

---

## Notes

- **Do not** include project management info (estimates, schedules, milestones, Gantt charts)
- Code examples must follow team C++ coding standards (see `.claude/skills/project-knowledge/`)
- Test case naming: `<Module>_<Function>_<Number>` for CI coverage tracking
