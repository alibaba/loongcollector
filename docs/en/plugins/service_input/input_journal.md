#JournalServer - High - Performance Systemd Journal Collector

![JournalServer](https://img.shields.io/badge/Component-JournalServer-blue) ![Platform](https://img.shields.io/badge/Platform-Linux-green) ![Language](https://img.shields.io/badge/Language-C%2B%2B-red)

JournalServer is a high-performance systemd journal log collection component designed for efficient and reliable journal data extraction. It provides comprehensive filtering capabilities and maintains compatibility with the Golang implementation.

---

## ✨ Key Features

- **🚀 High Performance**: Optimized C++ implementation for maximum throughput
- **🔧 Flexible Filtering**: Support for units, identifiers, kernel logs, and custom patterns
- **⚡ Real-time Collection**: Live journal streaming with checkpoint recovery
- **🛡️ Production Ready**: Battle-tested with comprehensive error handling
- **🎯 OR Logic**: All filters use OR logic for maximum flexibility
- **📊 Rich Metadata**: Collects comprehensive journal entry metadata


### Core Components

| Component | Purpose | Description |
|-----------|---------|-------------|
| **JournalServer** | Main coordinator | Manages journal connections and data flow |
| **Connection Manager** | Connection handling | Manages journal reader connections and lifecycle |
| **Journal Filter** | Data filtering | Applies configured filters with OR logic |
| **Journal Reader** | Data extraction | Interfaces with systemd journal API |
| **Checkpoint Manager** | State persistence | Manages reading position and recovery |

## 🎯 Filter System

JournalServer provides a sophisticated filtering system that allows precise control over which journal entries are collected. **All filters use OR logic**, meaning entries matching any configured filter will be collected.

### Filter Types

#### 1. Units Filter
Collects logs from specific systemd units (services, timers, etc.).

**Configuration:**
```json
{
    "units" : [ "nginx.service", "mysql.service", "redis.service" ]
}
```

**Matching Logic:**
- Service messages: `_SYSTEMD_UNIT=nginx.service`
- Core dumps: `MESSAGE_ID=<coredump_id> + COREDUMP_UNIT=nginx.service`
- PID1 messages: `_PID=1 + UNIT=nginx.service`
- Daemon messages: `_UID=0 + OBJECT_SYSTEMD_UNIT=nginx.service`
- Slice messages: `_SYSTEMD_SLICE=nginx.service`

#### 2. Identifiers Filter
Collects logs from specific syslog identifiers.

**Configuration:**
```json
{
    "identifiers" : [ "sshd", "systemd", "kernel" ]
}
```

**Matching Logic:**
- `SYSLOG_IDENTIFIER=sshd`
- `SYSLOG_IDENTIFIER=systemd`
- `SYSLOG_IDENTIFIER=kernel`

#### 3. Kernel Filter ⚠️
Collects kernel logs (dmesg equivalent).

**Important:** Kernel filter only activates when **both conditions** are met:
- `units` is configured (not empty)
- `kernel` is set to `true`

**Configuration:**
```json
{
    "units" : ["nginx.service"], "kernel" : true
}
```

**Matching Logic:**
- `_TRANSPORT=kernel`

**Why this condition?** 
This prevents accidental collection of high-volume kernel logs when no specific collection target is defined.

#### 4. Match Patterns Filter
Supports custom journal field matching patterns.

**Configuration:**
```json
{
    "matchPatterns" : [ "_SYSTEMD_USER_UNIT=myapp.service", "PRIORITY=3", "_COMM=nginx" ]
}
```

### Filter Logic Relationship

All filters are combined using **OR logic**:

```
(Unit1 OR Unit2 OR Unit3)
OR
(Identifier1 OR Identifier2)  
OR
(Kernel Transport)
OR
(Pattern1 OR Pattern2)
```

### Configuration Examples

#### Example 1: Web Server + Database Monitoring
```json
{
    "units" : [ "nginx.service", "mysql.service" ], "kernel" : true, "identifiers" : ["sshd"]
}
```
**Result:** Collects nginx, mysql, kernel logs, and SSH daemon logs.

#### Example 2: System Service Monitoring  
```json
{
    "units" : [ "systemd-networkd.service", "systemd-resolved.service" ], "kernel" : false, "matchPatterns" : ["_UID=0"]
}
```
**Result:** Collects networkd, resolved services, and all root user processes.

#### Example 3: Kernel-Only Collection
```json
{
    "matchPatterns" : ["_TRANSPORT=kernel"]
}
```
**Result:** Collects only kernel logs (bypasses the units+kernel requirement).

#### Example 4: High-Priority Alerts
```json
{
    "matchPatterns" : [ "PRIORITY=0", "PRIORITY=1", "PRIORITY=2" ]
}
```
**Result:** Collects emergency, alert, and critical priority messages only.

## ⚙️ Configuration Reference

### Complete Configuration Schema

```json
{
    "Type" : "input_journal",
             "JournalPaths" : ["/var/log/journal"],
                              "SeekPosition" : "tail",
                                               "CursorSeekFallback" : "head",
                                                                      "ResetIntervalSecond" : 3600,

                                                                      "Units" : [ "nginx.service", "mysql.service" ],
                                                                                "Kernel" : true,
                                                                                           "Identifiers"
        : [ "sshd", "systemd" ],
          "MatchPatterns" : ["_UID=0"],

                            "ParseSyslogFacility" : true,
                                                    "ParsePriority" : true,
                                                                      "UseJournalEventTime" : true
}
```

### Configuration Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `JournalPaths` | Array | `[]` | Journal file paths (empty = system journal) |
| `SeekPosition` | String | `"tail"` | Initial read position: `head`, `tail`, `cursor` |
| `CursorSeekFallback` | String | `"head"` | Fallback position when cursor invalid (options: `head` or `tail`) |
| `ResetIntervalSecond` | Integer | `3600` | Checkpoint reset interval |
| `Units` | Array | `[]` | Systemd units to monitor |
| `Kernel` | Boolean | `false` | Enable kernel log collection |
| `Identifiers` | Array | `[]` | Syslog identifiers to monitor |
| `MatchPatterns` | Array | `[]` | Custom matching patterns |
| `ParseSyslogFacility` | Boolean | `false` | Convert facility numbers to names |
| `ParsePriority` | Boolean | `false` | Convert priority numbers to names |
| `UseJournalEventTime` | Boolean | `false` | Use journal timestamp vs current time |

## 🚀 Usage Examples

### Basic Service Monitoring
Monitor specific services and collect kernel logs:

```json
{
    "Type" : "input_journal", "Units" : [ "nginx.service", "mysql.service" ], "Kernel" : true, "SeekPosition" : "tail"
}
```

### System-Wide Monitoring
Collect logs from multiple sources:

```json
{
    "Type" : "input_journal",
             "Units" : ["systemd-networkd.service"],
                       "Identifiers" : [ "kernel", "systemd", "NetworkManager" ],
                                       "Kernel" : true,
                                                  "MatchPatterns" : ["_UID=0"],
                                                                    "ParsePriority" : true
}
```

### Emergency Alerts Only
High-priority messages across the system:

```json
{
    "Type" : "input_journal",
             "MatchPatterns" : [ "PRIORITY=0", "PRIORITY=1", "PRIORITY=2" ],
                               "ParsePriority" : true,
                                                 "UseJournalEventTime" : true
}
```

## 🔧 Building and Development

### Prerequisites
- Linux with systemd
- CMake 3.16+
- GCC 9+ or Clang 10+
- libsystemd-dev

### Build Commands
```bash
#From project root
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make journal_server
```


## 🐛 Troubleshooting

### Common Issues

#### Issue: Kernel logs not collected despite `kernel: true`
**Cause:** Kernel filter requires `units` to be configured.

**Solution:**
```json
{
    "units" : ["some.service"], // Add at least one unit
              "kernel" : true
}
```
Or use match patterns:
```json
{
    "matchPatterns" : ["_TRANSPORT=kernel"]
}
```

#### Issue: No logs collected with multiple filters
**Cause:** Expecting AND logic instead of OR logic.

**Solution:** Remember filters use OR logic. If you need AND logic, use specific match patterns:
```json
{
    "matchPatterns" : ["_SYSTEMD_UNIT=nginx.service + PRIORITY=3"]
}
```

#### Issue: High CPU usage
**Cause:** Too broad filters collecting excessive data.

**Solution:** Use more specific filters:
```json
{
    "units" : ["specific.service"], "matchPatterns" : [ "PRIORITY=0", "PRIORITY=1", "PRIORITY=2" ]
}
```

#### Issue: Missing log entries
**Cause:** 
1. Incorrect unit names
2. Journal not readable
3. Checkpoint position issues

**Solution:**
1. Verify unit names: `systemctl list-units`
2. Check permissions: `journalctl --verify`
3. Reset position: `"SeekPosition": "head"`

### Debug Tips

1. **Enable debug logging** in logtail configuration
2. **Check journal integrity**: `journalctl --verify`
3. **Test filters manually**: `journalctl -u nginx.service`
4. **Monitor checkpoint files** for position tracking
5. **Use `journalctl --list-fields`** to discover available fields


---

**📚 Related Documentation:**
- [LoongCollector Main README](../../../README.md)
- [Plugin Development Guide](../../../docs/en/guides/README.md)
- [Performance Benchmarks](../../../docs/en/concept&designs/README.md)
