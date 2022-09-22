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

cd $(dirname "${BASH_SOURCE[0]}")

machine=`uname -m`
[[ $machine = 'aarch64' ]] && arch=arm64 || arch=amd64

docker build --rm -t local/c7-systemd-linux-${arch} . -f Dockerfile.c7-systemd-linux
docker build --rm -t local/ilogtail-toolchain-linux-${arch} . -f Dockerfile.ilogtail-toolchain-linux-${arch}
docker build --rm -t local/ilogtail-build-linux-${arch} . -f Dockerfile.ilogtail-build-linux-${arch}

cd -
