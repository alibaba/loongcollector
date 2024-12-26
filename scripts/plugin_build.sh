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
    return 3
  fi
}

MOD=${1:-mod}
BUILDMODE=${2:-default}
OUT_DIR=${3:-output}
VERSION=${4:-0.0.1}
PLUGINS_CONFIG_FILE=${5:-${PLUGINS_CONFIG_FILE:-plugins.yml,external_plugins.yml}}
GO_MOD_FILE=${6:-${GO_MOD_FILE:-go.mod}}
NAME=loongcollector
LDFLAGS="${GO_LDFLAGS:-}"' -X "github.com/alibaba/ilogtail/pkg/config.BaseVersion='$VERSION'"'
BUILD_FLAG=${BUILD_FLAG:-}

os
OS_FLAG=$?

ROOTDIR=$(cd $(dirname "${BASH_SOURCE[0]}") && cd .. && pwd)
CURRDIR=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)
mkdir -p "$ROOTDIR"/bin

if [ $OS_FLAG = 1 ]; then
  if uname -m | grep x86_64; then
    LDFLAGS=$LDFLAGS' -extldflags "-Wl,--wrap=memcpy"'
  fi
  if [ $BUILDMODE = "c-shared" ]; then
    NAME=libGoPluginBase.so
  fi
elif [ $OS_FLAG = 3 ]; then
  export GOARCH=386
  export CGO_ENABLED=1
  if [ $BUILDMODE = "c-shared" ]; then
    NAME=GoPluginBase.dll
  fi
elif [ $OS_FLAG = 2 ]; then
  BUILDMODE=default
fi

# if not vendor mod, update plugin registry and go tidy
# otherwise you should have done it and go vendor
if [[ $MOD != "vendor" ]]; then
    "$CURRDIR/import_plugins.sh" "$PLUGINS_CONFIG_FILE" "$GO_MOD_FILE"
fi

# rebuild gozstd's libzstd.a, because it is not compatible in some env
GOOS=$(go env GOOS)
GOARCH=$(go env GOARCH)
if [[ $MOD = "vendor" ]]; then
  cd vendor/github.com/valyala/gozstd
else
  "$CURRDIR/import_plugins.sh" "$PLUGINS_CONFIG_FILE" "$GO_MOD_FILE"
  cd $(go env GOPATH)/pkg/mod/github.com/valyala/gozstd@*
fi
# if libzstd.a is available in the image, copy instead of rebuild
lib_name=libzstd_${GOOS}_${GOARCH}.a
if [[ -f /opt/logtail/deps/lib64/libzstd.a ]]; then
  sudo cp /opt/logtail/deps/lib64/libzstd.a libzstd_${GOOS}_${GOARCH}.a
else
  sudo MOREFLAGS=-fPIC make clean libzstd.a
  sudo mv libzstd__.a ${lib_name}
fi
GROUP=$(id -gn $USER)
sudo chown ${USER}:${GROUP} ${lib_name}
cd -

# make plugins stuffs
go build -mod="$MOD" -modfile="$GO_MOD_FILE" -buildmode="$BUILDMODE" -ldflags="$LDFLAGS" $BUILD_FLAG -o "$ROOTDIR/$OUT_DIR/${NAME}" "$ROOTDIR"/plugin_main
