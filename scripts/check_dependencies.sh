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

# initialize variables
OUT_DIR=${1:-output}
ROOTDIR=$(cd $(dirname "${BASH_SOURCE[0]}") && cd .. && pwd)
# BIN="${BIN_TO_CHECK:-${ROOTDIR}/${OUT_DIR}/loongcollector}"
BIN=/workspaces/ilogtail-debug/debug/loongcollector

# Define allowed dynamic libraries
# These are the only libraries that should be linked to the binary
ALLOWED_LIBRARIES=(
    "linux-vdso.so.1"
    "libpthread.so.0"
    "libdl.so.2"
    "libuuid.so.1"
    "librt.so.1"
    "libm.so.6"
    "libc.so.6"
    "/lib64/ld-linux-x86-64.so.2"
)

echo "Checking dynamic library dependencies..."
echo "Allowed libraries: ${ALLOWED_LIBRARIES[*]}"
echo ""

# Function to check if a library is in the allowed list
is_library_allowed() {
    local lib_name="$1"
    
    # Extract the library name from the ldd output
    # Handle different formats: 
    # - "linux-vdso.so.1 (0x...)"
    # - "libpthread.so.0 => /lib64/libpthread.so.0 (0x...)"
    # - "/lib64/ld-linux-x86-64.so.2 (0x...)"
    
    for allowed in "${ALLOWED_LIBRARIES[@]}"; do
        if [[ "$lib_name" == "$allowed"* ]] || [[ "$lib_name" == *"$allowed"* ]]; then
            return 0
        fi
    done
    return 1
}

# Function to check a single binary file
check_binary_dependencies() {
    local obj="$1"
    local failed=0
    
    echo "Checking dependencies in $obj ..."
    
    # Use ldd to get dynamic library dependencies
    if ! ldd_output=$(ldd "$obj" 2>&1); then
        echo -e "\033[0;31mError: Failed to run ldd on $obj\033[0m"
        echo "$ldd_output"
        return 1
    fi
    
    # Parse ldd output and check each library
    local unwanted_libs=()
    
    while IFS= read -r line; do
        # Skip empty lines
        [[ -z "$line" ]] && continue
        
        # Extract library name from different ldd output formats
        if [[ "$line" =~ ^[[:space:]]*([^[:space:]]+)[[:space:]]*\( ]]; then
            # Format: "linux-vdso.so.1 (0x...)"
            lib_name="${BASH_REMATCH[1]}"
        elif [[ "$line" =~ ^[[:space:]]*([^[:space:]]+)[[:space:]]*=\>[[:space:]]*([^[:space:]]+)[[:space:]]*\( ]]; then
            # Format: "libpthread.so.0 => /lib64/libpthread.so.0 (0x...)"
            lib_name="${BASH_REMATCH[1]}"
        elif [[ "$line" =~ ^[[:space:]]*(/[^[:space:]]+)[[:space:]]*\( ]]; then
            # Format: "/lib64/ld-linux-x86-64.so.2 (0x...)"
            lib_name="${BASH_REMATCH[1]}"
        else
            continue
        fi
        
        # Check if this library is allowed
        if ! is_library_allowed "$lib_name"; then
            unwanted_libs+=("$line")
        fi
    done <<< "$ldd_output"
    
    # Report results
    if [[ ${#unwanted_libs[@]} -gt 0 ]]; then
        echo -e "\033[0;31mError: Found unwanted library dependencies in $obj:\033[0m"
        printf '%s\n' "${unwanted_libs[@]}"
        echo ""
        echo -e "\033[0;31mFull ldd output:\033[0m"
        echo "$ldd_output"
        echo ""
        failed=1
    else
        echo -e "\033[0;32m✓ All library dependencies are allowed in $obj\033[0m"
        echo "Dependencies found:"
        echo "$ldd_output" | sed 's/^/  /'
        echo ""
    fi
    
    return $failed
}

# Check all binaries
all=("$BIN")
failed=0

for obj in "${all[@]}"; do
    if [[ -f "$obj" ]]; then
        check_binary_dependencies "$obj" || failed+=1
    else
        echo -e "\033[0;33mWarning: $obj not found, skipping...\033[0m"
    fi
done

# Final result
if [[ $failed -gt 0 ]]; then
    echo -e "\033[0;31mError: Found unwanted library dependencies in $failed binary file(s)\033[0m"
    echo -e "\033[0;31mThese dependencies may cause compatibility issues or security concerns\033[0m"
    echo -e "\033[0;31mConsider static linking or removing unnecessary dependencies\033[0m"
    exit 1
else
    echo -e "\033[0;32mAll dependency checks passed\033[0m"
    echo -e "\033[0;32mOnly allowed library dependencies found\033[0m"
fi 