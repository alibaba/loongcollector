# input_journal

## Overview

The `input_journal` plugin collects systemd journal logs. It supports flexible filtering configurations to collect logs by service units, identifiers, kernel logs, and custom patterns.

## Configuration Parameters

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| Type | String | Yes | - | Plugin type, must be `input_journal` |
| JournalPaths | String Array | No | `[]` | Journal file paths (empty = system default) |
| SeekPosition | String | No | `tail` | Initial read position: `head` (from beginning), `tail` (from end), `cursor` (from saved position) |
| CursorSeekFallback | String | No | `head` | Fallback position when cursor is invalid: `head` or `tail` |
| ResetIntervalSecond | Int | No | `3600` | Checkpoint reset interval (seconds) |
| Units | String Array | No | `[]` | List of systemd service units to monitor |
| Kernel | Boolean | No | `false` | Enable kernel log collection (requires Units to be configured) |
| Identifiers | String Array | No | `[]` | List of syslog identifiers to monitor |
| MatchPatterns | String Array | No | `[]` | Custom match patterns supporting journal field matching |
| ParseSyslogFacility | Boolean | No | `false` | Convert facility numbers to names |
| ParsePriority | Boolean | No | `false` | Convert priority numbers to names |
| UseJournalEventTime | Boolean | No | `false` | Use journal event time (otherwise use collection time) |

## Filter Logic

All filters use **OR logic**, meaning logs matching any filter condition will be collected:

```text
(Any service in Units) OR (Any identifier in Identifiers) OR (Kernel logs) OR (Any pattern in MatchPatterns)
```

### Filter Types

#### 1. Units (Service Unit Filter)

Specifies systemd service units to collect logs from, e.g., `nginx.service`, `mysql.service`

#### 2. Identifiers (Identifier Filter)

Specifies syslog identifiers to collect logs from, e.g., `sshd`, `systemd`

#### 3. Kernel (Kernel Log Filter)

Collects kernel logs (dmesg equivalent).

**Note**: Kernel filtering requires both conditions:

- `Units` is not empty
- `Kernel` is set to `true`

To collect kernel logs only, use `MatchPatterns: ["_TRANSPORT=kernel"]`

#### 4. MatchPatterns (Custom Pattern Filter)

Supports custom journal field matching, examples:

- `_SYSTEMD_USER_UNIT=myapp.service` - Match user services
- `PRIORITY=3` - Match specific priority
- `_COMM=nginx` - Match specific command
- `_UID=0` - Match specific user ID

## Checkpoint Mechanism

### Overview

The plugin supports resumable log collection through a checkpoint mechanism that records the reading position. This ensures that after restart or failure recovery, collection can continue from the last stopped position, preventing log loss or duplicate collection.

### How It Works

1. **Position Recording**: The plugin periodically saves the current journal cursor position
2. **Resume Reading**: After restart, with `SeekPosition: "cursor"`, reading continues from the last saved position
3. **Cursor Invalidation**: If the saved cursor is invalid (e.g., log files deleted or rotated), it falls back to `head` or `tail` based on `CursorSeekFallback` configuration
4. **Periodic Reset**: The `ResetIntervalSecond` parameter controls checkpoint reset interval to avoid performance issues from long-term accumulation

### Related Configuration

```json
{
    "Type": "input_journal",
    "SeekPosition": "cursor",        // Use cursor mode to enable resumable collection
    "CursorSeekFallback": "tail",    // Start from tail when cursor is invalid
    "ResetIntervalSecond": 3600      // Reset checkpoint every hour
}
```

## Configuration Examples

### Example 1: Monitor Web Services

```json
{
    "Type": "input_journal",
    "Units": ["nginx.service", "mysql.service"],
    "Kernel": true,
    "SeekPosition": "tail"
}
```

### Example 2: System-Wide Monitoring

```json
{
    "Type": "input_journal",
    "Units": ["systemd-networkd.service"],
    "Identifiers": ["sshd", "systemd"],
    "Kernel": true,
    "SeekPosition": "cursor",
    "ParsePriority": true
}
```

### Example 3: Kernel Logs Only

```json
{
    "Type": "input_journal",
    "MatchPatterns": ["_TRANSPORT=kernel"]
}
```

### Example 4: High-Priority Alerts

```json
{
    "Type": "input_journal",
    "MatchPatterns": ["PRIORITY=0", "PRIORITY=1", "PRIORITY=2"],
    "ParsePriority": true,
    "UseJournalEventTime": true
}
```

## Troubleshooting

### 1. Kernel logs not collected despite `Kernel: true`

**Cause**: Kernel filtering requires `Units` to be configured

**Solution**:

```json
{
    "Units": ["any.service"],  // Add at least one service unit
    "Kernel": true
}
```

Or use MatchPatterns:

```json
{
    "MatchPatterns": ["_TRANSPORT=kernel"]
}
```

### 2. Incorrect log collection position

**Solutions**:

- Start from beginning: Set `"SeekPosition": "head"`
- Collect only new logs: Set `"SeekPosition": "tail"`
- Resume from last position: Set `"SeekPosition": "cursor"`

### 3. How to verify configuration

Test manually using journalctl commands:

```bash
# Test service unit filter
journalctl -u nginx.service

# Test identifier filter
journalctl -t sshd

# List available fields
journalctl --list-fields
```
