#!/usr/bin/env bash

# Copyright 2021 iLogtail Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


# Currently, there are 3 supported docker categories, which are goc, build, and default.
#
# goc: build goc server with Dockerfile_doc
# build: build core or plugin binary with Dockerfile_build
# default: build ilogtail images.
CATEGORY=$1
GENERATED_HOME=$2
VERSION=${3:-1.1.0}
REPOSITORY=${4:-aliyun/ilogtail}
PUSH=${5:-false}

HOST_OS=`uname -s`
ROOTDIR=$(cd $(dirname "${BASH_SOURCE[0]}") && cd .. && pwd)
GEN_DOCKERFILE=$GENERATED_HOME/Dockerfile

mkdir $GENERATED_HOME
rm -rf $GEN_DOCKERFILE
touch $GEN_DOCKERFILE

if [[ $CATEGORY = "goc" || $CATEGORY = "build" ]]; then
    cat $ROOTDIR/docker/Dockerfile_$CATEGORY|grep -v "#" > $GEN_DOCKERFILE;
elif [[  $CATEGORY = "default" ]]; then
    cat $ROOTDIR/docker/Dockerfile_build |grep -v "#" > $GEN_DOCKERFILE;
    cat $ROOTDIR/docker/Dockerfile_ilogtail_part |grep -v "#">> $GEN_DOCKERFILE;
fi

echo "=============DOCKERFILE=================="
cat $GEN_DOCKERFILE
echo "========================================="
docker build --build-arg VERSION="$VERSION" \
 --build-arg HOST_OS="$HOST_OS" \
  -t "$REPOSITORY":"$VERSION" \
  --no-cache . -f $GEN_DOCKERFILE


if [[ $PUSH = "true" ]]; then
    echo "COMMAND:"
    echo "docker push $REPOSITORY:$VERSION"
    if [[ $VERSION = "latest" ]]; then
      echo "Current operation is so dangerous, you should push by yourself !!!"
    else
      docker push "$REPOSITORY:$VERSION"
    fi
fi
