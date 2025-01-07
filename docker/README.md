# Dockerfile

## Dockerfiles to build iLogtail

Dockerfile_build: build iLogtail
Dockerfile_goc: goc server
Dockerfile_development_part: testing entrypoint
Dockerfile_production: production entrypoint

Dockerfile_development_part works with the e2e engine. For the usage, we add a ENV named HOST_OS to distinguish host system because the `host.docker.internal` host cannot be parsed in the docker engine of Linux, more details please see [here](https://github.com/docker/for-linux/issues/264).
