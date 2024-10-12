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

NAME=libGoPluginBase.so
goc version
if [ $? != 0 ]; then
  echo "==========================================================================================================="
  echo "goc is not install on your test machine, please install it by the doc https://github.com/qiniu/goc."
  echo "If you don't find binary lib on your platform or arch, you also could run e2e with normal docker container."
  echo "==========================================================================================================="
  exit 1
fi

OUT_DIR=${1:-output}
PLUGINS_CONFIG_FILE=${2:-${PLUGINS_CONFIG_FILE:-plugins.yml,external_plugins.yml}}
GO_MOD_FILE=${3:-${GO_MOD_FILE:-go.mod}}
ROOTDIR=$(cd $(dirname "${BASH_SOURCE[0]}") && cd .. && pwd)
CURRDIR=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)
if [ -d "$ROOTDIR/$OUT_DIR" ]; then
  rm -rf "$ROOTDIR/$OUT_DIR"
fi
mkdir "$ROOTDIR/$OUT_DIR"

cd "$ROOTDIR"/plugin_main
pwd

if uname -s | grep Linux; then
  LDFLAGS=
  if uname -m | grep x86_64; then
    LDFLAGS='-extldflags "-Wl,--wrap=memcpy"'
  fi
  # make plugins stuffs
  "$CURRDIR/import_plugins.sh" "$PLUGINS_CONFIG_FILE" "$GO_MOD_FILE"
  goc build '--buildflags=-mod=mod -modfile="'"$ROOTDIR/$GO_MOD_FILE"'" -buildmode=c-shared -ldflags="'"$LDFLAGS"'"' --center=http://goc:7777 -o "$ROOTDIR/$OUT_DIR/${NAME}"
else
  echo "goc build only build a dynamic library in linux platform"
  exit 1
fi
