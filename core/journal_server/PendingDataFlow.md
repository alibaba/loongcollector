# PendingData Processing Flow with Epoll Events

## Overview
This diagram illustrates how pendingData is handled in different scenarios, combined with epoll event processing.

## Three PendingData Scenarios

### Scenario 1: pushFailed
- **When**: `PushEventGroupToQueue` fails (queue full or error)
- **Result**: 
  - `hasPendingData = true`
  - `accumulatedEventGroup` retained
  - Cursor seeked back to `firstEntryCursor`
- **Next**: Retried in next epoll cycle (with or without events)

### Scenario 2: accumulated continue
- **When**: New entries read but `totalEntryCount < maxEntriesPerBatch` and no timeout
- **Result**:
  - `hasPendingData = true`
  - `accumulatedEventGroup` contains merged data (old + new)
- **Next**: Continue accumulating in next epoll cycle

### Scenario 3: noNewData but processQueue Not valid
- **When**: `noNewData == true` but `IsValidToPush(queueKey) == false`
- **Result**:
  - `hasPendingData = true`
  - `accumulatedEventGroup` retained
- **Next**: Retried when queue becomes valid

```mermaid
flowchart TD
    Start([JournalServer::run Loop]) --> EpollWait[epoll_wait]
    EpollWait --> CheckNfds{nfds == 0?}
    
    %% No Events Path
    CheckNfds -->|Yes: No Events| ProcessPendingNoEvents[processPendingDataWhenNoEvents]
    ProcessPendingNoEvents --> Continue1[continue to next loop]
    Continue1 --> EpollWait
    
    %% Has Events Path
    CheckNfds -->|No: Has Events| BuildActiveFDs[Build activeFDs set from events]
    BuildActiveFDs --> LoopReaders[Loop through monitoredReaders]
    
    %% Reader Selection Logic
    LoopReaders --> CheckReader{Reader has event<br/>OR hasPendingData?}
    CheckReader -->|No| SkipReader[Skip this reader]
    SkipReader --> NextReader{More readers?}
    
    CheckReader -->|Yes| ValidateReader[GetValidatedCurrentReader]
    ValidateReader --> CheckStatus[CheckJournalStatus]
    
    %% HandleJournalEntries Processing
    CheckStatus --> HandleEntries[HandleJournalEntries]
    HandleEntries --> MergeData[Merge accumulated data<br/>with new entries]
    
    %% Decision Tree in HandleJournalEntries
    MergeData --> CheckTotalCount{totalEntryCount == 0?}
    CheckTotalCount -->|Yes| NoPendingData[hasPendingData = false<br/>return false]
    NoPendingData --> NextReader
    
    CheckTotalCount -->|No| CheckConditions{Check conditions}
    
    %% Condition 1: Timeout or Max Batch
    CheckConditions -->|timeoutTrigger OR<br/>reachedMaxBatch| MustPush[Must Push]
    MustPush --> PushToQueue1[PushEventGroupToQueue]
    PushToQueue1 --> CheckPushResult1{Push Success?}
    CheckPushResult1 -->|Yes| ClearPending1[Clear accumulated data<br/>hasPendingData = false]
    CheckPushResult1 -->|No: pushFailed| KeepPending1[Keep accumulated data<br/>hasPendingData = true<br/>Seek cursor back]
    ClearPending1 --> NextReader
    KeepPending1 --> NextReader
    
    %% Condition 2: No New Data
    CheckConditions -->|noNewData AND<br/>!timeoutTrigger AND<br/>!reachedMaxBatch| CheckQueueValid{IsValidToPush?}
    CheckQueueValid -->|Yes| PushToQueue2[PushEventGroupToQueue]
    PushToQueue2 --> CheckPushResult2{Push Success?}
    CheckPushResult2 -->|Yes| ClearPending2[Clear accumulated data<br/>hasPendingData = false]
    CheckPushResult2 -->|No: pushFailed| KeepPending2[Keep accumulated data<br/>hasPendingData = true<br/>Seek cursor back]
    ClearPending2 --> NextReader
    KeepPending2 --> NextReader
    
    CheckQueueValid -->|No: Queue Not Valid| KeepPending3[Keep accumulated data<br/>hasPendingData = true<br/>Return false]
    KeepPending3 --> NextReader
    
    %% Condition 3: Continue Accumulating
    CheckConditions -->|!noNewData AND<br/>!timeoutTrigger AND<br/>!reachedMaxBatch| ContinueAccum[Continue Accumulating]
    ContinueAccum --> KeepPending4[Keep accumulated data<br/>hasPendingData = true]
    KeepPending4 --> NextReader
    
    %% Loop Control
    NextReader -->|Yes| LoopReaders
    NextReader -->|No| EndLoop[End of loop iteration]
    EndLoop --> EpollWait
    
    %% Styling
    classDef pendingData fill:#ffcccc,stroke:#ff0000,stroke-width:2px
    classDef clearPending fill:#ccffcc,stroke:#00ff00,stroke-width:2px
    classDef decision fill:#ffffcc,stroke:#ffaa00,stroke-width:2px
    
    class KeepPending1,KeepPending2,KeepPending3,KeepPending4 pendingData
    class ClearPending1,ClearPending2,NoPendingData clearPending
    class CheckNfds,CheckReader,CheckTotalCount,CheckConditions,CheckQueueValid,CheckPushResult1,CheckPushResult2,NextReader decision
```

## PendingData States

### State 1: pushFailed
- **Trigger**: `PushEventGroupToQueue` returns false (queue full or error)
- **Action**: 
  - Keep `accumulatedEventGroup` intact
  - Set `hasPendingData = true`
  - Seek cursor back to `firstEntryCursor`
- **Next Processing**: Will be retried in next epoll cycle (with or without events)

### State 2: accumulated continue
- **Trigger**: New entries read but `totalEntryCount < maxEntriesPerBatch` and no timeout
- **Action**:
  - Keep `accumulatedEventGroup` with merged data
  - Set `hasPendingData = true`
  - Continue accumulating in next cycle
- **Next Processing**: Will be processed in next epoll cycle (with or without events)

### State 3: noNewData but processQueue Not valid
- **Trigger**: `noNewData == true` but `IsValidToPush(queueKey) == false`
- **Action**:
  - Keep `accumulatedEventGroup` intact
  - Set `hasPendingData = true`
  - Return false (don't clear accumulated data)
- **Next Processing**: Will be retried in next epoll cycle when queue becomes valid

## Epoll Event Integration

### When nfds == 0 (No Events)
1. Call `processPendingDataWhenNoEvents()` to process all readers with pendingData
2. Skip readers without pendingData

### When nfds > 0 (Has Events)
1. Build `activeFDs` set from epoll events
2. Process readers that:
   - Have events in `activeFDs`, OR
   - Have `hasPendingData == true`
3. Skip readers without events and without pendingData
4. `HandleJournalEntries` merges accumulated data with new entries

## Key Points

1. **PendingData is always processed**: Whether there are epoll events or not, pendingData will be processed
2. **Merging happens in HandleJournalEntries**: When there are events, accumulated data is merged with new entries
3. **Three main pendingData scenarios**:
   - `pushFailed`: Queue push failed, data retained
   - `accumulated continue`: Still accumulating, not yet ready to push
   - `noNewData but queue invalid`: No new data but queue not available
4. **Epoll events determine which readers to process**: Only readers with events or pendingData are processed

