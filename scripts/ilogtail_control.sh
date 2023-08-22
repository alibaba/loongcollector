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

# If you want to investigate in the container, you may set env
# LIVENESS_CHECK_INTERVAL=3600 to keep it running when ilogtail exits.

set -ue
set -o pipefail

caller_dir="$PWD"
ilogtail_dir=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)
bin_file="$ilogtail_dir/ilogtail"
pid_file="$ilogtail_dir/ilogtail.pid"
kill_timeout=10
port=${HTTP_PROBE_PORT:-7953}
port_initial_delay_sec=${PORT_INITIAL_DELAY_SEC:-3}
liveness_check_interval=${LIVENESS_CHECK_INTERVAL:-3}
port_retry_count=${PORT_RETRY_COUNT:-3}
port_retry_interval=${PORT_RETRY_INTERVAL:-1}
exit_flag=0

load_pid() {
    [[ -f "$pid_file" ]] && cat "$pid_file" || :
}

save_pid() {
    echo -n $1 > "$pid_file"
}

remove_pid() {
    rm "$pid_file"
}

gen_config() {
    :
}

start_ilogtail() {
    check_liveness_by_pid && {
        local ilogtail_pid=$(load_pid)
        echo "ilogtail already started. pid: $ilogtail_pid"
    } || {
        ($bin_file $@) &
        local ilogtail_pid=$!
        save_pid $ilogtail_pid
        echo "ilogtail started. pid: $ilogtail_pid"
    }
}

check_liveness_by_pid() {
    # check if process exists
    local ilogtail_pid=$(load_pid)
    [[ ! -z $ilogtail_pid && -d /proc/$ilogtail_pid ]] || {
        return 1
    }
    pid_status=`head /proc/$ilogtail_pid/status | grep "State:*"`
    # check if process is zombie
    [[ "$pid_status" =~ .*"zombie"*. ]] && \
        return 2 || :
    # ilogtail is healthy
    return 0
}

check_liveness_by_port() {
    # check if port is open
    for ((i=0; $i<$port_retry_count; i+=1)); do
        [[ $exit_flag -eq 0 ]] || return 0  # exit on signal
        curl localhost:$port &>/dev/null && return 0 || sleep $port_retry_interval
    done
    return 1
}

block_on_check_liveness_by_pid() {
    while [[ $exit_flag -eq 0 ]]
    do
        check_liveness_by_pid || {
            echo "ilogtail exited unexpectedly"
            exit 1
        }
        sleep $liveness_check_interval
    done
}

block_on_check_liveness_by_port() {
    sleep $port_initial_delay_sec
    while [[ $exit_flag -eq 0 ]]
    do
        check_liveness_by_port || {
            echo "ilogtail plugin exited unexpectedly"
            exit 1
        }
        sleep $liveness_check_interval
    done
}

stop_ilogtail() {
    # just return if ilogtail has not started
    local ilogtail_pid=$(load_pid)
    [[ ! -z $ilogtail_pid ]] || {
        echo "ilogtail not started"
        return
    }

    local delaySec=${1:-0}
    echo 'delay stop ilogtail, sleep' $delaySec
    sleep $delaySec
    echo "stop ilogtail"
    # try to kill with SIGTERM, and wait for process to exit
    kill $ilogtail_pid
    local exit_time=0
    local exit_result="force killed"
    while [[ $exit_time -lt $kill_timeout ]]
    do
        check_liveness_by_pid || {
            [[ $? -eq 1 ]] && exit_result="exited normally" || exit_result="became zombie"
            break
        }
        sleep 1
    done
    # force kill with SIGKILL if timed out
    [[ $exit_time -ge $kill_timeout ]] && kill -9 $ilogtail_pid || :
    echo "stop ilogtail done, result: ${exit_result}"
    remove_pid
}

usage() {
    echo "Usage: $0 command [ stop_delay_sec ]"
    echo "command:         start | stop | restart | status | -h"
    echo "stop_delay_sec:  time to wait before stop"
}

exit_handler() {
    echo 'receive stop signal'
    exit_flag=1
}

# main procedure
if [ $# -lt 1 ];then
    usage
    exit
fi

command="$1"
args=""
if [ $# -gt 1 ];then
    shift
    args="$@"
fi

trap 'exit_handler' SIGTERM
trap 'exit_handler' SIGINT

case "$command" in
    start)
    start_ilogtail $args
    ;;
    stop)
    stop_ilogtail $args
    ;;
    restart)
    stop_ilogtail $args
    start_ilogtail $args
    ;;
    status)
    check_liveness_by_pid && exit 0 || exit $?
    ;;
    plugin_status)
    check_liveness_by_port && exit 0 || exit $?
    ;;
    start_and_block)
    start_ilogtail $args
    block_on_check_liveness_by_pid
    stop_ilogtail $args
    ;;
    -h)
    usage
    ;;
    *)
    usage
    ;;
esac