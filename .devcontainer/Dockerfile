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

FROM sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail-build-linux:gcc_9.3.1-1

ARG USERNAME=admin
USER root

# Create the user
COPY .env /tmp/.env
RUN source /tmp/.env && rm /tmp/.env; \
    if getent passwd $USERNAME; then userdel -f $USERNAME; fi; \
    if [ $HOST_OS = "Linux" ]; then \
    if getent group $GROUPNAME; then groupdel -f $GROUPNAME; fi; \
    if getent group $GROUP_GID; then TMP_NAME=$(getent group $GROUP_GID | cut -d: -f1); groupdel $TMP_NAME; fi; \
    groupadd --gid $GROUP_GID $GROUPNAME; \
    fi; \
    useradd --uid $USER_UID --gid $GROUP_GID -m $USERNAME; \
    echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME; \
    chmod 0440 /etc/sudoers.d/$USERNAME; \
    chown -R $USERNAME:$GROUPNAME /opt $(eval echo ~$USERNAME); \
    chmod -R 755 $(eval echo ~$USERNAME);

USER $USERNAME

RUN go env -w GO111MODULE=on && go env -w GOPROXY=https://goproxy.cn,direct