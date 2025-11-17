# BytesBoundedProcessQueue Unit Test

## Overview
This unit test file provides comprehensive test coverage for the `BytesBoundedProcessQueue` class, focusing on byte-based queue capacity management.

## Test Cases

### 1. TestPush
- Tests basic push operations based on byte size
- Verifies queue behavior when reaching high watermark
- Tests that push operations are forbidden when queue exceeds high watermark

### 2. TestPop
- Tests pop operations from empty and non-empty queues
- Verifies behavior when pop is disabled
- Tests downstream queue validation
- Verifies feedback mechanism when crossing low watermark

### 3. TestSetUpStreamFeedbacks
- Tests setting upstream feedback interfaces
- Verifies nullptr handling in feedback list
- Tests feedback triggering when watermarks are crossed

### 4. TestMetric
- Verifies metric labels are correctly set
- Tests metric counters (in/out items, data size)
- Validates gauge metrics (queue size, valid to push flag)

### 5. TestBytesWatermark
- Tests exact watermark boundary conditions
- Verifies behavior at low and high watermarks
- Tests transitions between valid and invalid push states

### 6. TestEmptyQueue
- Tests operations on empty queue
- Verifies initial state of queue

### 7. TestSingleItemExceedsMaxBytes ⭐
**Edge case**: Single item size exceeds maximum queue capacity
- Tests that item can be pushed if `IsValidToPush()` is true
- Verifies queue becomes invalid after exceeding capacity
- Tests rejection of subsequent items

### 8. TestSingleItemExceedsHighWatermark ⭐
**Edge case**: Single item size exceeds high watermark but not max capacity
- Tests push operation for large single items
- Verifies watermark state transitions

### 9. TestExactWatermarkBoundaries ⭐
**Edge case**: Tests exact boundary values
- Push exactly to low watermark
- Push exactly to high watermark
- Push item with size equal to max capacity

### 10. TestQueueFullScenario
- Tests complete workflow of filling queue to capacity
- Verifies behavior when attempting to push to full queue
- Tests recovery by popping items below low watermark

### 11. TestMultiplePushPopCycles
- Tests multiple cycles of push and pop operations
- Verifies queue state consistency across cycles
- Tests that queue properly resets between cycles

### 12. TestZeroSizeItem ⭐
**Edge case**: Tests items with zero or minimal size
- Push items with empty content
- Tests multiple small items
- Verifies queue handles minimal-size items correctly

### 13. TestExactlyOnceEnabled
- Tests BytesBoundedProcessQueue with ExactlyOnce mode
- Verifies metric label for exactly-once is set
- Tests basic operations with exactly-once semantics

## Edge Cases Covered

✅ **Single item exceeds max capacity** - Tests system behavior when a single item is larger than the queue's maximum capacity

✅ **Single item exceeds high watermark** - Tests when a single item crosses the high watermark threshold

✅ **Exact boundary values** - Tests behavior at exact watermark boundaries (low, high, and max)

✅ **Zero/minimal size items** - Tests queue with items that have no or minimal content

✅ **Queue full scenarios** - Tests complete workflow of filling and draining the queue

✅ **Multiple push/pop cycles** - Tests queue consistency over multiple cycles

✅ **Feedback mechanism** - Tests upstream feedback when crossing watermarks

✅ **Metric tracking** - Tests all metric counters and gauges

✅ **Exactly-once mode** - Tests queue behavior with exactly-once semantics enabled

## Configuration

Test uses the following queue configuration:
- **Max Bytes**: 10,000 bytes
- **Low Watermark**: 2,000 bytes
- **High Watermark**: 8,000 bytes

## Build Instructions

The test is included in the CMake build system:

```bash
cd build
cmake .. -DBUILD_LOGTAIL_UT=ON
make bytes_bounded_process_queue_unittest
```

## Run Instructions

```bash
./bytes_bounded_process_queue_unittest
```

Or run with ctest:
```bash
ctest -R bytes_bounded_process_queue_unittest -V
```

## Dependencies

- Google Test framework
- BytesBoundedProcessQueue implementation
- SenderQueue (for downstream queue testing)
- FeedbackInterfaceMock (for upstream feedback testing)
- PipelineEventGroup (for creating test items)

