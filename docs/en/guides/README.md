# iLogtail Developer Guide
Through the following content, you will learn about the overall structure of iLogtail and learn how to contribute plug-ins.

## Plugin contribution guide
- [How to use Logger](How-to-use-logger.md)
- [Develop Input Plugin](How-to-write-input-plugins.md)
- [Develop Processor Plugin](How-to-write-processor-plugins.md)
- [Develop Aggregator plugin](How-to-write-aggregator-plugins.md)
- [Develop Flusher plugin](How-to-write-flusher-plugins.md)

## Test
When the plugin is written, the following content will guide you how to do unit testing and E2E testing. E2E testing can help you mock testing environments, such as Mysql dependencies.
- [How to write a unit test](How-to-write-unit-test.md)
- [How to write E2E test](../../../test/README.md)
- [How to do manual testing](How-to-do-manual-test.md)

## Add plugins doc
- [generate plugin doc](./How-to-genernate-plugin-doc.md)

## Code Check
Before you submit a Pull Request, you need to ensure that the code style check and the test are whole pass. The following content will help you to check them.
- [Check Code Style](How-to-chek-codestyle.md)
- [Check Code License](How-to-check-license.md)
- [Check Code Dependency License](How-to-chek-dependency-license.md), if there is no new dependency package, please ignore this step.
- Use `make test` to execute all unit tests to ensure them pass.
- Use `make e2e` to execute the e2e test to ensure them pass.


## Others
- [How to build image or dynamic library with docker?](How-to-build-with-docker.md)
- [How to release iLogtail?](How-to-release.md)

