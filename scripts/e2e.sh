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

function run() {
  name=$1
  command=$2
  test_home=$3
  echo "========================================="
  echo "$name testing case"
  echo "========================================="
  
  eval "$command"
  if [ $? = 1 ]; then
      cat $test_home/report/"$name"_report.json
      echo "========================================="
      echo "$name testing case failed"
      echo "========================================="
      exit 1
  fi
}

# There are 2 kinds of test, which are e2e and performance.
TYPE=$1
TEST_SCOPE=$2

ROOT_DIR=$(cd $(dirname "${BASH_SOURCE[0]}") && cd .. && pwd)
TESTDIR=$ROOT_DIR/test
TEST_HOME=$ROOT_DIR/$TYPE-test

rm -rf "$TEST_HOME"
mkdir "$TEST_HOME"

cd "$TESTDIR"
go build -buildvcs=false -v -o "$TEST_HOME"/ilogtail-test-tool "$TESTDIR"

if [ $? != 0 ]; then
  echo "build ilogtail e2e engine failed"
  exit 1
fi

cd "$TEST_HOME"

prefix="./ilogtail-test-tool start"
if [ "$TEST_DEBUG" = "true" ]; then
  prefix=$prefix" --debug"
fi
if [ "$TEST_PROFILE" = "true" ]; then
  prefix=$prefix" --profile"
fi

if [ "$TEST_SCOPE" = "all" ]; then
  ls "$TESTDIR"/cases/plugin/scenarios/"$TYPE" | while read case
  do
    # currently, latest github runner cannot run ebpf program, skip it.
    if [ "$case" != "input_observer_dns" -a "$case" != "input_observer_http" ]; then
      command=$prefix" -c $TESTDIR/cases/plugin/scenarios/$TYPE/$case"
      run "$case" "$command" "$TEST_HOME"
      if [ $? = 1 ]; then
        exit 1
      fi
    fi
  done
else
  command=$prefix" -c $TESTDIR/cases/plugin/scenarios/$TYPE/$TEST_SCOPE"
  run "$TEST_SCOPE" "$command" "$TEST_HOME"
fi

if [ $? = 0 ]; then
  sh "$ROOT_DIR"/scripts/e2e_coverage.sh "$TYPE"-test
  echo "========================================="
  echo "All testing cases are passed"
  echo "========================================="
else
  exit 1
fi
