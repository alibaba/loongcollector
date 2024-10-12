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

FROM sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail-build-linux:2.0.3

ARG USERNAME=admin
USER root

RUN wget http://mirrors.ustc.edu.cn/gnu/libc/glibc-2.18.tar.gz && \
    tar -zxvf glibc-2.18.tar.gz && \
    cd glibc-2.18 && \
    mkdir build && \
    cd build && \
    ../configure --prefix=/usr && \
    make -j$(nproc) && \
    make install && \
    cd ../ && \
    rm -fr glibc-2.18*

# install python3.8
RUN cd /opt && curl -O https://mirrors.aliyun.com/python-release/source/Python-3.8.12.tgz && \
    tar -zxvf Python-3.8.12.tgz && cd Python-3.8.12 && \
    mkdir /usr/local/python3 && \
    ./configure --prefix=/usr/local/python3 && \
    make clean && make && make install && \
    cp /usr/local/python3/bin/python3.8 /usr/bin/python3
# install gcovr
RUN python3 -m pip install --upgrade pip
RUN cp /usr/local/python3/bin/pip3 /usr/bin/pip3 && pip3 config set global.index-url https://mirrors.aliyun.com/pypi/simple/ && pip3 install gcovr==7.0
RUN cp /usr/local/python3/bin/gcovr /usr/bin/gcovr

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
