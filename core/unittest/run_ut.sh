#!/bin/bash
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


# Input: /path/to/build_dir, such as build-all
buildDir=build-all
if [ "$1" = "" ]; then
	echo "Without setting build dir, use default: '$buildDir'"
else
	buildDir=$1
fi

CURRENT_DIR="$( cd "$(dirname "$0")" ; pwd -P )"
absoluteBuildDir=$CURRENT_DIR/../$buildDir
if [ ! -d "$absoluteBuildDir" ]; then
	echo "Build dir '$absoluteBuildDir' is not existing."
	exit 1
fi
echo "Build dir: $absoluteBuildDir"

CURRENT_TIME=`date +%Y-%m-%d-%H-%M-%S`
OUTPUT_DIR=$CURRENT_DIR/ut.output/$CURRENT_TIME
mkdir -p $OUTPUT_DIR
output=$OUTPUT_DIR/output.log

cd $absoluteBuildDir/unittest
if [ $? -ne 0 ]; then
	echo "Enter '$absoluteBuildDir/unittest' failed"
	exit 1
fi

# Clean up ilogtail.LOG in UTs.
for logFile in `find . -name "ilogtail.LOG"`
do
    rm $logFile
done

# Start UTs.
echo "============== common =============" >> $output
cd common
./common_simple_utils_unittest >> $output 2>&1
./common_util_unittest >> $output 2>&1
cd ..
echo "====================================" >> $output

echo "============== config ==============" >> $output
cd config
./config_match_unittest >> $output 2>&1
./config_updator_unittest >> $output 2>&1
cd ..
echo "====================================" >> $output

echo "============== parser ==============" >> $output
cd parser
./parser_unittest >> $output 2>&1
cd ..
echo "====================================" >> $output

echo "============== polling ==============" >> $output
cd polling
./polling_unittest >> $output 2>&1
cd ..
echo "====================================" >> $output

echo "============== processor ==============" >> $output
cd processor
./processor_filter_unittest >> $output 2>&1
cd ..
echo "====================================" >> $output

echo "============== reader ==============" >> $output
cd reader
if [ ! -d testDataSet ]; then
	cp -r $CURRENT_DIR/reader/testDataSet ./
fi
./reader_unittest >> $output 2>&1
cd ..
echo "====================================" >> $output

echo "============== sender ==============" >> $output
cd sender
./sender_unittest >> $output 2>&1
cd ..

echo "============== profiler ==============" >> $output
cd profiler
./profiler_data_integrity_unittest >> $output 2>&1
cd ..
echo "====================================" >> $output

# eBPF unittest (gtest); requires BUILD_LOGTAIL_UT on Linux. Keep targets in sync with
# core/unittest/ebpf/CMakeLists.txt (add_unittest / add_driver_unittest).
# Use absolute paths: earlier sections may leave the shell cwd outside unittest/ if a cd failed.
UT_DIR_FOR_EBPF="$absoluteBuildDir/unittest"
EBPF_UT_DIR="$UT_DIR_FOR_EBPF/ebpf"
echo "============== ebpf ==============" >> $output
if [ -d "$EBPF_UT_DIR" ]; then
	EBPF_DRIVER_LIB_DIR="$absoluteBuildDir/ebpf/driver"
	COOLBPF_LIB_DIR="$absoluteBuildDir/_thirdparty/coolbpf/src"
	export LD_LIBRARY_PATH="${EBPF_UT_DIR}:${UT_DIR_FOR_EBPF}:${EBPF_DRIVER_LIB_DIR}:${COOLBPF_LIB_DIR}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
	if cd "$EBPF_UT_DIR"; then
		./aggregator_unittest >> $output 2>&1
		./ebpf_adapter_unittest >> $output 2>&1
		./ebpf_server_unittest >> $output 2>&1
		./sampler_unittest >> $output 2>&1
		./converger_unittest >> $output 2>&1
		./table_unittest >> $output 2>&1
		./protocol_parser_unittest >> $output 2>&1
		./common_util_unittest >> $output 2>&1
		./trace_id_benchmark >> $output 2>&1
		./network_observer_event_unittest >> $output 2>&1
		./network_observer_manager_unittest >> $output 2>&1
		./network_observer_config_update_unittest >> $output 2>&1
		./connection_unittest >> $output 2>&1
		./connection_manager_unittest >> $output 2>&1
		./process_cache_unittest >> $output 2>&1
		./process_cache_value_unittest >> $output 2>&1
		./process_cache_manager_unittest >> $output 2>&1
		./process_data_map_unittest >> $output 2>&1
		./process_cleanup_retryable_event_unittest >> $output 2>&1
		./process_clone_retryable_event_unittest >> $output 2>&1
		./process_execve_retryable_event_unittest >> $output 2>&1
		./process_exit_retryable_event_unittest >> $output 2>&1
		./process_sync_retryable_event_unittest >> $output 2>&1
		./file_retryable_event_unittest >> $output 2>&1
		./file_security_manager_unittest >> $output 2>&1
		./process_security_manager_unittest >> $output 2>&1
		./network_security_manager_unittest >> $output 2>&1
		./agentsight_manager_unittest >> $output 2>&1
		./retryable_event_unittest >> $output 2>&1
		./http_retryable_event_unittest >> $output 2>&1
		./id_allocator_unittest >> $output 2>&1
		./ebpf_driver_log_unittest >> $output 2>&1
		./ebpf_driver_unittest >> $output 2>&1
		./bpf_map_traits_unittest >> $output 2>&1
		./bpf_wrapper_unittest >> $output 2>&1
		cd "$UT_DIR_FOR_EBPF" || {
			echo "cd back to unittest from ebpf failed" >> $output
		}
	else
		echo "cd to ebpf unittest failed" >> $output
	fi
else
	echo "ebpf unittest dir not found ($EBPF_UT_DIR), skip (Linux + BUILD_LOGTAIL_UT for ebpf UTs)" >> $output
fi
echo "====================================" >> $output
echo "====================================" >> $output

# Collect logs.
for dir in `ls -al`
do
	if [ -f $dir/ilogtail.LOG ]; then
		mkdir -p $OUTPUT_DIR/$dir
		cp $dir/ilogtail.LOG $OUTPUT_DIR/$dir/
	fi
done
