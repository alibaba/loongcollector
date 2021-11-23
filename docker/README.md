# Dockerfile

The Logtailplugin would be built with the c-shared mode in production to work with Logtail. So the Logtailplugin image
would not be pushed to the Dockerhub. There are only 2 usages of it.

1. Provide an easy way to run and learn Logtailplugin.
2. Work with the e2e engine. For the usage, we add a ENV named HOST_OS to distinguish host system because
   the `host.docker.internal` host cannot be parsed in the docker engine of Linux, more details please
   see [here](https://github.com/docker/for-linux/issues/264).
