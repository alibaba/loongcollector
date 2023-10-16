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

OS_FLAG=0
function os() {
  if uname -s | grep Darwin; then
    OS_FLAG=2
  elif uname -s | grep Linux; then
    OS_FLAG=1
  else
    OS_FLAG=3
  fi
}

os

ROOTDIR=$(cd $(dirname "${BASH_SOURCE[0]}") && cd .. && pwd)
SOURCEDIR="core/build/plugin"
[[ $# -eq 2 ]] && SOURCEDIR="$1" || :

if [ $OS_FLAG = 1 ]; then
  cp ${ROOTDIR}/core/build/go_pipeline/libPluginAdapter.so ${ROOTDIR}/pkg/logtail/libPluginAdapter.so
fi
