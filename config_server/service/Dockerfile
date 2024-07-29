# Copyright 2023 iLogtail Authors
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

FROM sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail-build-linux:2.0.3 as build

USER root
WORKDIR /src
COPY . .
RUN go env -w GOPROXY="https://goproxy.cn,direct" && \
    go build -o ConfigServer

FROM centos:centos7.9.2009
MAINTAINER TomYu yyuuttaaoo@gmail.com

ENV container docker

RUN curl -L -o /etc/yum.repos.d/CentOS-Base.repo http://mirrors.aliyun.com/repo/Centos-7.repo

RUN yum update -y && yum upgrade -y && yum -y clean all && rm -fr /var/cache && rm -rf /core.*

WORKDIR /config_server
COPY --from=build /src/ConfigServer /config_server/ConfigServer
COPY --from=build /src/setting/setting.json /config_server/setting/setting.json
RUN chmod 755 /config_server/ConfigServer && \
    sed -i 's/127\.0\.0\.1/0\.0\.0\.0/' setting/setting.json
ENV GIN_MODE=release

CMD ["./ConfigServer"]
