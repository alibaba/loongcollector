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

- [public] [both] [added] Add support for PostgreSQL input
- [public] [both] [added] Add support for SqlServer input
- [public] [both] [added] Add k8s event when control sls resource
- [public] [both] [updated] Remove chmod and use inherited file permissions on target platform
- [public] [both] [fixed] fix timezone process for microtime in Apsara mode
- [public] [both] [fixed] fix log context lost in plugin system bug
- [public] [both] [fixed] restore "__topic__" field in plugin system
- [public] [both] [added] Add support for Grok processor
- [public] [both] [added] add support for log protocol conversion
