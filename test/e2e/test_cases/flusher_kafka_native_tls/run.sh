#!/usr/bin/env bash

# Copyright 2025 iLogtail Authors
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

wait_for_port() {
  local host="$1"
  local port="$2"
  local timeout="${3:-120}"
  local interval=2
  echo "waiting for ${host}:${port} to become ready ..."
  local start_time
  start_time=$(date +%s)
  while true; do
    if (exec 3<>"/dev/tcp/${host}/${port}") 2>/dev/null; then
      exec 3>&-
      exec 3<&-
      echo "${host}:${port} is ready"
      return 0
    fi
    local now
    now=$(date +%s)
    if (( now - start_time >= timeout )); then
      echo "timeout while waiting for ${host}:${port}" >&2
      return 1
    fi
    sleep "${interval}"
  done
}
wait_for_port "kafka" 29093 150 || exit 1

sleep 3
for i in {1..30} ; do
  echo "{\"msg\":\"hello-$i\"}"
  sleep 0.1
done
sleep 3600

