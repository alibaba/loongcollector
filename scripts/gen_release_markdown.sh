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
    echo "Usage: $0 <version> <milestone_id>" >&2
    echo "Example: $0 3.1.4 26" >&2
    echo "Note: milestone_id is the number from the milestone URL, e.g., https://github.com/\${REPO_OWNER}/\${REPO_NAME}/milestone/26" >&2
    exit 1
fi

INITPWD=$PWD
ROOTDIR=$(cd $(dirname $0) && cd .. && pwd)

# GitHub repository information
# 从GitHub Actions环境变量中获取，如果没有则使用默认值
if [ -n "${GITHUB_REPOSITORY:-}" ]; then
    # 从GITHUB_REPOSITORY中解析owner和name
    REPO_OWNER=$(echo "$GITHUB_REPOSITORY" | cut -d'/' -f1)
    REPO_NAME=$(echo "$GITHUB_REPOSITORY" | cut -d'/' -f2)
else
    # 如果没有GITHUB_REPOSITORY，尝试使用单独的变量
    REPO_OWNER="${GITHUB_REPOSITORY_OWNER:-alibaba}"
    REPO_NAME="${GITHUB_REPOSITORY_NAME:-ilogtail}"
fi

function createReleaseFile() {
    local version=$1
    local milestone_id=$2
    
    if [ ! -d "${ROOTDIR}/changes" ]; then
        mkdir -p ${ROOTDIR}/changes
    fi
    
    local doc=${ROOTDIR}/changes/v${version}.md
    if [ -f "${doc}" ]; then
        rm -rf ${doc}
    fi
    
    touch $doc
    echo "# Release v${version}" >>$doc
    echo >>$doc
    echo "## Changes" >>$doc
    echo >>$doc
    
    # 获取milestone信息
    local milestone_url="https://github.com/$REPO_OWNER/$REPO_NAME/milestone/${milestone_id}"
    echo "All issues and pull requests are [here](${milestone_url})." >>$doc
    echo >>$doc
    
    # 尝试获取milestone标题
    if command -v gh >/dev/null 2>&1; then
        local milestone_title=$(gh api repos/$REPO_OWNER/$REPO_NAME/milestones/$milestone_id --jq '.title' 2>/dev/null || echo "v${version}")
        echo "**Milestone:** $milestone_title" >>$doc
        echo >>$doc
    fi
    
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
    
    if [ ! -f "$changeDoc" ]; then
        echo "Warning: CHANGELOG.md not found, skipping unreleased changes" >&2
        return
    fi
    
    local tempFile=$(mktemp temp.XXXXXX)
    
    # 提取Unreleased部分的内容
    if grep -q "## \[Unreleased\]" "$changeDoc"; then
        # 使用awk提取Unreleased部分，直到遇到下一个##开头的行
        awk '/^## \[Unreleased\]/{flag=1; next} /^## \[/{flag=0} flag{print}' "$changeDoc" > "$tempFile"
    else
        echo "Warning: No [Unreleased] section found in CHANGELOG.md" >&2
        touch "$tempFile"
    fi
    
    # 按类型分类输出
    echo "### Features" >>$doc
    echo >>$doc
    cat $tempFile | grep -E '\[added\]|\[updated\]|\[deprecated\]|\[removed\]' >>$doc || echo "- No new features" >>$doc
    echo >>$doc
    
    echo "### Fixed" >>$doc
    echo >>$doc
    cat $tempFile | grep -E '\[fixed\]|\[security\]' >>$doc || echo "- No bug fixes" >>$doc
    echo >>$doc
    
    echo "### Documentation" >>$doc
    echo >>$doc
    cat $tempFile | grep -E '\[doc\]' >>$doc || echo "- No documentation changes" >>$doc
    echo >>$doc
    
    echo "### Tests" >>$doc
    echo >>$doc
    cat $tempFile | grep -E '\[test\]' >>$doc || echo "- No test changes" >>$doc
    echo >>$doc
    
    rm -rf $tempFile
}

function appendDownloadLinks() {
    local doc=$1
    local version=$2
    
    # 更新下载链接格式，支持更多平台
    local linux_amd64_url="https://loongcollector-community-edition.oss-cn-shanghai.aliyuncs.com/${version}/loongcollector-${version}.linux-amd64.tar.gz"
    local linux_amd64_sig="https://loongcollector-community-edition.oss-cn-shanghai.aliyuncs.com/${version}/loongcollector-${version}.linux-amd64.tar.gz.sha256"
    local linux_arm64_url="https://loongcollector-community-edition.oss-cn-shanghai.aliyuncs.com/${version}/loongcollector-${version}.linux-arm64.tar.gz"
    local linux_arm64_sig="https://loongcollector-community-edition.oss-cn-shanghai.aliyuncs.com/${version}/loongcollector-${version}.linux-arm64.tar.gz.sha256"
    local windows_amd64_url="https://loongcollector-community-edition.oss-cn-shanghai.aliyuncs.com/${version}/loongcollector-${version}.windows-amd64.zip"
    local windows_amd64_sig="https://loongcollector-community-edition.oss-cn-shanghai.aliyuncs.com/${version}/loongcollector-${version}.windows-amd64.zip.sha256"
    
    cat >>$doc <<-EOF
| **Filename** | **OS** | **Arch** | **SHA256 Checksum** |
|  ----  | ----  | ----  | ----  |
|[loongcollector-${version}.linux-amd64.tar.gz](${linux_amd64_url})|Linux|x86-64|[loongcollector-${version}.linux-amd64.tar.gz.sha256](${linux_amd64_sig})|
|[loongcollector-${version}.linux-arm64.tar.gz](${linux_arm64_url})|Linux|arm64|[loongcollector-${version}.linux-arm64.tar.gz.sha256](${linux_arm64_sig})|
|[loongcollector-${version}.windows-amd64.zip](${windows_amd64_url})|Windows|x86-64|[loongcollector-${version}.windows-amd64.zip.sha256](${windows_amd64_sig})|
EOF
}

function appendDockerImageLinks() {
    local doc=$1
    local version=$2
    
    cat >>$doc <<-EOF
**Docker Pull Command**
\`\`\` bash
docker pull sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/loongcollector-community-edition/loongcollector:${version}
docker pull ghcr.io/${REPO_OWNER}/loongcollector:${version}
docker pull ghcr.io/${REPO_OWNER}/loongcollector:latest
\`\`\`

**Docker Image Tags**
- \`sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/loongcollector-community-edition/loongcollector:${version}\`
- \`ghcr.io/${REPO_OWNER}/loongcollector:${version}\`
- \`ghcr.io/${REPO_OWNER}/loongcollector:latest\`
EOF
}

function removeHistoryUnrelease() {
    local changeDoc=$ROOTDIR/CHANGELOG.md
    
    if [ ! -f "$changeDoc" ]; then
        echo "Warning: CHANGELOG.md not found, skipping cleanup" >&2
        return
    fi
    
    local tempFile=$(mktemp temp.XXXXXX)
    
    # 保留Unreleased部分之前的内容，并添加新的版本条目
    if grep -q "## \[Unreleased\]" "$changeDoc"; then
        # 提取Unreleased之前的内容
        awk '/^## \[Unreleased\]/{exit} {print}' "$changeDoc" > "$tempFile"
        
        # 添加新的版本条目
        echo "" >> "$tempFile"
        echo "## [$version] - $(date +%Y-%m-%d)" >> "$tempFile"
        echo "" >> "$tempFile"
        echo "See [changes/v${version}.md](changes/v${version}.md) for details." >> "$tempFile"
        echo "" >> "$tempFile"
        echo "## [Unreleased]" >> "$tempFile"
        echo "" >> "$tempFile"
        
        # 替换原文件
        cat "$tempFile" > "$changeDoc"
    else
        echo "Warning: No [Unreleased] section found in CHANGELOG.md" >&2
    fi
    
    rm -rf $tempFile
}

function validateMilestone() {
    local milestone_id=$1
    
    if command -v gh >/dev/null 2>&1; then
        local milestone_info=$(gh api repos/$REPO_OWNER/$REPO_NAME/milestones/$milestone_id 2>/dev/null || echo "")
        if [ -z "$milestone_info" ]; then
            echo "Warning: Milestone $milestone_id not found or not accessible" >&2
            return 1
        fi
    else
        echo "Warning: GitHub CLI (gh) not found, skipping milestone validation" >&2
    fi
    return 0
}

# 主执行逻辑
version=$1
milestone_id=$2

echo "Generating release markdown for version: $version"
echo "Using milestone ID: $milestone_id"

# 验证milestone
if ! validateMilestone $milestone_id; then
    echo "Warning: Milestone validation failed, but continuing with release generation..." >&2
fi

# 创建release文件
createReleaseFile $version $milestone_id

# 清理CHANGELOG.md中的Unreleased部分
removeHistoryUnrelease

# 更新版本号
if [ -f "scripts/update_version.sh" ]; then
    echo "Updating version numbers..."
    chmod +x scripts/update_version.sh
    ./scripts/update_version.sh $version
else
    echo "Warning: update_version.sh not found, skipping version updates" >&2
fi

echo "Release markdown generation completed!"
echo "Generated files:"
echo "  - changes/v${version}.md"
echo "  - Updated CHANGELOG.md"
