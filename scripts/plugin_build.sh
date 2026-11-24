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

MOD=${1:-mod}
OUT_DIR=${3:-output}
VERSION=${4:-0.0.1}
PLUGINS_CONFIG_FILE=${5:-${PLUGINS_CONFIG_FILE:-plugins.yml,external_plugins.yml}}
GO_MOD_FILE=${6:-${GO_MOD_FILE:-go.mod}}
LDFLAGS="${GO_LDFLAGS:-}"' -X "github.com/alibaba/ilogtail/pkg/config.BaseVersion='$VERSION'"'
BUILD_FLAG=${BUILD_FLAG:-}
# BUILD_FLAG='"-gcflags=all=-N -l"' # disable optimization and inlining for debuggablity
BUILD_TAGS=${BUILD_TAGS:-}

if [[ ${2:-c-shared} != "c-shared" ]]; then
  printf 'Unsupported Go plugin build mode %s (expected c-shared)\n' "$2" >&2
  exit 1
fi

ROOTDIR=$(cd $(dirname "${BASH_SOURCE[0]}") && cd .. && pwd)
CURRDIR=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)

OS_NAME=$(uname -s)
case "$OS_NAME" in
  Linux)
    if [ "$(uname -m)" = "x86_64" ]; then
      LDFLAGS=$LDFLAGS' -extldflags "-Wl,--wrap=memcpy"'
    fi
    if [ "${ENABLE_CORP_FEATURE:-}" = "ON" ]; then
      NAME=libPluginBase.so
      BUILD_TAGS="${BUILD_TAGS} corp"
    else
      NAME=libGoPluginBase.so
    fi
    ;;
  MINGW*|MSYS*|CYGWIN*)
    export GOARCH=386
    export CGO_ENABLED=1
    NAME=GoPluginBase.dll
    ;;
  *)
    printf 'Go plugin shared libraries are unsupported on %s\n' "$OS_NAME" >&2
    exit 1
    ;;
esac

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
  cd $(go env GOMODCACHE)/github.com/valyala/gozstd@*
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
go build -mod="$MOD" -modfile="$GO_MOD_FILE" -buildmode=c-shared -ldflags="$LDFLAGS" $BUILD_FLAG -tags "$BUILD_TAGS" -o "$ROOTDIR/$OUT_DIR/${NAME}" "$ROOTDIR"/plugin_main
