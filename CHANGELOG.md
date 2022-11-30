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

## Example

- [public] [both] [updated] add a new feature

## [Unreleased]

- [public] [both] [added] service\_http\_server support OTLP log input.
- [public] [both] [added] add a new flusher\_otlp\_log plugin.
- [public] [both] [added] add a new flusher\_kafka\_v2 plugin.
- [public] [linux] [fixed] strip binaries to reduce dist size.
- [public] [both] [fixed] fix docker fd leak and event lost by using official docker Go library
- [public] [linux] [updated] polish ebpf data structure
- [public] [both] [added] add processor\_desensitize processor

