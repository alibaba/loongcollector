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

- [public] [both] [updated] Decoder support Opentelemetry metric to SLS Log Protocol, service_otlp supports v1 pipeline, service_http_server v1 pipeline supports otlp metric
- [public] [both] [fixed] Resolved issue of double counting disk total metrics in the disk partition condition of metric_system_v2
- [public] [both] [fixed] do not read env config from exited containers
- [public] [both] [updated] Optimize flusher pulsar to improve performance in static topic scenarios
- [public] [both] [updated] upgrade sarama version to 1.38.1, Rewrite service_kafka input plugin
- [public] [both] [updated] Optimize flusher kafka v2 to improve performance in static topic scenarios
- [public] [both] [fix] json converter marshal without HTML escaped

