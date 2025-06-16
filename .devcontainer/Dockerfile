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

FROM sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/loongcollector-community-edition/loongcollector-build-linux:2.1.6

ARG USERNAME=admin
ARG USER_PASSWORD
USER root

RUN wget https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/toolchain/glibc-2.18.tar.gz && \
    tar -zxvf glibc-2.18.tar.gz && \
    cd glibc-2.18 && \
    mkdir build && \
    cd build && \
    ../configure --prefix=/usr && \
    make -j$(nproc) && \
    make install && \
    cd ../ && \
    rm -fr glibc-2.18*

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
    if [ -n "$USER_PASSWORD" ]; then echo "$USERNAME:$USER_PASSWORD" | chpasswd; fi; \
    # the filename should not have the . or ~ symbol.
    USER_SUDOER_FILE=$(echo $USERNAME | sed 's/\./-/g'); \
    echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USER_SUDOER_FILE; \
    chmod 0440 /etc/sudoers.d/$USER_SUDOER_FILE; \
    chown -R $USERNAME:$GROUPNAME /opt $(eval echo ~$USERNAME); \
    chmod -R 755 $(eval echo ~$USERNAME);

USER $USERNAME

RUN go env -w GO111MODULE=on && go env -w GOPROXY=https://goproxy.cn,direct
