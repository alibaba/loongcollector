# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), but make some changes.

## Release tags

- `public` for changes that will be published in release notes.
- `inner` for other changes.

## Platform tags

This project will generate binaries for at least two platforms (Linux/Windows.noarch), so please add platform tag for
your changes, such as:

- `both` for all platforms.
- `win` for Windows.
- `linux` for Linux on amd64 or arm64 arch.
- `linux.arm` for Linux on arm64 arch.
- ....

## Type of changes

- `added` for new features.
- `updated` for changes in existing functionality.
- `deprecated` for soon-to-be removed features.
- `removed` for now removed features.
- `fixed` for any bug fixes.
- `security` in case of vulnerabilities.
- `doc` for doc changes.
- `test` for tests.

## Example

- [public] [both] [updated] add a new feature

## [Unreleased]
- [public] [both] [fixed] fix prometheus send wrong sls logs when reuse string memory
- [public] [both] [fixed] fix dropping jvm metrics when collecting multiple skywalking instances's data
- [public] [both] [fixed] fix elasticsearch flusher authentication tls config and http config
- [public] [both] [fixed] fix profiling wrong type when the different profiling type having same stack. 
- [public] [both] [added] add UsingOldContentTag. When UsingOldContentTag is set to false, the Tag is now placed in the Meta instead of Logs during cgo.
- [public] [both] [fixed] fix send local buffer failed when upgrade iLogtail from version earlier than 1.3.
- [public] [both] [updated] Updated strptime_ns to parse %c format from "%x %X" to "%a %b %d %H:%M:%S %Y" for consistent behavior with striptime.
- [public] [both] [fixed] fix topic key does not support underscore.
- [public] [both] [fixed] fix jmxfetch status error when exist multi jmxfetch config in the same machine.
- [public] [both] [fixed] fix increasing WSS memory issue in collected containers.
- [public] [both] [fixed] fix cannot log blacklist config error
