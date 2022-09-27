#!/usr/bin/env bash

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

set -ue
set -o pipefail

usage() {
    echo "Upload current package/image to OSS/Repository"
    echo 'Before run this script make sure you have `make dist` or `make docker`'
    echo ''
    echo "Usage: $0 artifact_type local_version [remote_version]"
    echo ''
    echo 'artifact_type   package|image'
    echo 'local_version   example: 1.1.0, edge'
    echo 'remote_version  same as local_version if omited. example: 1.1.0, edge, latest'
    exit 1
}

[[ $# -ne 2 && $# -ne 3 ]] && {
    usage
} || :

SCRIPT_DIR=$(dirname ${BASH_SOURCE[0]})
VERSION=$2
SPECIAL_TAG=
[[ $# -eq 3 ]] && {
    SPECIAL_TAG=$3
}

upload_package() {
    # upgrade version to latest|edge
    [[ ! -z $SPECIAL_TAG ]] && {
        ossutil64 cp oss://ilogtail-community-edition/$VERSION/ilogtail-$VERSION.linux-amd64.tar.gz \
            oss://ilogtail-community-edition/${SPECIAL_TAG}/ilogtail-${SPECIAL_TAG}.linux-amd64.tar.gz
        ossutil64 cp oss://ilogtail-community-edition/$VERSION/ilogtail-$VERSION.linux-amd64.tar.gz.sha256 \
            oss://ilogtail-community-edition/${SPECIAL_TAG}/ilogtail-${SPECIAL_TAG}.linux-amd64.tar.gz.sha256
        exit
    } || :

    sha256sum ilogtail-$VERSION.tar.gz > ilogtail-$VERSION.tar.gz.sha256
    ossutil64 cp ilogtail-$VERSION.tar.gz oss://ilogtail-community-edition/$VERSION/ilogtail-$VERSION.linux-amd64.tar.gz
    ossutil64 cp ilogtail-$VERSION.tar.gz.sha256 oss://ilogtail-community-edition/$VERSION/ilogtail-$VERSION.linux-amd64.tar.gz.sha256
}

upload_image() {
    # upgrade version to latest|edge
    [[ ! -z $SPECIAL_TAG ]] && {
        docker tag sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail:$VERSION \
            sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail:${SPECIAL_TAG}
        [[ $SPECIAL_TAG == "edge" ]] && $SCRIPT_DIR/rm_ilogtail_edge_image || :
        docker push sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail:${SPECIAL_TAG}
        exit
    } || :

    docker tag aliyun/ilogtail:$VERSION sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail:$VERSION
    [[ $VERSION == "edge" ]] && $SCRIPT_DIR/rm_ilogtail_edge_image || :
    docker push sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail:$VERSION
}

if [[ $1 == "package" ]]; then
    upload_package
elif [[ $1 == "image" ]]; then
    upload_image
else
    usage
fi
