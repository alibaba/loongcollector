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

GO_ADDLICENSE=$(go env GOPATH)/bin/addlicense
OPERATION=$1
SCOPE=$2

command="$GO_ADDLICENSE -v -c \"iLogtail Authors\""
if [ "$OPERATION" = "check" ]; then
  command=$command"  -check"
fi
command+=' -ignore "**/.idea/**" -ignore "**/protocol/**" -ignore "**/oldtest/**"'
command+=' -ignore "**/internal/**" -ignore "**/diagnose/**" -ignore "**/external/**" -ignore "**/*.html"'
command+=' -ignore "**/core/protobuf/**/*.pb.*" -ignore "core/common/Version.cpp"'
command+=" $SCOPE"
eval "$command"

