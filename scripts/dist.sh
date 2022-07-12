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

# intialize variables
OUT_DIR=${1:-output}
DIST_DIR=${2:-ilogtail-1.1.0}
ROOTDIR=$(cd $(dirname "${BASH_SOURCE[0]}") && cd .. && pwd)

# prepare dist dir
mkdir "${ROOTDIR}/${DIST_DIR}"
cp LICENSE README.md README-cn.md "${ROOTDIR}/${DIST_DIR}"
cp "${ROOTDIR}/${OUT_DIR}/ilogtail" "${ROOTDIR}/${DIST_DIR}"
cp "${ROOTDIR}/${OUT_DIR}/libPluginAdapter.so" "${ROOTDIR}/${DIST_DIR}"
cp "${ROOTDIR}/${OUT_DIR}/libPluginBase.so" "${ROOTDIR}/${DIST_DIR}"
cp "${ROOTDIR}/${OUT_DIR}/ilogtail_config.json" "${ROOTDIR}/${DIST_DIR}"
cp -a "${ROOTDIR}/${OUT_DIR}/user_yaml_config.d" "${ROOTDIR}/${DIST_DIR}"
cp -a "${ROOTDIR}/example_config" "${ROOTDIR}/${DIST_DIR}"

# pack dist dir
cd "${ROOTDIR}"
tar -cvzf "${DIST_DIR}.tar.gz" "${DIST_DIR}"
rm -rf "${DIST_DIR}"
cd -