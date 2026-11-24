# How to do manual testing

Run Go plugins with the LoongCollector executable and place the test configuration in
`conf/continuous_pipeline_config/local/`. For input plugins with external dependencies,
the [E2E framework](../../cn/developer-guide/test/e2e-test.md) can prepare the test environment.

## Test on Linux

Build the Go plugin shared library with `make plugin_local`. Stop the LoongCollector instance,
replace its `libGoPluginBase.so` with the copy in `output/`, then restart the instance.

## Test in a container

Run `make dist` and `make docker` to build an `aliyun/loongcollector:0.0.1` image from the
current sources. Run it with your test pipeline configuration or deploy it to your test environment.
