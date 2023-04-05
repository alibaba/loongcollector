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

- [public] [both] [updated] Enable enable_env_ref_in_config configuration to support system variable binding
- [public] [both] [fixed] When using the TagFieldsRename configuration in flusher_kafka_v2/flusher_pulsar, some fields in tags cannot be renamed
- [public] [both] [added] add new plugin type: extension
- [public] [both] [updated] http flusher support custom authenticator, filter and request circuit-breaker via the extension plugin mechanism
- [public] [both] [added] add new plugin: flusher_loki
- [public] [both] [added] add new plugin: processor_string_replace
