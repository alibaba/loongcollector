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

DOWNLOAD_DIR=${1:-.}

US_REGION="logtail-release-us-west-1.oss-us-west-1.aliyuncs.com"
CN_REGION="logtail-release-cn-beijing.oss-cn-beijing.aliyuncs.com"
cn_time_connect=$(curl -o /dev/null -s -m 3 -w "%{time_total}" https://$CN_REGION) || cn_time_connect=9999
us_time_connect=$(curl -o /dev/null -s -m 4 -w "%{time_total}" https://$US_REGION) || us_time_connect=9999
OSS_REGION=$CN_REGION
if (( $(echo "$us_time_connect < $cn_time_connect" | bc) )); then
    OSS_REGION=$US_REGION
fi
echo "cn_time_connect=$cn_time_connect us_time_connect=$us_time_connect OSS_REGION=$OSS_REGION"

curl -sfL -m 30 https://$OSS_REGION/kubernetes/libebpf.so -o ${DOWNLOAD_DIR}/libebpf.so

curl -sfL -m 30 https://$OSS_REGION/kubernetes/libnetwork_observer.so -o ${DOWNLOAD_DIR}/libnetwork_observer.so
curl -sfL -m 30 https://$OSS_REGION/kubernetes/libcoolbpf.so.1.0.0 -o ${DOWNLOAD_DIR}/libcoolbpf.so.1.0.0
