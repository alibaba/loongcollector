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
BIN="${ROOTDIR}/${OUT_DIR}/loongcollector"

# Define unwanted CPU instructions (AVX-512, AVX2, etc.)
# These instructions require newer CPU architectures and may cause compatibility issues
UNWANTED_INSTRUCTIONS=(
    "vpclmulqdq"    # AVX-512 VPCLMULQDQ instruction
    "vpbroadcast"   # AVX-512 broadcast instructions
    "vextracti128"  # AVX2 instruction
    "vinserti128"   # AVX2 instruction
    "vperm2i128"    # AVX2 instruction
    "vpshufb"       # AVX2 instruction
    "vpshufd"       # AVX2 instruction
    "vpshufhw"      # AVX2 instruction
    "vpshuflw"      # AVX2 instruction
    "vpslldq"       # AVX2 instruction
    "vpsrldq"       # AVX2 instruction
    "vpunpckhbw"    # AVX2 instruction
    "vpunpckhwd"    # AVX2 instruction
    "vpunpckhdq"    # AVX2 instruction
    "vpunpckhqdq"   # AVX2 instruction
    "vpunpcklbw"    # AVX2 instruction
    "vpunpcklwd"    # AVX2 instruction
    "vpunpckldq"    # AVX2 instruction
    "vpunpcklqdq"   # AVX2 instruction
    "vpxor"         # AVX2 instruction
    "vpand"         # AVX2 instruction
    "vpor"          # AVX2 instruction
    "vpadd"         # AVX2 instruction
    "vpsub"         # AVX2 instruction
    "vpmul"         # AVX2 instruction
    "vpcmp"         # AVX2 instruction
    "vpmov"         # AVX2 instruction
    "vpsll"         # AVX2 instruction
    "vpsrl"         # AVX2 instruction
    "vpsra"         # AVX2 instruction
    "vblend"        # AVX2 instruction
    "vshuf"         # AVX2 instruction
    "vround"        # AVX2 instruction
    "vrcp"          # AVX2 instruction
    "vrsqrt"        # AVX2 instruction
    "vsqrt"         # AVX2 instruction
    "vfmadd"        # AVX2 FMA instructions
    "vfnmadd"       # AVX2 FMA instructions
    "vfmsub"        # AVX2 FMA instructions
    "vfnmsub"       # AVX2 FMA instructions
    "vdp"           # AVX2 dot product instructions
    "vpmadd"        # AVX2 multiply-add instructions
    "vpsadbw"       # AVX2 instruction
    "vphadd"        # AVX2 instruction
    "vphsub"        # AVX2 instruction
    "vpmaddubsw"    # AVX2 instruction
    "vpmulhrsw"     # AVX2 instruction
    "vpsign"        # AVX2 instruction
    "vpalignr"      # AVX2 instruction
    "vpshuf"        # AVX2 instruction
    "vpshufb"       # AVX2 instruction
    "vpshufd"       # AVX2 instruction
    "vpshufhw"      # AVX2 instruction
    "vpshuflw"      # AVX2 instruction
    "vpslldq"       # AVX2 instruction
    "vpsrldq"       # AVX2 instruction
    "vpunpckhbw"    # AVX2 instruction
    "vpunpckhwd"    # AVX2 instruction
    "vpunpckhdq"    # AVX2 instruction
    "vpunpckhqdq"   # AVX2 instruction
    "vpunpcklbw"    # AVX2 instruction
    "vpunpcklwd"    # AVX2 instruction
    "vpunpckldq"    # AVX2 instruction
    "vpunpcklqdq"   # AVX2 instruction
    "vpxor"         # AVX2 instruction
    "vpand"         # AVX2 instruction
    "vpor"          # AVX2 instruction
    "vpadd"         # AVX2 instruction
    "vpsub"         # AVX2 instruction
    "vpmul"         # AVX2 instruction
    "vpcmp"         # AVX2 instruction
    "vpmov"         # AVX2 instruction
    "vpsll"         # AVX2 instruction
    "vpsrl"         # AVX2 instruction
    "vpsra"         # AVX2 instruction
    "vblend"        # AVX2 instruction
    "vshuf"         # AVX2 instruction
    "vround"        # AVX2 instruction
    "vrcp"          # AVX2 instruction
    "vrsqrt"        # AVX2 instruction
    "vsqrt"         # AVX2 instruction
    "vfmadd"        # AVX2 FMA instructions
    "vfnmadd"       # AVX2 FMA instructions
    "vfmsub"        # AVX2 FMA instructions
    "vfnmsub"       # AVX2 FMA instructions
    "vdp"           # AVX2 dot product instructions
    "vpmadd"        # AVX2 multiply-add instructions
    "vpsadbw"       # AVX2 instruction
    "vphadd"        # AVX2 instruction
    "vphsub"        # AVX2 instruction
    "vpmaddubsw"    # AVX2 instruction
    "vpmulhrsw"     # AVX2 instruction
    "vpsign"        # AVX2 instruction
    "vpalignr"      # AVX2 instruction
)

# Build regex pattern for unwanted instructions
pattern=""
for instr in "${UNWANTED_INSTRUCTIONS[@]}"; do
    if [[ -z "$pattern" ]]; then
        pattern="$instr"
    else
        pattern="$pattern|$instr"
    fi
done

echo "Checking for unwanted CPU instructions (AVX-512, AVX2, etc.)..."
echo "Pattern: $pattern"
echo ""

# Function to check a single binary file
check_binary() {
    local obj="$1"
    local failed=0
    
    echo "Checking instructions in $obj ..."
    
    # Use objdump to disassemble and grep for unwanted instructions
    if objdump -d "$obj" 2>/dev/null | grep -E "$pattern" > /tmp/unwanted_instructions.tmp; then
        echo -e "\033[0;31mError: Found unwanted CPU instructions in $obj:\033[0m"
        cat /tmp/unwanted_instructions.tmp
        echo ""
        failed=1
    else
        echo -e "\033[0;32m✓ No unwanted instructions found in $obj\033[0m"
    fi
    
    rm -f /tmp/unwanted_instructions.tmp
    return $failed
}

# Check all binaries
all=("$BIN")
failed=0

for obj in "${all[@]}"; do
    if [[ -f "$obj" ]]; then
        check_binary "$obj" || failed+=1
    else
        echo -e "\033[0;33mWarning: $obj not found, skipping...\033[0m"
    fi
done

# Final result
if [[ $failed -gt 0 ]]; then
    echo -e "\033[0;31mError: Found unwanted CPU instructions in $failed binary file(s)\033[0m"
    echo -e "\033[0;31mThese instructions may cause compatibility issues on older CPUs\033[0m"
    echo -e "\033[0;31mConsider recompiling with -mno-avx -mno-avx2 -mno-avx512f flags\033[0m"
    exit 1
else
    echo -e "\033[0;32mAll CPU instruction checks passed\033[0m"
    echo -e "\033[0;32mNo unwanted instructions (AVX-512, AVX2) found\033[0m"
fi 

