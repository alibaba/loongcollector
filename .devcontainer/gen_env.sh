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

if [[ $(echo 'x\ty') = 'x\ty' ]]; then
  echo -e "HOST_OS=Linux\nUSERNAME=$USER\nUSER_UID=$(id -u $USER)\nGROUPNAME=$(id -gn $USER)\nGROUP_GID=$(id -g $USER)" > .env;
else
  echo "HOST_OS=Darwin\nUSERNAME=$USER\nUSER_UID=$(id -u $USER)\nGROUPNAME=root\nGROUP_GID=0" > .env;  
fi

PUB_KEY_PATH="$HOME/.ssh/id_rsa.pub"
if [ -f "$PUB_KEY_PATH" ]; then
  cp "$PUB_KEY_PATH" authorized_keys
  chmod 600 authorized_keys
else
  echo "Public key $PUB_KEY_PATH not found on host. Please generate it first."
  exit 1
fi