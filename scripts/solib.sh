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

ROOTDIR=$(cd $(dirname "${BASH_SOURCE[0]}") && cd .. && pwd)

if [ -d "$ROOTDIR"/bin ]; then
  rm -rf "$ROOTDIR"/bin
fi

mkdir "$ROOTDIR"/bin

id=$(docker create aliyun/ilogtail:github-latest)
echo "$id"
docker cp "$id":/src/bin/libPluginBase.so "$ROOTDIR"/bin
docker rm -v "$id"
