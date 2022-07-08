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

caller_dir="$PWD"
ilogtail_dir=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)
ilogtail_pid=
delaySec=0
exit_flag=0
exit_timeout=10

gen_config() {
    :
}

start_ilogtail() {
    ./ilogtail &
    ilogtail_pid=$!
    echo "ilogtail started. pid: $ilogtail_pid"
}

check_liveness() {
    # check if process exists
    [[ -d /proc/$ilogtail_pid ]] || {
        echo "ilogtail process exited."
        return 1
    }
    pid_status=`head /proc/$ilogtail_pid/status | grep "State:*"`
    # check if process is zombie
    [[ "$pid_status" =~ .*"zombie"*. ]] && \
        echo "ilogtail process became zombie" && \
        return 2 || :
    # ilogtail is healthy
    return 0
}

stop_ilogtail () {
    # just return if ilogtail has not started
    [[ -z $ilogtail_pid ]] && echo stop ilogtail done || :

    echo 'delay stop ilogtail, sleep ' $delaySec
    sleep $delaySec
    echo stop ilogtail
    # try to kill with SIGTERM, and wait for process to exit
    kill $ilogtail_pid
    exit_time=0
    exit_result="force killed"
    while [[ $exit_time -lt $exit_timeout ]]
    do
        check_liveness || {
            [[ $? -eq 1 ]] && exit_result="exited normally" || exit_result="became zombie"
            break
        }
        sleep 1
    done
    # force kill with SIGKILL if timed out
    [[ $exit_time -ge $exit_timeout ]] && kill -9 ilogtail_pid || :
    echo stop ilogtail done, result ${exit_result}.
}

exit_handler() {
    echo 'receive stop signal'
    exit_flag=1
}


# main procedure
if [ $# -gt 0 ];then
    delaySec=$1
fi
echo 'delay stop seconds: ' $delaySec

trap 'exit_handler' SIGTERM
trap 'exit_handler' SIGINT

gen_config

start_ilogtail

while [[ $exit_flag -eq 0 ]]
do
    sleep 1
    check_liveness || {
        echo "ilogtail exited unexpectedly"
        exit 1
    }
done

stop_ilogtail
cd "$caller_dir"
