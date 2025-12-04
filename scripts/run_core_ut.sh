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

# Blacklist: directory names to skip
# Example: "test_dir"
BLACKLIST_DIRS=(
    # "pipeline"
    # "host_monitor"
)

# Get CPU core count for parallel execution
MAX_JOBS=${MAX_JOBS:-$(nproc)}
if [ -z "$MAX_JOBS" ] || [ "$MAX_JOBS" -lt 1 ]; then
    MAX_JOBS=1
fi

# Timeout for directory tests (20 minutes = 1200 seconds)
DIRECTORY_TIMEOUT=${DIRECTORY_TIMEOUT:-1200}

# Global variables
declare -A DIR_TESTS  # Directory -> space-separated test files
declare -a DIR_ORDER # Order of directories
declare -A DIR_OUTPUT_FILES  # Directory -> output file path
FAILED_TESTS=()
TOTAL_START_TIME=0
TOTAL_END_TIME=0
OUTPUT_LOCK_FILE=""

# Check if a directory is in the blacklist
is_blacklisted() {
    local test_dir="$1"
    local dir_name=$(basename "$test_dir")
    
    for blacklist_item in "${BLACKLIST_DIRS[@]}"; do
        # Skip empty entries
        [ -z "$blacklist_item" ] && continue
        
        # Directory name match
        if [[ "$dir_name" == "$blacklist_item" ]]; then
            return 0
        fi
    done
    return 1
}

# Collect all test files grouped by directory
collect_tests() {
    local search_dir="$1"
    for file in "$search_dir"/*; do
        if [ -d "$file" ]; then
            # Recursively handle folder
            collect_tests "$file"
        elif [[ -f "$file" ]]; then
            unittest="${file##*_}"
            if [ "$unittest" == "unittest" ]; then
                full_path=$(realpath "$file")
                test_dir="${full_path%/*}"
                
                # Check if directory is blacklisted
                if is_blacklisted "$test_dir"; then
                    continue
                fi
                
                # Group tests by directory
                if [ -z "${DIR_TESTS[$test_dir]}" ]; then
                    DIR_TESTS["$test_dir"]="$full_path"
                    DIR_ORDER+=("$test_dir")
                else
                    DIR_TESTS["$test_dir"]="${DIR_TESTS[$test_dir]} $full_path"
                fi
            fi
        fi
    done
}

# Calculate duration using awk (more portable than bc)
calc_duration() {
    local start=$1
    local end=$2
    awk "BEGIN {printf \"%.2f\", $end - $start}"
}

# Get relative path of test file (relative to TARGET_ARTIFACT_PATH)
get_test_relative_path() {
    local test_file="$1"
    local artifact_path="$2"
    
    # Get absolute paths
    local test_abs
    if [ -f "$test_file" ] || [ -L "$test_file" ]; then
        test_abs=$(realpath "$test_file" 2>/dev/null || echo "$test_file")
    else
        test_abs="$test_file"
    fi
    
    local artifact_abs
    if [ -d "$artifact_path" ]; then
        artifact_abs=$(realpath "$artifact_path" 2>/dev/null || echo "$artifact_path")
    else
        artifact_abs="$artifact_path"
    fi
    
    # Remove artifact path prefix (with trailing slash)
    if [[ "$test_abs" == "$artifact_abs"/* ]]; then
        echo "${test_abs#$artifact_abs/}"
    elif [[ "$test_abs" == "$artifact_abs" ]]; then
        # If test file is the artifact path itself, return basename
        basename "$test_file"
    else
        # Fallback: try to remove common prefix patterns
        local pattern="*/core/build/unittest/"
        if [[ "$test_abs" == *"$pattern"* ]]; then
            echo "${test_abs#*$pattern}"
        else
            # Last resort: return basename
            basename "$test_file"
        fi
    fi
}

# Run tests in a directory sequentially
run_directory_tests() {
    local test_dir="$1"
    local output_file="$2"
    local stats_file="$3"
    local artifact_path="$4"  # TARGET_ARTIFACT_PATH absolute path
    local tests="${DIR_TESTS[$test_dir]}"
    local dir_name=$(basename "$test_dir")
    local dir_test_count=0
    local start_time=$(date +%s.%N)
    
    {
        echo "[$(date '+%Y-%m-%d %H:%M:%S')] ========== Directory: $dir_name =========="
        echo
    } > "$output_file" 2>&1
    
    for test_file in $tests; do
        local test_name=$(basename "$test_file")
        local test_start=$(date +%s.%N)
        
        {
            echo "[$(date '+%Y-%m-%d %H:%M:%S')] [$dir_name] $test_file Start **********"
        } >> "$output_file" 2>&1
        
        # Output real-time result for test start (outside output redirection)
        (
            flock -x 200
            local rel_path=$(get_test_relative_path "$test_file" "$artifact_path")
            echo "🔄 $rel_path (starting...)" >&1
        ) 200>"$OUTPUT_LOCK_FILE.lock"
        
        cd "$test_dir"
        local test_output=$(mktemp)
        if ! "./$test_name" > "$test_output" 2>&1; then
            local test_end=$(date +%s.%N)
            local test_duration=$(calc_duration "$test_start" "$test_end")
            {
                echo "[$(date '+%Y-%m-%d %H:%M:%S')] [$dir_name] $test_file Failed (${test_duration}s) **********"
                cat "$test_output"
            } >> "$output_file" 2>&1
            cd - > /dev/null
            rm -f "$test_output"
            
            # Use lock to safely append to FAILED_TESTS and output real-time result (outside redirection)
            (
                flock -x 200
                echo "$test_file" >> "$OUTPUT_LOCK_FILE.failed"
                local rel_path=$(get_test_relative_path "$test_file" "$artifact_path")
                echo "❌ $rel_path" >&1
            ) 200>"$OUTPUT_LOCK_FILE.lock"
            
            local end_time=$(date +%s.%N)
            local duration=$(calc_duration "$start_time" "$end_time")
            echo "$test_dir|$start_time|$end_time|$dir_test_count|$duration" > "$stats_file"
            {
                echo "[$(date '+%Y-%m-%d %H:%M:%S')] ========== Directory: $dir_name Completed (${duration}s, $dir_test_count tests) =========="
                echo
            } >> "$output_file" 2>&1
            return 1
        fi
        cd - > /dev/null
        rm -f "$test_output"
        
        local test_end=$(date +%s.%N)
        local test_duration=$(calc_duration "$test_start" "$test_end")
        {
            echo "[$(date '+%Y-%m-%d %H:%M:%S')] [$dir_name] $test_file End (${test_duration}s) ############"
            echo
        } >> "$output_file" 2>&1
        
        # Output real-time result for successful test (outside output redirection)
        (
            flock -x 200
            local rel_path=$(get_test_relative_path "$test_file" "$artifact_path")
            echo "✅ $rel_path" >&1
        ) 200>"$OUTPUT_LOCK_FILE.lock"
        
        ((dir_test_count++))
    done
    
    # Write statistics to file (outside the output redirection)
    local end_time=$(date +%s.%N)
    local duration=$(calc_duration "$start_time" "$end_time")
    {
        echo "[$(date '+%Y-%m-%d %H:%M:%S')] ========== Directory: $dir_name Completed (${duration}s, $dir_test_count tests) =========="
        echo
    } >> "$output_file" 2>&1
    echo "$test_dir|$start_time|$end_time|$dir_test_count|$duration" > "$stats_file"
    
    # Use lock to safely update TESTS_RUN
    (
        flock -x 200
        local current_count=$(cat "$OUTPUT_LOCK_FILE.count" 2>/dev/null || echo "0")
        local new_count=$((current_count + dir_test_count))
        echo "$new_count" > "$OUTPUT_LOCK_FILE.count"
    ) 200>"$OUTPUT_LOCK_FILE.lock"
    
    return 0
}

# Format duration in human readable format
format_duration() {
    local duration=$1
    awk -v d="$duration" '
    BEGIN {
        hours = int(d / 3600)
        minutes = int((d % 3600) / 60)
        seconds = d % 60
        
        if (hours > 0) {
            printf "%dh %dm %.2fs", hours, minutes, seconds
        } else if (minutes > 0) {
            printf "%dm %.2fs", minutes, seconds
        } else {
            printf "%.2fs", seconds
        }
    }'
}

# Main execution with parallel directory processing
main() {
    # Maybe some unittest depend on relative paths, so execute in the unittest directory
    UT_BASE_PATH="$(pwd)/${TARGET_ARTIFACT_PATH:2}"
    export LD_LIBRARY_PATH=${UT_BASE_PATH}:$LD_LIBRARY_PATH
    
    local original_dir=$(pwd)
    cd "$TARGET_ARTIFACT_PATH" || exit 1
    
    # Get absolute path of TARGET_ARTIFACT_PATH for relative path calculation
    local artifact_abs_path=$(pwd)
    
    # Disable job control messages to suppress termination messages
    # This prevents bash from printing "Terminated" messages when background jobs are killed
    set +m 2>/dev/null || true
    
    # Create temporary directory for output files
    local temp_dir=$(mktemp -d)
    OUTPUT_LOCK_FILE="$temp_dir/lock"
    echo "0" > "$OUTPUT_LOCK_FILE.count"
    touch "$OUTPUT_LOCK_FILE.failed"
    touch "$OUTPUT_LOCK_FILE.lock"  # Create lock file before use
    
    # Collect all tests grouped by directory
    echo "Collecting test files..."
    collect_tests .
    
    if [ ${#DIR_ORDER[@]} -eq 0 ]; then
        echo "No test files found!"
        cd "$original_dir"
        rm -rf "$temp_dir"
        exit 1
    fi
    
    # Create output files and stats files for each directory
    declare -A DIR_STATS_FILES
    local dir_index=0
    for test_dir in "${DIR_ORDER[@]}"; do
        # Use directory index as hash to avoid md5sum dependency
        local dir_hash="dir_${dir_index}"
        DIR_OUTPUT_FILES["$test_dir"]="$temp_dir/${dir_hash}.out"
        DIR_STATS_FILES["$test_dir"]="$temp_dir/${dir_hash}.stats"
        ((dir_index++))
    done
    
    echo "Found ${#DIR_ORDER[@]} directories with tests"
    echo "Running tests with max $MAX_JOBS parallel directories"
    echo
    echo "Real-time test results:"
    echo "----------------------"
    
    TOTAL_START_TIME=$(date +%s.%N)
    
    # Run directories in parallel, but tests within each directory sequentially
    local dir_index=0
    local failed=0
    local active_pids=()
    declare -A PID_TO_DIR  # PID -> test_dir
    declare -A PID_START_TIME  # PID -> start_time
    
    while [ $dir_index -lt ${#DIR_ORDER[@]} ] || [ ${#active_pids[@]} -gt 0 ]; do
        # Check for timeouts first
        local new_active_pids=()
        for pid in "${active_pids[@]}"; do
            if kill -0 "$pid" 2>/dev/null; then
                # Process is still running, check for timeout
                local test_dir="${PID_TO_DIR[$pid]}"
                local start_time="${PID_START_TIME[$pid]}"
                if [ -n "$test_dir" ] && [ -n "$start_time" ]; then
                    local current_time=$(date +%s.%N)
                    local elapsed=$(calc_duration "$start_time" "$current_time")
                    if awk -v elapsed="$elapsed" -v timeout="$DIRECTORY_TIMEOUT" 'BEGIN {exit !(elapsed >= timeout)}'; then
                        # Timeout occurred, kill the process and its children
                        # Temporarily redirect stderr to suppress bash termination messages
                        exec 3>&2 2>/dev/null
                        
                        local dir_name=$(basename "$test_dir")
                        echo "⏱️  Directory $dir_name timeout exceeded (${DIRECTORY_TIMEOUT}s), killing process $pid" >&3
                        
                        # Kill the process and all its children
                        # First try to get the process group ID
                        local pgid=$(ps -o pgid= -p "$pid" 2>/dev/null | tr -d ' ')
                        if [ -n "$pgid" ] && [ "$pgid" != "$$" ]; then
                            # Kill the entire process group
                            kill -TERM -"$pgid" 2>/dev/null || kill -TERM "$pid" 2>/dev/null
                        else
                            # Fallback: kill the process and find its children
                            kill -TERM "$pid" 2>/dev/null
                            # Kill all children of this process
                            pkill -TERM -P "$pid" 2>/dev/null || true
                        fi
                        sleep 1
                        # Force kill if still running
                        if kill -0 "$pid" 2>/dev/null; then
                            if [ -n "$pgid" ] && [ "$pgid" != "$$" ]; then
                                kill -KILL -"$pgid" 2>/dev/null || kill -KILL "$pid" 2>/dev/null
                            else
                                kill -KILL "$pid" 2>/dev/null
                                pkill -KILL -P "$pid" 2>/dev/null || true
                            fi
                        fi
                        
                        # Wait for the killed process to clean up (suppress termination messages)
                        wait "$pid" 2>/dev/null || true
                        # Small delay to ensure termination message is suppressed
                        sleep 0.1
                        
                        # Restore stderr
                        exec 2>&3 3>&-
                        
                        # Mark all tests in this directory as failed
                        local output_file="${DIR_OUTPUT_FILES[$test_dir]}"
                        local stats_file="${DIR_STATS_FILES[$test_dir]}"
                        local tests_in_dir="${DIR_TESTS[$test_dir]}"
                        
                        {
                            flock -x 200
                            for test_file in $tests_in_dir; do
                                echo "$test_file" >> "$OUTPUT_LOCK_FILE.failed"
                                local rel_path=$(get_test_relative_path "$test_file" "$artifact_abs_path")
                                echo "❌ $rel_path (timeout)" >&1
                            done
                        } 200>"$OUTPUT_LOCK_FILE.lock"
                        
                        # Write timeout message to output file
                        {
                            echo "[$(date '+%Y-%m-%d %H:%M:%S')] ⚠️  Directory timeout exceeded (${DIRECTORY_TIMEOUT}s)"
                            echo "[$(date '+%Y-%m-%d %H:%M:%S')] ========== Directory: $dir_name Timeout =========="
                            echo
                        } >> "$output_file" 2>&1
                        
                        # Write timeout stats
                        local end_time=$(date +%s.%N)
                        local duration=$(calc_duration "$start_time" "$end_time")
                        echo "$test_dir|$start_time|$end_time|0|$duration|TIMEOUT" > "$stats_file"
                        
                        failed=1
                        # Clean up tracking info
                        unset PID_TO_DIR[$pid]
                        unset PID_START_TIME[$pid]
                        # Don't add to new_active_pids, process is killed
                        continue
                    fi
                fi
                # Process is still running and not timed out
                new_active_pids+=($pid)
            else
                # Process has completed, wait for it to get exit status
                # Temporarily redirect stderr to suppress bash termination messages
                exec 3>&2 2>/dev/null
                wait "$pid" 2>/dev/null
                local wait_status=$?
                exec 2>&3 3>&-
                if [ $wait_status -ne 0 ]; then
                    failed=1
                fi
                # Clean up tracking info
                unset PID_TO_DIR[$pid]
                unset PID_START_TIME[$pid]
                # Don't add to new_active_pids, process has completed
            fi
        done
        active_pids=("${new_active_pids[@]}")
        
        # Start new jobs if we have capacity and more directories
        while [ ${#active_pids[@]} -lt $MAX_JOBS ] && [ $dir_index -lt ${#DIR_ORDER[@]} ]; do
            local test_dir="${DIR_ORDER[$dir_index]}"
            local output_file="${DIR_OUTPUT_FILES[$test_dir]}"
            local stats_file="${DIR_STATS_FILES[$test_dir]}"
            
            (
                # Suppress job control messages
                set +m 2>/dev/null || true
                if ! run_directory_tests "$test_dir" "$output_file" "$stats_file" "$artifact_abs_path"; then
                    exit 1
                fi
            ) 2>/dev/null &
            local pid=$!
            local start_time=$(date +%s.%N)
            active_pids+=($pid)
            PID_TO_DIR[$pid]="$test_dir"
            PID_START_TIME[$pid]="$start_time"
            ((dir_index++))
        done
        
        # Wait for at least one job to complete (compatible with older bash)
        if [ ${#active_pids[@]} -gt 0 ]; then
            # Check which jobs have completed by polling
            local new_active_pids=()
            local old_count=${#active_pids[@]}
            for pid in "${active_pids[@]}"; do
                if kill -0 "$pid" 2>/dev/null; then
                    # Process is still running
                    new_active_pids+=($pid)
                else
                    # Process has completed, wait for it to get exit status
                    wait "$pid" 2>/dev/null
                    local wait_status=$?
                    if [ $wait_status -ne 0 ]; then
                        failed=1
                    fi
                    # Clean up tracking info
                    unset PID_TO_DIR[$pid]
                    unset PID_START_TIME[$pid]
                fi
            done
            active_pids=("${new_active_pids[@]}")
            
            # If no job completed, sleep briefly to avoid busy waiting
            if [ ${#active_pids[@]} -eq $old_count ] && [ ${#active_pids[@]} -gt 0 ]; then
                sleep 0.1
            fi
        fi
    done
    
    # Wait for any remaining jobs to complete
    for pid in "${active_pids[@]}"; do
        # Temporarily redirect stderr to suppress bash termination messages
        exec 3>&2 2>/dev/null
        wait "$pid" 2>/dev/null
        local wait_status=$?
        exec 2>&3 3>&-
        if [ $wait_status -ne 0 ]; then
            failed=1
        fi
        # Clean up tracking info
        unset PID_TO_DIR[$pid]
        unset PID_START_TIME[$pid]
    done
    
    TOTAL_END_TIME=$(date +%s.%N)
    
    # Read updated TESTS_RUN count
    if [ -f "$OUTPUT_LOCK_FILE.count" ]; then
        TESTS_RUN=$(cat "$OUTPUT_LOCK_FILE.count")
    fi
    
    # Read failed tests
    if [ -f "$OUTPUT_LOCK_FILE.failed" ]; then
        while IFS= read -r line; do
            [ -n "$line" ] && FAILED_TESTS+=("$line")
        done < "$OUTPUT_LOCK_FILE.failed"
    fi
    
    cd "$original_dir"
    
    echo
    echo "----------------------"
    
    # Calculate and display statistics
    echo "=========================================="
    echo "Statistics"
    echo "=========================================="
    
    local total_duration=$(calc_duration "$TOTAL_START_TIME" "$TOTAL_END_TIME")
    echo "Total Duration: $(format_duration $total_duration)"
    echo "Total Tests: $TESTS_RUN"
    echo
    
    # Directory statistics - read from stats files
    local max_duration=0
    local longest_dir=""
    echo "Directory Statistics:"
    for test_dir in "${DIR_ORDER[@]}"; do
        local stats_file="${DIR_STATS_FILES[$test_dir]}"
        if [ -f "$stats_file" ]; then
            IFS='|' read -r dir_path start_time end_time test_count dir_duration < "$stats_file"
            local dir_name=$(basename "$test_dir")
            
            # Compare to find longest directory
            if awk -v d1="$dir_duration" -v d2="$max_duration" 'BEGIN {exit !(d1 > d2)}'; then
                max_duration=$dir_duration
                longest_dir="$dir_name"
            fi
            
            printf "  %-40s: %8s (%d tests)\n" "$dir_name" "$(format_duration $dir_duration)" "$test_count"
        fi
    done
    echo
    
    if [ -n "$longest_dir" ] && [ "$max_duration" != "0" ]; then
        echo "Longest Directory: $longest_dir ($(format_duration $max_duration))"
    fi
    echo
    
    # Output failed tests details after statistics
    if [ ${#FAILED_TESTS[@]} -gt 0 ]; then
        echo "=========================================="
        echo "❌ Failed Tests Summary"
        echo "=========================================="
        echo
        for failed_test in "${FAILED_TESTS[@]}"; do
            local rel_path=$(get_test_relative_path "$failed_test" "$artifact_abs_path")
            echo "  ❌ $rel_path"
            echo
            
            # Extract and display failure details from output file
            for test_dir in "${DIR_ORDER[@]}"; do
                local output_file="${DIR_OUTPUT_FILES[$test_dir]}"
                if [ -f "$output_file" ] && grep -q "$failed_test" "$output_file" 2>/dev/null; then
                    local test_name=$(basename "$failed_test")
                    # Extract failure section: find Start line and extract until next test or completion
                    # Use a simpler approach: find the line number and extract from there
                    local start_line=$(grep -n "Start.*$test_name" "$output_file" 2>/dev/null | head -1 | cut -d: -f1)
                    if [ -n "$start_line" ]; then
                        # Extract from start_line until we hit next test start or directory completion
                        awk -v start="$start_line" -v test_name="$test_name" '
                        NR >= start {
                            print
                            # Stop at next test start (but not our own)
                            if (NR > start && /Start.*\*\*\*\*\*\*\*\*\*\*/ && !/Start.*test_name/) {
                                exit
                            }
                            # Stop at directory completion
                            if (/========== Directory.*Completed/) {
                                exit
                            }
                        }
                        ' "$output_file" 2>/dev/null | sed 's/^/    /' || true
                    else
                        # Fallback: show lines containing the test and failure info
                        grep -A 200 "$test_name" "$output_file" 2>/dev/null | \
                        grep -B 5 -A 200 "Failed" | head -100 | sed 's/^/    /' || true
                    fi
                    break
                fi
            done
            echo
        done
    fi
    
    # Report results
    if [ $failed -eq 0 ] && [ ${#FAILED_TESTS[@]} -eq 0 ]; then
        echo "=========================================="
        echo "✅ All $TESTS_RUN tests completed successfully!"
        echo "=========================================="
        rm -rf "$temp_dir"
        exit 0
    else
        echo "=========================================="
        echo "❌ Some tests failed!"
        echo "=========================================="
        rm -rf "$temp_dir"
        exit 1
    fi
}

main