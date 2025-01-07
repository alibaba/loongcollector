#!/bin/sh
# Copyright 2022 iLogtail Authors
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

# called by generate-release-markdown.sh

set -ue
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 1.1.0" >&2
    exit 1
fi

INITPWD=$PWD
ROOTDIR=$(cd $(dirname $0) && cd .. && pwd)
version=$1

cd "${ROOTDIR}"

# Build version
sed -i "s/VERSION ?= .*/VERSION ?= $version/g" Makefile
sed -i "s/set(LOGTAIL_VERSION \".*\")/set(LOGTAIL_VERSION \"$version\")/g" \
    core/options.cmake

# Dockerfile
sed -i "s/ARG VERSION=.*/ARG VERSION=$version/g" docker/Dockerfile*

# Scripts
sed -i "s/VERSION=\${1:-.*}/VERSION=\${1:-$version}/g" scripts/*.sh
sed -i "s/VERSION=\${2:-.*}/VERSION=\${2:-$version}/g" scripts/*.sh
sed -i "s/VERSION=\${3:-.*}/VERSION=\${3:-$version}/g" scripts/*.sh
sed -i "s/VERSION=\${4:-.*}/VERSION=\${4:-$version}/g" scripts/*.sh
sed -i "s/DIST_DIR=\${2:-loongcollector-.*}/DIST_DIR=\${2:-loongcollector-$version}/g" scripts/dist.sh
sed -i "s/^set ILOGTAIL_VERSION=.*/set ILOGTAIL_VERSION=$version/g" scripts/*.bat
sed -i "s/image: aliyun\\/loongcollector:.*/image: aliyun\\/loongcollector:$version/g" test/engine/setup/dockercompose/compose.go

# Docs
sed -i "s/aliyun\\/loongcollector:[^\` ]*/aliyun\\/loongcollector:$version/g" \
    README.md \
    docs/en/guides/How-to-do-manual-test.md \
    docs/en/guides/How-to-build-with-docker.md
sed -i -r "s/VERSION(.*)\`.*\`/VERSION\\1\`$version\`/g" \
    docs/en/guides/How-to-build-with-docker.md

cd "${INITPWD}"
