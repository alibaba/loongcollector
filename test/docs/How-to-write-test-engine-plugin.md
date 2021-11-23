# How to write Test Engine plugin?

## Write plugins

Currently, Test Engine provies 3 plugins for custom extending, which are Subscriber, LogValidator, and SysValidator.

- Subscrer:  It is the mock backend for Logtailplugin, which ia mapping to the LogtailPlugin flusher.
    -  [grpc subscriber](../engine/subscriber/grpc.go)
- LogValidator: It is used to check each log group, and returns the failure reports.
    -  [log_fields](../engine/validator/log_fields.go)
- SysValidator: It is used to verify the total status of_ the system from the testing start to the end, such as compare
  logs count.
  -  [sys_counter](../engine/validator/sys_counter.go)
  -  [sys_docker_profile](../engine/validator/sys_docker_profile.go)

## Auto generate plugin docs
Please read the [doc](./How-to-generate-plugin-doc.md).