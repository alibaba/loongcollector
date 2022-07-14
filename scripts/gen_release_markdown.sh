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

set -ue
set -o pipefail

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 1.1.0(version) 11(milestone)" >&2
    exit 1
fi

INITPWD=$PWD
ROOTDIR=$(cd $(dirname $0) && cd .. && pwd)


function createReleaseFile () {
    local version=$1
    local mileston=$2
    if [ ! -d "${ROOTDIR}/changes" ]; then
        mkdir ${ROOTDIR}/changes
    fi
    local doc=${ROOTDIR}/changes/v${version}.md
    if [ -f "${doc}" ]; then
        rm -rf ${doc}
    fi
    touch $doc
    echo "# ${version}" >> $doc
    echo "## Changes" >> $doc
    echo  "All issues and pull requests are [here](https://github.com/alibaba/ilogtail/milestone/${mileston})." >> $doc
    appendUnreleaseChanges $doc

    echo "## Download" >> $doc
    appendDownloadLinks $doc $version
    echo "## Docker Image" >> $doc
    appendDockerImageLinks $doc $version
}


function appendUnreleaseChanges () {
    local doc=$1
    local changeDoc=$ROOTDIR/CHANGELOG.md
    local tempFile=$(mktemp temp.XXXXXX)
    cat $changeDoc|grep "Unreleased" -A 500 |grep -v "Unreleased"|grep -v '^$' >> $tempFile
    echo "### Features" >> $doc
    cat $tempFile|grep -E '\[added\]|\[updated\]|\[deprecated\]|\[removed\]' >> $doc
    echo "### Fixed" >> $doc
    cat $tempFile|grep -E '\[fixed\]|\[security\]' >> $doc
    echo "### Doc" >> $doc
    cat $tempFile|grep -E '\[doc\]' >> $doc
    rm -rf $tempFile
}

function appendDownloadLinks () {
    local doc=$1
    local version=$2
    local linux_amd_url="https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/${version}/ilogtail-${version}.linux-amd64.tar.gz"
    local linux_amd_sig=$(wget -nv -O- https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/${version}/ilogtail-${version}.linux-amd64.tar.gz.sha256 | cut -d' ' -f1)
cat <<- EOF
| **Filename** | **OS** | **Arch** | **SHA256 Checksum** |
|  ----  | ----  | ----  | ----  |
|[ilogtail-1.1.0.linux-amd64.tar.gz]($linux_amd64_url)|Linux|x86-64|${linux_amd_sig}|
EOF >> $doc
}

function appendDockerImageLinks () {
    local doc=$1
    local version=$2
    cat <<- EOF
**Docker Pull Command**
``` bash
docker pull sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail:${version}
```
EOF >> $doc
}

function removeHistoryUnrelease () {
    local changeDoc=$ROOTDIR/CHANGELOG.md
    local tempFile=$(mktemp temp.XXXXXX)
    cat $changeDoc|grep "Unreleased" -B 500 > $tempFile
    cat $tempFile > $ROOTDIR/CHANGELOG.md
    rm -rf $tempFile
}

version=$1
milestone=$2
createReleaseFile $version $milestone
removeHistoryUnrelease

scripts/update_version.sh $version