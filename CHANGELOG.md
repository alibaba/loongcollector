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
- [public] [both] [added] add a new service_otlp plugin.
- [public] [both] [updated] service_http_server supports otlp metrics/traces requests.
- [public] [both] [added] add a new flusher_pulsar plugin.
- [public] [both] [fixed] ignore time zone adjustment in config when using system time as log time
- [public] [both] [updated] flusher kafka v2 support TLS and Kerberos authentication.
- [public] [both] [updated] improve logs related to file discovery
- [public] [both] [fixed] fix collection duplication problem caused by checkpoint overwritten
- [public] [both] [fixed] ignore timezone adjustment when system time is used
- [public] [both] [fixed] ignore timezone adjustment when log parse fails
- [public] [both] [fixed] fix blocking problem caused by alwaysonline config update
- [public] [both] [added] add a new flusher\_clickhouse plugin.
- [public] [both] [updated] grok processor reports unmatched errors by default
- [public] [both] [fixed] grok processor gets stuck with Chinese
- [public] [both] [fixed] fix plugin version in logs
- [public] [both] [updated] flusher http support variable config in request header   