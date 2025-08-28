#!/usr/bin/env bash
# Copyright 2025 LoongCollector Authors
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
    echo "Usage: $0 artifact_type version"
    echo ''
    echo 'artifact_type   package|image'
    echo 'version        example: 1.1.0'
    exit 1
}

[[ $# -ne 2 ]] && {
    usage
} || :

SCRIPT_DIR=$(dirname ${BASH_SOURCE[0]})
VERSION=$2
OSSUTIL="ossutil -e oss-cn-shanghai.aliyuncs.com --region cn-shanghai -i $OSS_ACCESS_KEY_ID -k $OSS_ACCESS_KEY_SECRET"

upload_package() {
    # Upload Linux packages
    for arch in amd64 arm64; do
        if [ -f "dist/loongcollector-$VERSION.linux-$arch.tar.gz" ]; then
            echo "Uploading Linux $arch package..."
            $OSSUTIL cp dist/loongcollector-$VERSION.linux-$arch.tar.gz oss://loongcollector-community-edition/$VERSION/loongcollector-$VERSION.linux-$arch.tar.gz
            if [ -f "dist/loongcollector-$VERSION.linux-$arch.tar.gz.sha256" ]; then
                $OSSUTIL cp dist/loongcollector-$VERSION.linux-$arch.tar.gz.sha256 oss://loongcollector-community-edition/$VERSION/loongcollector-$VERSION.linux-$arch.tar.gz.sha256
            fi
        fi
    done
    
    # Upload Windows package
    if [ -f "dist/loongcollector-$VERSION.windows-amd64.zip" ]; then
        echo "Uploading Windows package..."
        $OSSUTIL cp dist/loongcollector-$VERSION.windows-amd64.zip oss://loongcollector-community-edition/$VERSION/loongcollector-$VERSION.windows-amd64.zip
        if [ -f "dist/loongcollector-$VERSION.windows-amd64.zip.sha256" ]; then
            $OSSUTIL cp dist/loongcollector-$VERSION.windows-amd64.zip.sha256 oss://loongcollector-community-edition/$VERSION/loongcollector-$VERSION.windows-amd64.zip.sha256
        fi
    fi
}

upload_image() {
    echo "Building and pushing Docker image for version $VERSION"
    docker buildx build --platform linux/amd64,linux/arm64 \
        --file docker/Dockerfile_release \
        --build-arg VERSION=$VERSION \
        --build-arg HOST_OS=Linux \
        --tag sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/loongcollector-community-edition/loongcollector:$VERSION \
        --push .
    echo "Docker image pushed to sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/loongcollector-community-edition/loongcollector:$VERSION"
}

if [[ $1 == "package" ]]; then
    upload_package
elif [[ $1 == "image" ]]; then
    upload_image
else
    usage
fi

