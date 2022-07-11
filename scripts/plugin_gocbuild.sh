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

NAME=libPluginBase.so
goc version
if [ $? != 0 ]; then
  echo "==========================================================================================================="
  echo "goc is not install on your test machine, please install it by the doc https://github.com/qiniu/goc."
  echo "If you don't find binary lib on your platform or arch, you also could run e2e with normal docker container."
  echo "==========================================================================================================="
  exit 1
fi

OUT_DIR=${1:-output}
ROOTDIR=$(cd $(dirname "${BASH_SOURCE[0]}") && cd .. && pwd)
if [ -d "$ROOTDIR/$OUT_DIR" ]; then
  rm -rf "$ROOTDIR/$OUT_DIR"
fi
mkdir "$ROOTDIR/$OUT_DIR"

cd "$ROOTDIR"/plugin_main
pwd

if uname -s | grep Linux; then
  goc build '--buildflags=-mod=vendor -buildmode=c-shared -ldflags="-extldflags "-Wl,--wrap=memcpy""' --center=http://goc:7777 -o "$ROOTDIR/$OUT_DIR/${NAME}"
else
  echo "goc build only build a dynamic library in linux platform"
  exit 1
fi
