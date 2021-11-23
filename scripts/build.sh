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

function os() {
  if uname -s | grep Darwin; then
    return 2
  elif uname -s | grep Linux; then
    return 1
  else
    return 2
  fi
}

MOD=$1
BUILDMODE=$2
NAME=ilogtail
IDFLAGS=''

os
OS_FLAG=$?

ROOTDIR=$(cd $(dirname "${BASH_SOURCE[0]}") && cd .. && pwd)
if [ -d "$ROOTDIR"/bin ]; then
    rm -rf $ROOTDIR/bin
fi

mkdir "$ROOTDIR"/bin

if [ $OS_FLAG = 1 ]; then
  IDFLAGS='-extldflags "-Wl,--wrap=memcpy"'
  if [ $BUILDMODE = "c-shared" ]; then
    NAME=libPluginBase.so
  fi
elif [ $OS_FLAG = 2 ] && [ $BUILDMODE = "c-shared" ]; then
  NAME=libPluginBase.dll
fi

go build -mod="$MOD" -buildmode="$BUILDMODE" -ldflags="$IDFLAGS" -o "$ROOTDIR"/bin/${NAME} "$ROOTDIR"/main
