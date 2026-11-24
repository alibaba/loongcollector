# How to build Docker images and the Go plugin library

## Build the Go plugin shared library

Run `make plugin` to build the Linux shared library in Docker. The build copies
`libGoPluginBase.so` to `output/`. To build the library locally, run `make plugin_local`.

## Build the LoongCollector image

Run `make dist` to build the release archive and `make docker` to package it into an image.

```shell
make dist
make docker
```

The defaults are `DOCKER_REPOSITORY=aliyun/loongcollector` and `VERSION=0.0.1`. Use the same
version when building the archive and image. To push the image as part of the build, set
`DOCKER_PUSH=true` when running `make docker`.
