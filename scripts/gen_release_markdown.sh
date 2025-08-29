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

function createReleaseFile() {
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
    echo "# ${version}" >>$doc
    echo >>$doc
    echo "## Changes" >>$doc
    echo >>$doc
    echo "All issues and pull requests are [here](https://github.com/alibaba/ilogtail/milestone/${mileston})." >>$doc
    echo >>$doc
    appendUnreleaseChanges $doc
    echo >>$doc

    echo "## Download" >>$doc
    echo >>$doc
    appendDownloadLinks $doc $version
    echo >>$doc
    echo "## Docker Image" >>$doc
    echo >>$doc
    appendDockerImageLinks $doc $version
}

function appendUnreleaseChanges() {
    local doc=$1
    local changeDoc=$ROOTDIR/CHANGELOG.md
    local tempFile=$(mktemp temp.XXXXXX)
    cat $changeDoc | grep "Unreleased" -A 500 | grep -v "Unreleased" | grep -v '^$' >>$tempFile || :
    echo "### Features" >>$doc
    echo >>$doc
    cat $tempFile | grep -E '\[added\]|\[updated\]|\[deprecated\]|\[removed\]' >>$doc || :
    echo >>$doc
    echo "### Fixed" >>$doc
    echo >>$doc
    cat $tempFile | grep -E '\[fixed\]|\[security\]' >>$doc || :
    echo >>$doc
    echo "### Doc" >>$doc
    echo >>$doc
    cat $tempFile | grep -E '\[doc\]' >>$doc || :
    rm -rf $tempFile
}

function appendDownloadLinks() {
    local doc=$1
    local version=$2
    local linux_amd64_url="https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/${version}/ilogtail-${version}.linux-amd64.tar.gz"
    local linux_amd64_sig="https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/${version}/ilogtail-${version}.linux-amd64.tar.gz.sha256"
    local linux_arm64_url="https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/${version}/ilogtail-${version}.linux-arm64.tar.gz"
    local linux_arm64_sig="https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/${version}/ilogtail-${version}.linux-arm64.tar.gz.sha256"
    local windows_amd64_url="https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/${version}/ilogtail-${version}.windows-amd64.zip"
    local windows_amd64_sig="https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/${version}/ilogtail-${version}.windows-amd64.zip.sha256"
    cat >>$doc <<-EOF
| **Filename** | **OS** | **Arch** | **SHA256 Checksum** |
|  ----  | ----  | ----  | ----  |
|[ilogtail-${version}.linux-amd64.tar.gz](${linux_amd64_url})|Linux|x86-64|[ilogtail-${version}.linux-amd64.tar.gz.sha256](${linux_amd64_sig})|
|[ilogtail-${version}.linux-arm64.tar.gz](${linux_arm64_url})|Linux|arm64|[ilogtail-${version}.linux-arm64.tar.gz.sha256](${linux_arm64_sig})|
|[ilogtail-${version}.windows-amd64.zip](${windows_amd64_url})|Windows|x86-64|[ilogtail-${version}.windows-amd64.zip.sha256](${windows_amd64_sig})|
EOF
}

function appendDockerImageLinks() {
    local doc=$1
    local version=$2
    cat >>$doc <<-EOF
**Docker Pull Command**
\`\`\` bash
docker pull sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail:${version}
\`\`\`
EOF
}

function removeHistoryUnrelease() {
    local changeDoc=$ROOTDIR/CHANGELOG.md
    local tempFile=$(mktemp temp.XXXXXX)
    cat $changeDoc | grep "Unreleased" -B 500 >$tempFile
    cat $tempFile >$ROOTDIR/CHANGELOG.md
    rm -rf $tempFile
}

version=$1
milestone=$2
createReleaseFile $version $milestone
removeHistoryUnrelease

scripts/update_version.sh $version
