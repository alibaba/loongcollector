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

# There are 3 kinds of test, which are e2e, core and performance.
TYPE=$1
TEST_SCOPE=$2
AGENT=$3

# 处理TEST_CASE环境变量
if [ -n "$TEST_CASE" ]; then
  echo "Running specific test case: $TEST_CASE"
  # 验证测试用例是否存在
  if [ ! -d "$(cd $(dirname "${BASH_SOURCE[0]}") && cd ../test/e2e/test_cases/$TEST_CASE)" ]; then
    echo "Error: Test case directory '$TEST_CASE' not found"
    exit 1
  fi
  if [ ! -f "$(cd $(dirname "${BASH_SOURCE[0]}") && cd ../test/e2e/test_cases/$TEST_CASE/case.feature)" ]; then
    echo "Error: Test case '$TEST_CASE' is missing case.feature file"
    exit 1
  fi
  # 设置环境变量供测试使用
  export TEST_CASE="$TEST_CASE"
else
  echo "Running all test cases"
fi

ROOT_DIR=$(cd $(dirname "${BASH_SOURCE[0]}") && cd .. && pwd)
TESTDIR=$ROOT_DIR/test

cd "$TESTDIR"
if [ "$TEST_SCOPE" = "performance" ]; then
  if [ -n "$AGENT" ]; then
    export AGENT="$AGENT"
    if [ "$TYPE" = "benchmark" ]; then
      go test -v -timeout 30m -run ^TestE2EOnDockerComposePerformance$ github.com/alibaba/ilogtail/test/$TYPE/local
    else
      go test -v -timeout 30m -run ^TestE2EOnDockerComposePerformance$ github.com/alibaba/ilogtail/test/$TYPE
    fi
  else
    if [ "$TYPE" = "benchmark" ]; then
      go test -v -timeout 30m -run ^TestE2EOnDockerComposePerformance$ github.com/alibaba/ilogtail/test/$TYPE/local
    else
      go test -v -timeout 30m -run ^TestE2EOnDockerComposePerformance$ github.com/alibaba/ilogtail/test/$TYPE
    fi
  fi
else
  go test -v -timeout 30m -run ^TestE2EOnDockerCompose$ github.com/alibaba/ilogtail/test/$TYPE
fi

if [ $? = 0 ]; then
  echo "========================================="
  echo "All testing cases are passed"
  echo "========================================="
else
  exit 1
fi
