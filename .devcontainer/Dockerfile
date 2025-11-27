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

FROM sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/loongcollector-community-edition/loongcollector-build-linux:2.1.12

ARG USERNAME=admin
ARG USER_PASSWORD
USER root

RUN yum -y install openssh-server && \
    ssh-keygen -A
    
# Create the user
COPY .env /tmp/.env
COPY authorized_keys /tmp/authorized_keys

RUN cp /opt/logtail/deps/lib/libssl.so.1.0.0 /usr/lib64; \
    cp /opt/logtail/deps/lib/libcrypto.so.1.0.0 /usr/lib64; \
    source /tmp/.env && rm /tmp/.env; \
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
    echo "$USERNAME     soft    nproc     unlimited" >> /etc/security/limits.d/20-nproc.conf; \
    chmod 0440 /etc/sudoers.d/$USER_SUDOER_FILE; \
    chmod -R 755 $(eval echo ~$USERNAME); \
    mkdir -p /home/$USERNAME/.ssh; \
    cp /tmp/authorized_keys /home/$USERNAME/.ssh/authorized_keys; \
    chmod 700 /home/$USERNAME/.ssh; \
    chmod 600 /home/$USERNAME/.ssh/authorized_keys; \
    chown -R $USERNAME:$GROUPNAME /opt $(eval echo ~$USERNAME); \
    rm /tmp/authorized_keys; \
    echo "export PATH=/usr/local/go/bin:/opt/logtail/deps/bin:$PATH" >> /home/$USERNAME/.bashrc; \
    su - $USERNAME -c "\
    go env -w GO111MODULE=on && \
    go env -w GOPROXY=https://goproxy.cn,direct"

USER $USERNAME
