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

- [public] [both] [updated] Elasticsearch flusher new features: send batch request by bulk api and format index
- [public] [both] [added] Add new plugin: input_command
- [public] [both] [added] Add new plugin: processor_log_to_sls_metric
- [public] [both] [added] Support global hostpath blacklist (match by substring)
- [public] [both] [updated] Service_http_server support raw type on v1
- [public] [both] [updated] Add v2 interface to processor_json
- [public] [both] [updated] Json processor support parsing array
- [public] [both] [updated] Service_prometheus support scale in kubernetes
- [public] [both] [updated] Support to export logtail listening ports
- [public] [both] [updated] Add containerd custom rootpath support
- [public] [both] [updated] Upgrade go version to 1.19
- [public] [both] [updated] Refactory LogBuffer structure
- [inner] [both] [updated] Logtail containers monitor refine code

- [public] [both] [fixed] Fix service_go_profile nil panic
- [public] [both] [fixed] Fix broken container stdout log path link
- [public] [both] [fixed] Fix zstd batch send error
- [public] [both] [fixed] Fix go pb has TimeNs when not set
- [public] [both] [fixed] Fix elasticsearch flusher panic
- [public] [both] [fixed] Fix the abnormal shutdown issue of service_otlp
- [public] [both] [fixed] Fix inconsistence of ttl when param invalid
- [public] [both] [fixed] Fix proxy env key error
- [public] [both] [fixed] Update map time after mark container removed
- [public] [both] [fixed] Fix multi-bytes character cut off
- [public] [both] [fixed] Add timeout func when collect disk metrics
- [public] [both] [fixed] Fix too long log split alarm
- [public] [both] [fixed] Fix updating agent config's check_status in heartbeat in config server
- [inner] [both] [fixed] Fix file name of shennong profile data in container

- [public] [both] [doc] Elasticsearch flusher config examples
- [public] [both] [doc] Fix document of flusher kafka v2
