# Copyright 2023 iLogtail Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

#docker buildx build --platform linux/amd64,linux/arm64 \
#        -t sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/centos7-cve-fix:1.0.0 \
#        -o type=registry \
#        --no-cache -f Dockerfile.centos7-cve-fix .

#docker buildx build --platform linux/amd64,linux/arm64 \
#        -t sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail-toolchain-linux:1.2.0 \
#        -o type=registry \
#        --no-cache -f Dockerfile.ilogtail-toolchain-linux .

#docker buildx build --sbom=false --provenance=false --platform linux/amd64,linux/arm64 \
#        -t sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail-build-linux:2.0.1 \
#        -o type=registry \
#        --no-cache -f Dockerfile.ilogtail-build-linux .

#curl -L https://github.com/regclient/regclient/releases/latest/download/regctl-linux-amd64 >regctl

regctl image copy sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail-build-linux:2.0.1 \
        sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail-build-linux:2.0.1
