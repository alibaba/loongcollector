#!/bin/bash
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

TARGET_ARTIFACT_PATH=${TARGET_ARTIFACT_PATH:-"./core/build/unittest"}
CURSOR_FILE=${1:-""}
START_RUNNING=0
TESTS_RUN=0

search_files() {
    for file in "$1"/*; do
        if [ -d "$file" ]; then
            # Recursively handle folder
            search_files "$file"
        elif [[ -f "$file" ]]; then
            unittest="${file##*_}"
            if [ "$unittest" == "unittest" ]; then
                full_path=$(realpath "$file")
                if [ -n "$CURSOR_FILE" ] && [ $START_RUNNING -eq 0 ]; then
                    if [[ "$full_path" == "$CURSOR_FILE" ]]; then
                        START_RUNNING=1
                    else
                        continue
                    fi
                fi
                
                echo "[$(date '+%Y-%m-%d %H:%M:%S')] $full_path Start **********"
                cd "${full_path%/*}"
                if ! "./${full_path##*/}"; then
                    echo "Error: Test failed. You may resume the run by $0 $full_path"
                    exit 1
                fi
                cd - > /dev/null
                echo "[$(date '+%Y-%m-%d %H:%M:%S')] $full_path End ############"
                echo
                ((TESTS_RUN++))
            fi
        fi
    done
}

# Maybe some unittest depend on relative paths, so execute in the unittest directory
UT_BASE_PATH="$(pwd)/${TARGET_ARTIFACT_PATH:2}"
export LD_LIBRARY_PATH=${UT_BASE_PATH}:$LD_LIBRARY_PATH
cd $TARGET_ARTIFACT_PATH
search_files .
echo "All $TESTS_RUN tests completed successfully!"