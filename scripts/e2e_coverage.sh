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




goc version
if [ $? != 0 ]; then
    echo "==========================================================================================================="
    echo "goc is not install on your test machine, please install it by the doc https://github.com/qiniu/goc."
    echo "If you don't find binary lib on your platform or arch, you also could run e2e with normal docker container."
    echo "==========================================================================================================="
    exit 1
fi


ROOTDIR=$(cd $(dirname "${BASH_SOURCE[0]}") && cd .. && pwd)
PACKAGE=$1

cd "$ROOTDIR"/"$PACKAGE"/report

FILES=""
for file in `ls .|grep "coverage.out"`
do
  if [ -s "$file" ]; then
      FILES=$FILES$file" "
  fi
done

if [ "$FILES" = "" ]; then
  exit 0
fi

CMD="goc merge "$FILES" -o merge.cov"

eval "$CMD"
go tool cover -func=merge.cov -o coverage.func
go tool cover -html=merge.cov -o coverage.html