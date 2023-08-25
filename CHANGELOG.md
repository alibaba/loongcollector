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

- [public] [both] [updated] support continue/end regex patterns to split multiline log
- [public] [both] [updated] support reader flush timeout
- [public] [both] [added] add new logtail metric module
- [public] [both] [fixed] fix multiline is splitted if not flushed to disk together
- [public] [both] [fixed] fix line is truncated if \0 is in the middle of line
- [public] [both] [added] refactoried C++ process pipeline
- [public] [both] [added] support use accelerate processors with go processors
