# How to build docker images?
Currently, the repository only contains the GO part of the iLogtail, and the CPP part would open source soon in 2022. But we also provide many ways to build whole iLogtail images, Go part images, dynamic link library, and etc.


## Build Dynamic library for CPP part.
If you want to build a dynamic library to work together with the CPP binary, please try the following command.
```shell
make solib
```

## Build Whole iLogtail image.
*NOTICE: the way only update the libPluginBase.so [in an existing images](../../../docker/Dockerfile_whole). If the CPP part has also been updated, please try other ways.*

### Build image. 
    - The default {DOCKER_REPOSITORY} is `aliyun/ilogtail`.
    - The default {VERSION} is `1.1.0`.
    - The default {DOCKER_PUSH} is `false`. When the option is configured as true, the built images would also be pushed to the {DOCKER_REPOSITORY} with {VERSION} tag.

    ```shell
        DOCKER_PUSH={DOCKER_PUSH} DOCKER_REPOSITORY={DOCKER_REPOSITORY} VERSION={VERSION} make wholedocker
    ```
   So when you exec `make wholedocker` command, the built image named as `aliyun/ilogtail:1.1.0` would be stored in local repository.


## Build Pure Go image.
If the features that you want to use only in Go part, such as collecting stdout logs or metrics inputs, you cloud build a pure Go image to use.

### Build image.
    - The default {DOCKER_REPOSITORY} is `aliyun/ilogtail`.
    - The default {VERSION} is `1.1.0`.
    - The default {DOCKER_PUSH} is `false`. When the option is configured as true, the built images would also be pushed to the {DOCKER_REPOSITORY} with {VERSION} tag.

    ```shell
        DOCKER_PUSH={DOCKER_PUSH} DOCKER_REPOSITORY={DOCKER_REPOSITORY} VERSION={VERSION} make docker
    ```
So when you exec `make docker` command, the built image named as `aliyun/ilogtail:1.1.0` would be stored in local repository.