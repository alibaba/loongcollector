# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), but make some changes.

## Release tags
- `public` for changes that will be published in release notes.
- `inner` for other changes.


## Platform tags
This project will generate binaries for at least two platforms (Linux/Windows.noarch), so please add platform tag for your changes, such as:

- `both` for all platforms.
- `win` for Windows.
- `linux` for Linux.
- `linux.arm` for Linux on ARM.
- ....


## Type of changes
- `added` for new features.
- `updated` for changes in existing functionality.
- `deprecated` for soon-to-be removed features.
- `removed` for now removed features.
- `fixed` for any bug fixes.
- `security` in case of vulnerabilities.


## [Unreleased]
- `public` `both` `updated` optimize system_v2 IOCounter metrics
- `public` `both` `updated` change kafka flusher partition strategy to hash key
- [inner][both][fixed] `service_http_server`: unlink unix sock before listen.

