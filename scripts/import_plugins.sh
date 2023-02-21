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

set -ue
set -o pipefail

CONFIG_FILE=${1:-${PLUGINS_CONFIG_FILE:-plugins.yml,external_plugins.yml}}
MOD_FILE=${2:-${GO_MOD_FILE:-go.mod}}
ROOT_DIR=${3:-$(cd $(dirname "${BASH_SOURCE[0]}") && cd .. && pwd)}

echo "===============GENERATING PLUGINS IMPORT=================="
echo "config: $CONFIG_FILE"
echo "modfile: $MOD_FILE"
echo "root-dir: $ROOT_DIR"
echo "=========================================================="

if [ "$CONFIG_FILE" == "off" ]; then
  echo "process disabled, nothing to do"
  return
fi

rm -rf "$ROOT_DIR/plugins/all/all.go"
rm -rf "$ROOT_DIR/plugins/all/all_debug.go"
rm -rf "$ROOT_DIR/plugins/all/all_windows.go"
rm -rf "$ROOT_DIR/plugins/all/all_linux.go"

go run -mod=mod "$ROOT_DIR/tools/builder" -root-dir="$ROOT_DIR" -config="$CONFIG_FILE" -modfile="$MOD_FILE" && \
echo "generating plugins finished successfully"
