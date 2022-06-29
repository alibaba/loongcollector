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
echo "====================================" >> $output

# Collect logs.
for dir in `ls -al`
do
	if [ -f $dir/ilogtail.LOG ]; then
		mkdir -p $OUTPUT_DIR/$dir
		cp $dir/ilogtail.LOG $OUTPUT_DIR/$dir/
	fi
done
