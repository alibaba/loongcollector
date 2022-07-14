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

[[ $# -ne 2 ]] && {
    echo "Usage: $0 version"
} || :

export VERSION=$1
make dist
sha256sum ilogtail-$VERSION.tar.gz > ilogtail-$VERSION.tar.gz.sha256
ossutil64 cp ilogtail-$VERSION.tar.gz oss://ilogtail-community-edition/$VERSION/ilogtail-$VERSION.linux-amd64.tar.gz
ossutil64 cp ilogtail-$VERSION.tar.gz.sha256 oss://ilogtail-community-edition/$VERSION/ilogtail-$VERSION.linux-amd64.tar.gz.sha256