#!/usr/bin/env bash

# Copyright 2022 iLogtail Authors
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

# The script uses rsync to sync docs from current repository to destination gitbook repository.

set -ue
set -o pipefail

# intialize variables
ROOTDIR=$(cd $(dirname "${BASH_SOURCE[0]}") && cd .. && pwd)
SRC_DIR=$(realpath --relative-to=$PWD "$ROOTDIR/docs/cn/")
DEST_DIR=${1:-../ilogtail-docs}

rsync -av "$ROOTDIR/docs/cn/" "$DEST_DIR"
echo "Sync files from $SRC_DIR to $DEST_DIR finished."
echo "You may need to delete deperated files manually."
