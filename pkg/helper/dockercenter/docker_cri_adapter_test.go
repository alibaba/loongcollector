// Copyright 2021 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//go:build linux
// +build linux

package dockercenter

import (
	"fmt"
	"testing"

	"github.com/docker/docker/api/types"

	"github.com/stretchr/testify/require"
)

func TestLookupContainerRootfsAbsDir(t *testing.T) {
	crirt := &CRIV1Alpha2Wrapper{
		dockerCenter: nil,
		client:       nil,
		runtimeName:  "containerd",
		containers:   make(map[string]*innerContainerInfo),
		stopCh:       make(<-chan struct{}),
		rootfsCache:  make(map[string]string),
	}

	container := types.ContainerJSON{
		ContainerJSONBase: &types.ContainerJSONBase{
			ID: "1234567890abcde",
		},
	}
	dir := crirt.lookupContainerRootfsAbsDir(container)
	require.Equal(t, dir, "")
}

func TestCriVersion(t *testing.T) {
	/*
		通过脚本本地安装多版本的containerd进行测试，预期结果如下

		| -------------- | ---------------- |
		| containerd 版本 | cri-api 支持的版本 |
		| v1.4.13        | v1alpha2         |
		| v1.5.17        | v1alpha2         |
		| v1.6.38        | v1alpha2, v1     |
		| v1.7.27        | v1alpha2, v1     |
		| v2.0.5         | v1               |
		| -------------- | ---------------- |

		脚本参考（使用方式,输入指定版本的containerd，直接安装，自动处理冲突 `./containerd-install.sh 2.0.5`）：

		```shell
		set -ue
		set -o pipefail

		SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

		# 检查是否传入版本号
		if [ -z "$1" ]; then
		    echo "Usage: $0 <version>"
		    exit 1
		fi

		VERSION="$1"
		CONTAINERD_DIR="${SCRIPT_DIR}/containerd-${VERSION}"
		DOWNLOAD_FILE="containerd-${VERSION}-linux-amd64.tar.gz"
		DOWNLOAD_PATH="${CONTAINERD_DIR}/${DOWNLOAD_FILE}"

		# 创建工作目录
		mkdir -p "${CONTAINERD_DIR}"
		cd "${CONTAINERD_DIR}"

		# 下载 containerd
		if [ -f "${DOWNLOAD_PATH}" ]; then
		    echo "Containerd version ${VERSION} already downloaded at ${DOWNLOAD_PATH}. Skipping download."
		else
		    echo "Downloading containerd version ${VERSION}..."
		    wget "https://github.com/containerd/containerd/releases/download/v${VERSION}/${DOWNLOAD_FILE}"
		fi

		# 解压并安装
		echo "Extracting containerd..."
		TEMP_DIR="${CONTAINERD_DIR}/bin-${VERSION}"
		mkdir -p "${TEMP_DIR}"
		tar -xvf "${DOWNLOAD_FILE}" -C "${TEMP_DIR}"
		echo "Adding version suffix to containerd binaries..."
		for binary in "${TEMP_DIR}/bin"/*; do
		    base_name=$(basename "$binary")
		    sudo mv "$binary" "/usr/local/sbin/${base_name}-${VERSION}"
		done
		rm -rf "${TEMP_DIR}"

		# 创建配置文件和数据目录
		echo "Creating directories for containerd-${VERSION}..."
		sudo rm -rf "/etc/containerd-${VERSION}"
		sudo mkdir -p "/etc/containerd-${VERSION}"
		sudo rm -rf "/var/lib/containerd-${VERSION}"
		sudo mkdir -p "/var/lib/containerd-${VERSION}"
		sudo rm -rf "/run/containerd-${VERSION}"
		sudo mkdir -p "/run/containerd-${VERSION}"
		sudo rm -rf "/opt/containerd-${VERSION}"
		sudo mkdir -p "/opt/containerd-${VERSION}"

		# 生成默认配置文件
		echo "Generating default config for containerd-${VERSION}..."
		sudo containerd-${VERSION} config default | sudo tee "${CONTAINERD_DIR}/config.toml"

		# 修改配置文件以使用不同的 Unix 域套接字路径和 state 路径
		echo "Updating config file paths..."
		sudo sed -i "s|/etc/containerd|/etc/containerd-${VERSION}|" "${CONTAINERD_DIR}/config.toml"
		sudo sed -i "s|/run/containerd|/run/containerd-${VERSION}|" "${CONTAINERD_DIR}/config.toml"
		sudo sed -i "s|/var/lib/containerd|/var/lib/containerd-${VERSION}|" "${CONTAINERD_DIR}/config.toml"
		sudo sed -i "s|/opt/containerd|/opt/containerd-${VERSION}|" "${CONTAINERD_DIR}/config.toml"
		sudo sed -i "s|containerd-shim|containerd-${VERSION}-shim|" "${CONTAINERD_DIR}/config.toml"

		# 停止旧版本的 containerd 进程（如果存在）
		echo "Stopping old containerd processes (if any)..."
		OLD_CONTAINERD_PIDS=$(pgrep -f "containerd-${VERSION}") || true
		if [ -n "$OLD_CONTAINERD_PIDS" ]; then
		    echo "Found running containerd-${VERSION} processes: $OLD_CONTAINERD_PIDS"
		    sudo kill $OLD_CONTAINERD_PIDS || true
		    echo "Old containerd-${VERSION} processes stopped."
		else
		    echo "No old containerd-${VERSION} processes found."
		fi

		# 启动 containerd
		echo "Starting containerd-${VERSION}..."
		nohup containerd-${VERSION} --config "${CONTAINERD_DIR}/config.toml" \
		    --root "/var/lib/containerd-${VERSION}" \
		    --state "/run/containerd-${VERSION}" \
		    > "${CONTAINERD_DIR}/stdout" \
		    2> "${CONTAINERD_DIR}/stderr" &

		echo "Containerd-${VERSION} started successfully."
		```
	*/
	containerdVersions := []string{
		// "1.4.13", // only support v1alpha2
		"1.5.17", // only support v1alpha2
		// "1.6.38", // support v1alpha2 and v1, use v1alpha2
		// "1.7.27", // support v1alpha2 and v1, use v1alpha2
		// "2.0.5",  // only support v1
	}
	for _, version := range containerdVersions {
		t.Log("start check cri", version)
		t.Log(IsCRIRuntimeValid(fmt.Sprintf("/run/containerd-%s/containerd.sock", version)))
	}
}
