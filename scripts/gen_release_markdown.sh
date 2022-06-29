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
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 1.1.0(version) 11(milestone)" >&2
    exit 1
fi

INITPWD=$PWD
ROOTDIR=$(cd $(dirname $0) && cd .. && pwd)


function createReleaseFile () {
    version=$1
    mileston=$2
    if [ ! -d "${ROOTDIR}/changes" ]; then
        mkdir ${ROOTDIR}/changes
    fi
    doc=${ROOTDIR}/changes/v${version}.md
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
}


function appendUnreleaseChanges () {
    doc=$1
    changeDoc=$ROOTDIR/CHANGELOG.md
    tempFile=$(mktemp temp.XXXXXX)
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
    doc=$1
    version=$2
    echo "| Arch| Platform| Region| Link|" >> $doc
    echo "|  ----  | ----  | ----  | ----  |" >> $doc
    echo "|arm64|Linux|China|[link](https://logtail-release-cn-hangzhou.oss-cn-hangzhou.aliyuncs.com/linux64/${version}/aarch64/logtail-linux64.tar.gz)|" >> $doc
    echo "|amd64|Linux|China|[link](https://logtail-release-cn-hangzhou.oss-cn-hangzhou.aliyuncs.com/linux64/${version}/x86_64/logtail-linux64.tar.gz)" >> $doc
    echo "|arm64|Linux|US|[link](https://logtail-release-us-west-1.oss-us-west-1.aliyuncs.com/linux64/${version}/aarch64/logtail-linux64.tar.gz)" >> $doc
    echo "|amd64|Linux|US|[link](https://logtail-release-us-west-1.oss-us-west-1.aliyuncs.com/linux64/${version}/x86_64/logtail-linux64.tar.gz)" >> $doc
}


function removeHistoryUnrelease () {
    changeDoc=$ROOTDIR/CHANGELOG.md
    tempFile=$(mktemp temp.XXXXXX)
    cat $changeDoc|grep "Unreleased" -B 500 > $tempFile
    cat $tempFile > $ROOTDIR/CHANGELOG.md
    rm -rf $tempFile
}

version=$1
milestone=$2
createReleaseFile $version $milestone
removeHistoryUnrelease

scripts/update_version.sh $version