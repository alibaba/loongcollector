#!/bin/bash
# 基于 time 命令的编译、链接时间分析脚本
# 直接修改 CMake 生成的 Makefile
# 使用方式：
# mkdir -p core/build && cd core/build && cmake .. && cd -
# ./analyze_build_time.sh core/build

set -e

BUILD_DIR="${1:-core/build}"

# 转换为绝对路径（在 cd 之前）
if [ ! -d "${BUILD_DIR}" ]; then
    echo "错误: 构建目录 ${BUILD_DIR} 不存在"
    echo "请先运行: cd core && mkdir -p build && cd build && cmake .."
    exit 1
fi

# 获取 BUILD_DIR 的绝对路径
BUILD_DIR_ABS=$(cd "${BUILD_DIR}" && pwd)
# 获取项目根目录（假设脚本在项目根目录的 scripts 目录下）
PROJECT_ROOT=$(cd "$(dirname "$0")/.." && pwd)
# 添加时间戳到日志目录
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
LOG_DIR="${BUILD_DIR_ABS}/build_time_logs_${TIMESTAMP}"
COMPILE_LOG="${LOG_DIR}/compile_times.log"
LINK_LOG="${LOG_DIR}/link_times.log"
DEBUG_LOG="${LOG_DIR}/debug.log"
BUILD_INFO_LOG="${BUILD_DIR_ABS}/build_info.log"

echo "=========================================="
echo "编译时间分析工具"
echo "=========================================="
echo "构建目录: ${BUILD_DIR_ABS}"
echo ""

cd "${BUILD_DIR_ABS}"

if [ ! -f "Makefile" ]; then
    echo "错误: Makefile 不存在，请先运行 cmake"
    exit 1
fi

# 创建日志目录
mkdir -p "${LOG_DIR}"
rm -f "${COMPILE_LOG}" "${LINK_LOG}" "${DEBUG_LOG}" "${BUILD_INFO_LOG}"

# 注意：我们使用 CMAKE_COMPILER_LAUNCHER，不需要修改 Makefile
# Makefile 是由 CMake 自动生成的，不需要备份

# 2. 获取实际编译器路径
get_compiler() {
    local compiler_type="$1"
    local default="$2"
    local compiler=$(grep "^CMAKE_${compiler_type}_COMPILER:FILEPATH=" CMakeCache.txt 2>/dev/null | head -1 | cut -d= -f2- | tr -d '\n\r' | xargs)
    compiler="${compiler:-${default}}"
    if [ ! -x "${compiler}" ]; then
        echo "警告: 编译器 ${compiler} 不存在或不可执行，使用默认值 ${default}" >&2
        compiler="${default}"
    fi
    echo "${compiler}"
}

REAL_CC=$(get_compiler "C" "gcc")
REAL_CXX=$(get_compiler "CXX" "g++")

echo "检测到的编译器:"
echo "  CC:  ${REAL_CC}"
echo "  CXX: ${REAL_CXX}"
echo ""

# 3. 创建编译器包装脚本
echo "正在创建编译器包装脚本..."

# 设置环境变量供包装脚本使用
export BUILD_TIME_PROJECT_ROOT="${PROJECT_ROOT}"

# 创建包装脚本
WRAPPER_SCRIPT="${BUILD_DIR_ABS}/time_wrapper.sh"
cat > "${WRAPPER_SCRIPT}" << 'WRAPPER_EOF'
#!/bin/bash
# 编译器时间包装脚本
# CMake COMPILER_LAUNCHER 调用方式: launcher compiler_path args...
# 所以第一个参数是编译器路径（由 CMake 自动添加）

REAL_COMPILER="$1"
shift

LOG_DIR="${BUILD_TIME_LOG_DIR}"
PROJECT_ROOT="${BUILD_TIME_PROJECT_ROOT}"
COMPILE_LOG="${LOG_DIR}/compile_times.log"
LINK_LOG="${LOG_DIR}/link_times.log"
DEBUG_LOG="${LOG_DIR}/debug.log"
BUILD_INFO_LOG="${BUILD_TIME_BUILD_INFO_LOG}"

# 判断编译器类型（C 或 C++）
COMPILER_TYPE=""
if [[ "$REAL_COMPILER" == *"c++"* ]] || [[ "$REAL_COMPILER" == *"g++"* ]] || [[ "$REAL_COMPILER" == *"clang++"* ]]; then
    COMPILER_TYPE="CXX"
elif [[ "$REAL_COMPILER" == *"cc"* ]] || [[ "$REAL_COMPILER" == *"gcc"* ]] || [[ "$REAL_COMPILER" == *"clang"* ]]; then
    COMPILER_TYPE="CC"
fi

# 检查是否是 ar 命令（静态库归档）
IS_AR=false
if [[ "$REAL_COMPILER" == *"ar"* ]]; then
    IS_AR=true
fi

# 检查是否是链接命令
# 链接命令的特征：
# 1. 有 -o 选项（输出目标文件）
# 2. 通常没有源文件（.c, .cpp等），但有 .o 文件或其他库文件
# 3. 可能包含 -shared, -r, -pie 等链接选项
# 4. 可能包含 -Wl, 等链接器选项
IS_LINK=false
HAS_SOURCE=false
HAS_O_FLAG=false
HAS_OBJECT_FILES=false
HAS_LINK_FLAGS=false

# 调试：记录所有参数
echo "[DEBUG] Compiler: $REAL_COMPILER" >> "$DEBUG_LOG"
echo "[DEBUG] Args count: $#" >> "$DEBUG_LOG"
echo "[DEBUG] Args: $@" >> "$DEBUG_LOG"

for arg in "$@"; do
    # 检查是否是源文件（排除对象文件，如 .cpp.o）
    # 源文件应该以 .c/.cpp/.cc/.cxx 结尾，但不应该以 .o 结尾
    if [[ "$arg" == *.c ]] && [[ "$arg" != *.o ]]; then
        HAS_SOURCE=true
        break
    elif [[ "$arg" == *.cpp ]] && [[ "$arg" != *.o ]]; then
        HAS_SOURCE=true
        break
    elif [[ "$arg" == *.cc ]] && [[ "$arg" != *.o ]]; then
        HAS_SOURCE=true
        break
    elif [[ "$arg" == *.cxx ]] && [[ "$arg" != *.o ]]; then
        HAS_SOURCE=true
        break
    fi
    if [[ "$arg" == "-o" ]]; then
        HAS_O_FLAG=true
    fi
    if [[ "$arg" == *.o ]] || [[ "$arg" == *.a ]] || [[ "$arg" == *.so ]]; then
        HAS_OBJECT_FILES=true
    fi
    # 检查链接器相关选项
    if [[ "$arg" == -shared ]] || [[ "$arg" == -r ]] || [[ "$arg" == -pie ]] || \
       [[ "$arg" == -Wl,* ]] || [[ "$arg" == -L* ]] || [[ "$arg" == -l* ]]; then
        HAS_LINK_FLAGS=true
    fi
done

# 调试：记录检测结果
echo "[DEBUG] HAS_SOURCE: $HAS_SOURCE, HAS_O_FLAG: $HAS_O_FLAG, HAS_OBJECT_FILES: $HAS_OBJECT_FILES, HAS_LINK_FLAGS: $HAS_LINK_FLAGS, IS_AR: $IS_AR" >> "$DEBUG_LOG"

# 如果是 ar 命令，直接作为链接处理（静态库归档）
if [ "$IS_AR" = true ]; then
    IS_LINK=true
    echo "[DEBUG] Detected as LINK (AR command)" >> "$DEBUG_LOG"
# 如果有 -o 选项且没有源文件，很可能是链接命令
# 或者有对象文件但没有源文件，也可能是链接命令
# 或者有链接器标志但没有源文件，也可能是链接命令
elif [ "$HAS_O_FLAG" = true ] && [ "$HAS_SOURCE" = false ]; then
    IS_LINK=true
    echo "[DEBUG] Detected as LINK (HAS_O_FLAG && !HAS_SOURCE)" >> "$DEBUG_LOG"
elif [ "$HAS_OBJECT_FILES" = true ] && [ "$HAS_SOURCE" = false ]; then
    IS_LINK=true
    echo "[DEBUG] Detected as LINK (HAS_OBJECT_FILES && !HAS_SOURCE)" >> "$DEBUG_LOG"
elif [ "$HAS_LINK_FLAGS" = true ] && [ "$HAS_SOURCE" = false ] && [ "$HAS_O_FLAG" = true ]; then
    IS_LINK=true
    echo "[DEBUG] Detected as LINK (HAS_LINK_FLAGS && !HAS_SOURCE && HAS_O_FLAG)" >> "$DEBUG_LOG"
else
    echo "[DEBUG] Detected as COMPILE" >> "$DEBUG_LOG"
fi

if [ "$IS_LINK" = true ]; then
    # 链接命令
    TARGET=""
    if [ "$IS_AR" = true ]; then
        # ar 命令格式: ar qc libxxx.a file1.o file2.o ...
        # 目标文件通常是第二个参数（在 qc 之后）
        for i in $(seq 1 $#); do
            if [[ "${!i}" == *.a ]]; then
                TARGET=$(basename "${!i}")
                break
            fi
        done
    else
        # 编译器链接命令
        for i in $(seq 1 $#); do
            if [ "${!i}" = "-o" ] && [ $((i+1)) -le $# ]; then
                TARGET=$(basename "${@:$((i+1)):1}")
                break
            fi
        done
        [ -z "$TARGET" ] && [ $# -gt 0 ] && TARGET=$(basename "${@: -1}")
    fi
    
    echo "[DEBUG] LINK command detected, TARGET: $TARGET" >> "$DEBUG_LOG"
    
    if [ -n "$TARGET" ]; then
        # 将 time 的输出和链接器的输出分开处理
        TIME_OUTPUT=$(mktemp)
        LINKER_OUTPUT=$(mktemp)
        
        # 执行链接命令，将链接器输出保存到临时文件
        /usr/bin/time -f "LD ${TARGET}: %E" -o "$TIME_OUTPUT" "$REAL_COMPILER" "$@" > "$LINKER_OUTPUT" 2>&1
        EXIT_CODE=$?
        
        # 过滤掉time命令的输出并显示/保存链接器输出（同时过滤空行）
        if [ -f "$LINKER_OUTPUT" ] && [ -s "$LINKER_OUTPUT" ]; then
            grep -v "^LD .*: [0-9]*:[0-9][0-9]\.[0-9][0-9]$" "$LINKER_OUTPUT" | grep -v '^$' | tee -a "$BUILD_INFO_LOG" >&2 || true
        fi
        
        # 将时间信息追加到链接日志
        if [ -f "$TIME_OUTPUT" ] && [ -s "$TIME_OUTPUT" ]; then
            TIME_MSG=$(cat "$TIME_OUTPUT")
            # 使用文件锁原子写入日志文件
            (
                flock -x 200
                echo "$TIME_MSG" >> "$LINK_LOG"
            ) 200>"${LINK_LOG}.lock"
            echo "[DEBUG] Link time recorded: $TIME_MSG" >> "$DEBUG_LOG"
        else
            echo "[DEBUG] WARNING: TIME_OUTPUT is empty or missing" >> "$DEBUG_LOG"
        fi
        
        rm -f "$TIME_OUTPUT" "$LINKER_OUTPUT"
        exit $EXIT_CODE
    else
        echo "[DEBUG] WARNING: LINK command but TARGET is empty, executing directly" >> "$DEBUG_LOG"
        exec "$REAL_COMPILER" "$@"
    fi
else
    # 编译命令
    SOURCE_FILE=""
    for arg in "$@"; do
        # 检查是否是源文件（排除对象文件，如 .cpp.o）
        # 源文件应该以 .c/.cpp/.cc/.cxx 结尾，但不应该以 .o 结尾
        if [[ "$arg" == *.c ]] && [[ "$arg" != *.o ]]; then
            # 获取相对于项目根目录的路径
            if [ -n "${PROJECT_ROOT}" ] && [[ "$arg" == "${PROJECT_ROOT}"/* ]]; then
                SOURCE_FILE="${arg#${PROJECT_ROOT}/}"
            elif [[ "$arg" == /* ]]; then
                # 提取项目相关目录（core/common/go_pipeline/provider）
                for dir in core common go_pipeline provider; do
                    if [[ "$arg" == *"/${dir}/"* ]]; then
                        SOURCE_FILE="${dir}/${arg#*/*/${dir}/}"
                        break
                    fi
                done
                [ -z "$SOURCE_FILE" ] && SOURCE_FILE=$(basename "$arg")
            else
                SOURCE_FILE="$arg"
            fi
            break
        elif [[ "$arg" == *.cpp ]] && [[ "$arg" != *.o ]]; then
            # 获取相对于项目根目录的路径
            if [ -n "${PROJECT_ROOT}" ] && [[ "$arg" == "${PROJECT_ROOT}"/* ]]; then
                SOURCE_FILE="${arg#${PROJECT_ROOT}/}"
            elif [[ "$arg" == /* ]]; then
                # 提取项目相关目录（core/common/go_pipeline/provider）
                for dir in core common go_pipeline provider; do
                    if [[ "$arg" == *"/${dir}/"* ]]; then
                        SOURCE_FILE="${dir}/${arg#*/*/${dir}/}"
                        break
                    fi
                done
                [ -z "$SOURCE_FILE" ] && SOURCE_FILE=$(basename "$arg")
            else
                SOURCE_FILE="$arg"
            fi
            break
        elif [[ "$arg" == *.cc ]] && [[ "$arg" != *.o ]]; then
            # 获取相对于项目根目录的路径
            if [ -n "${PROJECT_ROOT}" ] && [[ "$arg" == "${PROJECT_ROOT}"/* ]]; then
                SOURCE_FILE="${arg#${PROJECT_ROOT}/}"
            elif [[ "$arg" == /* ]]; then
                # 提取项目相关目录（core/common/go_pipeline/provider）
                for dir in core common go_pipeline provider; do
                    if [[ "$arg" == *"/${dir}/"* ]]; then
                        SOURCE_FILE="${dir}/${arg#*/*/${dir}/}"
                        break
                    fi
                done
                [ -z "$SOURCE_FILE" ] && SOURCE_FILE=$(basename "$arg")
            else
                SOURCE_FILE="$arg"
            fi
            break
        elif [[ "$arg" == *.cxx ]] && [[ "$arg" != *.o ]]; then
            # 获取相对于项目根目录的路径
            if [ -n "${PROJECT_ROOT}" ] && [[ "$arg" == "${PROJECT_ROOT}"/* ]]; then
                SOURCE_FILE="${arg#${PROJECT_ROOT}/}"
            elif [[ "$arg" == /* ]]; then
                # 提取项目相关目录（core/common/go_pipeline/provider）
                for dir in core common go_pipeline provider; do
                    if [[ "$arg" == *"/${dir}/"* ]]; then
                        SOURCE_FILE="${dir}/${arg#*/*/${dir}/}"
                        break
                    fi
                done
                [ -z "$SOURCE_FILE" ] && SOURCE_FILE=$(basename "$arg")
            else
                SOURCE_FILE="$arg"
            fi
            break
        fi
    done
    if [ -n "$SOURCE_FILE" ]; then
        # 执行编译并记录时间，同时显示和保存
        # 将 time 的输出和编译器的输出分开处理
        # time 的输出写到临时文件，编译器的输出正常显示
        TIME_OUTPUT=$(mktemp)
        COMPILER_OUTPUT=$(mktemp)
        
        # 检查并添加 -ftime-report 选项（如果不存在）
        NEW_ARGS=("$@")
        for arg in "$@"; do
            [ "$arg" = "-ftime-report" ] && break
        done || NEW_ARGS+=("-ftime-report")
        
        # 执行编译，捕获编译器的 stderr（包含 -ftime-report 输出）
        # 注意：time 命令测量的时间（如 0:23.72）包括编译器启动、初始化等开销
        # -ftime-report 中的 TOTAL（如 22.06秒）是编译器内部统计的纯编译时间
        # 两者差异主要是编译器启动开销，不是链接时间（这是编译命令，有源文件）
        /usr/bin/time -f "${COMPILER_TYPE} ${SOURCE_FILE}: %E" -o "$TIME_OUTPUT" "$REAL_COMPILER" "${NEW_ARGS[@]}" 2> "$COMPILER_OUTPUT"
        EXIT_CODE=$?
        
        # 分离编译器的输出：
        # 1. 警告和错误信息（原始输出） -> 显示到终端并写入build_info.log
        # 2. -ftime-report 和时间信息 -> 只记录到compile_times.log，不显示到终端和build_info.log
        
        # 提取警告和错误（不包含-ftime-report和时间信息），并显示/保存（同时过滤空行）
        awk '
            BEGIN { in_report=0 }
            /^Time variable/ { in_report=1; next }
            in_report && /^TOTAL/ { in_report=0; next }
            in_report { next }
            /^[[:space:]]*[0-9.]*[[:space:]]+[0-9.]*[[:space:]]+[0-9.]*/ && in_report==0 { 
                if (NF >= 3 && $1 ~ /^[0-9.]+$/ && $2 ~ /^[0-9.]+$/ && $3 ~ /^[0-9.]+$/) next
            }
            /^---$/ || /^[A-Z]{2,3} .+: [0-9]+:[0-9]{2}\.[0-9]{2}$/ || /^$/ { next }
            { print }
        ' "$COMPILER_OUTPUT" 2>/dev/null | tee -a "$BUILD_INFO_LOG" >&2 || true
        
        # 将时间信息和 -ftime-report 输出作为一个整体原子写入，避免并发时错位
        if [ -f "$TIME_OUTPUT" ] && [ -s "$TIME_OUTPUT" ]; then
            TIME_MSG=$(cat "$TIME_OUTPUT")
            # 创建临时文件收集所有输出
            LOG_ENTRY=$(mktemp)
            echo "$TIME_MSG" > "$LOG_ENTRY"
            # 提取 -ftime-report 的输出并追加到临时文件
            # -ftime-report 的输出格式：以 "Time variable" 开头，以 "TOTAL" 行结束
            if grep -q "Time variable" "$COMPILER_OUTPUT" 2>/dev/null; then
                # 提取从 "Time variable" 开始到 "TOTAL" 行结束的完整报告
                awk '
                    /^Time variable/ { 
                        in_report=1
                    }
                    in_report {
                        print
                        if (/^TOTAL/) {
                            in_report=0
                        }
                    }
                ' "$COMPILER_OUTPUT" >> "$LOG_ENTRY" 2>/dev/null || true
                # 添加分隔行，便于区分不同文件的报告
                echo "---" >> "$LOG_ENTRY"
            fi
            # 使用文件锁原子写入日志文件，避免并发时错位
            (
                flock -x 200
                cat "$LOG_ENTRY" >> "$COMPILE_LOG"
            ) 200>"${COMPILE_LOG}.lock"
            rm -f "$LOG_ENTRY"
            # 时间信息不再显示到终端（已写入build_info.log）
        fi
        
        rm -f "$TIME_OUTPUT" "$COMPILER_OUTPUT"
        exit $EXIT_CODE
    else
        # 没有找到源文件，直接执行（可能是其他工具调用）
        exec "$REAL_COMPILER" "$@"
    fi
fi
WRAPPER_EOF

chmod +x "${WRAPPER_SCRIPT}"

# 4. 修改 CMakeCache.txt 使用编译器启动器（COMPILER_LAUNCHER）
echo "正在修改 CMakeCache.txt..."

# 备份 CMakeCache.txt
if [ ! -f "CMakeCache.txt.bak" ]; then
    cp CMakeCache.txt CMakeCache.txt.bak
fi

# 使用 CMake 的 COMPILER_LAUNCHER 功能（这是推荐的方式）
# COMPILER_LAUNCHER 只需要指向脚本路径，CMake 会自动在后面添加编译器路径
# 转义路径中的特殊字符，避免 sed 命令出错
WRAPPER_SCRIPT_ESCAPED=$(printf '%s\n' "${WRAPPER_SCRIPT}" | sed 's/[[\.*^$()+?{|]/\\&/g')

# 更新或添加 CMake launcher 配置
update_cmake_launcher() {
    local var_name="$1"
    if grep -q "^${var_name}:" CMakeCache.txt; then
        sed -i "s|^${var_name}:.*|${var_name}:STRING=${WRAPPER_SCRIPT_ESCAPED}|" CMakeCache.txt
    else
        echo "${var_name}:STRING=${WRAPPER_SCRIPT}" >> CMakeCache.txt
    fi
}

update_cmake_launcher "CMAKE_C_COMPILER_LAUNCHER"
update_cmake_launcher "CMAKE_CXX_COMPILER_LAUNCHER"
update_cmake_launcher "CMAKE_LINKER_LAUNCHER"

# 设置环境变量供包装脚本使用
export BUILD_TIME_LOG_DIR="${LOG_DIR}"
export BUILD_TIME_PROJECT_ROOT="${PROJECT_ROOT}"
export BUILD_TIME_BUILD_INFO_LOG="${BUILD_INFO_LOG}"

echo "✓ CMakeCache.txt 已修改（使用 COMPILER_LAUNCHER）"
echo "✓ 编译器包装脚本已创建: ${WRAPPER_SCRIPT}"
echo ""

# 5. 重新运行 cmake 以应用新的编译器设置
echo "正在重新运行 cmake 以应用编译器包装..."
cmake . >/dev/null 2>&1 || {
    echo "警告: cmake 重新配置失败，尝试继续编译..."
}

# 5.1. 修改所有 link.txt 文件，在链接命令前添加 launcher
# 因为 cmake_link_script 不会自动使用 CMAKE_CXX_COMPILER_LAUNCHER
echo "正在修改 link.txt 文件以添加链接器包装..."
find "${BUILD_DIR_ABS}" -name "link.txt" -type f | while read link_file; do
    # 读取第一行
    first_line=$(head -1 "$link_file")
    # 检查是否是编译器命令（c++ 或 cc）或归档工具（ar），并且还没有 launcher
    if echo "$first_line" | grep -qE "^/.*/(c\+\+|g\+\+|clang\+\+|cc|gcc|clang|ar)\s" && \
       ! echo "$first_line" | grep -qF "${WRAPPER_SCRIPT}"; then
        # 提取工具路径（第一个参数）
        tool_path=$(echo "$first_line" | awk '{print $1}')
        # 在工具路径前添加 launcher
        sed -i "1s|^${tool_path}|${WRAPPER_SCRIPT} ${tool_path}|" "$link_file"
    fi
done
echo "✓ link.txt 文件已修改"

# 6. 清理并编译
echo "=========================================="
echo "开始编译（带时间统计）..."
echo "=========================================="
echo ""

# 记录脚本开始时间
SCRIPT_START_TIME=$(date +%s)

make clean 2>/dev/null || true
# 过滤掉空行
# 设置最大并发数（可以从环境变量获取，默认64）
MAX_JOBS=${MAX_JOBS:-64}
make -j${MAX_JOBS} 2>&1 | grep -v '^$'

# 记录脚本结束时间并计算总时间
SCRIPT_END_TIME=$(date +%s)
SCRIPT_TOTAL_SEC=$((SCRIPT_END_TIME - SCRIPT_START_TIME))
SCRIPT_TOTAL_MIN=$((SCRIPT_TOTAL_SEC / 60))
SCRIPT_TOTAL_SEC_REMAINDER=$((SCRIPT_TOTAL_SEC % 60))

echo ""
echo "=========================================="
echo "编译完成！"
echo "=========================================="
echo ""

# 创建统计信息日志文件
SUMMARY_LOG="${LOG_DIR}/summary.log"
rm -f "${SUMMARY_LOG}"

# 5. 分析结果
# 使用 tee 将统计信息同时输出到终端和文件
{
if [ -f "${COMPILE_LOG}" ] && [ -s "${COMPILE_LOG}" ]; then
    # 创建过滤后的日志用于排序（不覆盖原文件）
    FILTERED_LOG=$(mktemp)
    grep -E "^(CC|CXX) .+: [0-9]+:[0-9]{2}\.[0-9]{2}$" "${COMPILE_LOG}" > "${FILTERED_LOG}" 2>/dev/null || true
    
    echo "=== 编译时间统计 ==="
    if [ -s "${FILTERED_LOG}" ]; then
        TOTAL=$(wc -l < "${FILTERED_LOG}")
        echo "总编译文件数: ${TOTAL}"
        echo ""
        
        echo "最慢的前10个文件:"
        # 解析时间并排序
        SORTED_OUTPUT=$(awk -F': ' '
            function parse_time(time_str) {
                split(time_str, parts, ":");
                if (length(parts) == 3) {
                    split(parts[3], sec_parts, ".");
                    return parts[1]*3600 + parts[2]*60 + sec_parts[1] + (sec_parts[2]/100);
                } else if (length(parts) == 2) {
                    split(parts[2], sec_parts, ".");
                    return parts[1]*60 + sec_parts[1] + (sec_parts[2]/100);
                }
                return time_str;
            }
            {
                prefix_and_file = $1;
                time = $2;
                if (prefix_and_file ~ /^CC /) {
                    prefix = "CC";
                    file = substr(prefix_and_file, 4);
                } else if (prefix_and_file ~ /^CXX /) {
                    prefix = "CXX";
                    file = substr(prefix_and_file, 5);
                } else {
                    prefix = prefix_and_file;
                    file = "";
                }
                sec = parse_time(time);
                printf "%012.3f %s %s: %s\n", sec, prefix, file, time;
            }
        ' "${FILTERED_LOG}" | sort -rn)
        
        # 显示前10个
        # 排序后的格式：秒数 前缀 文件名: 时间
        # 例如：000054.800 CXX core/unittest/pipeline/PipelineUpdateUnittest.cpp: 0:54.80
        # 显示时应该显示：CXX core/unittest/pipeline/PipelineUpdateUnittest.cpp: 0:54.80
        echo "$SORTED_OUTPUT" | head -10 | \
        awk '{
            # 从第2个字段开始到结尾，重新组合（去掉排序用的秒数）
            result = ""
            for (i=2; i<=NF; i++) {
                if (i > 2) result = result " "
                result = result $i
            }
            printf "  %-60s\n", result
        }'
        
        # 保存排序后的完整结果到文件（包含-ftime-report信息）
        SORTED_LOG="${LOG_DIR}/compile_times_sorted.log"
        # 读取原始日志，按排序后的顺序重新组织，保留-ftime-report信息
        > "${SORTED_LOG}"  # 清空文件
        echo "$SORTED_OUTPUT" | while IFS= read -r line; do
            # 从排序行中提取信息：格式为 "000012.345 CC filename: 0:00.12"
            # $2 是前缀（CC或CXX），$3 是文件名（可能包含冒号），$4 是时间
            prefix=$(echo "$line" | awk '{print $2}')
            # 文件名可能包含空格，需要从 $3 开始到最后一个冒号之前
            file_and_time=$(echo "$line" | awk '{for(i=3;i<=NF;i++) printf "%s ", $i; print ""}' | sed 's/ $//')
            file_name=$(echo "$file_and_time" | awk -F': ' '{print $1}')
            time_str=$(echo "$file_and_time" | awk -F': ' '{print $2}')
            
            # 在原始日志中查找对应的完整记录（包括-ftime-report）
            # 查找以 "prefix file_name: time_str" 开头的记录块
            awk -v prefix="$prefix" -v file_name="$file_name" -v time_str="$time_str" '
                BEGIN { found=0; in_block=0 }
                /^(CC|CXX) / {
                    # 检查是否匹配：前缀 + 空格 + 文件名 + ": " + 时间
                    pattern = "^" prefix " " file_name ": " time_str "$"
                    if ($0 ~ pattern) {
                        found=1
                        in_block=1
                        print
                        next
                    }
                }
                found && in_block {
                    print
                    if (/^---$/) {
                        in_block=0
                        found=0
                    }
                }
            ' "${COMPILE_LOG}" >> "${SORTED_LOG}"
        done
        
        echo ""
        echo "排序后的完整结果已保存到: ${SORTED_LOG}"
    else
        echo "警告: 未找到有效的编译时间记录"
    fi
    
    rm -f "${FILTERED_LOG}"
    
    echo ""
    
    # 计算平均时间和总时间（使用相同的时间解析逻辑）
    awk -F': ' '
        function parse_time(time_str) {
            split(time_str, parts, ":");
            if (length(parts) == 3) {
                split(parts[3], sec_parts, ".");
                return parts[1]*3600 + parts[2]*60 + sec_parts[1] + (sec_parts[2]/100);
            } else if (length(parts) == 2) {
                split(parts[2], sec_parts, ".");
                return parts[1]*60 + sec_parts[1] + (sec_parts[2]/100);
            }
            return time_str;
        }
        {
            sec = parse_time($2);
            sum += sec;
            count++;
        }
        END {
            if (count > 0) {
                printf "平均编译时间: %.2f 秒\n", sum/count;
            }
        }
    ' "${COMPILE_LOG}"
    
    # 计算总编译时间
    TOTAL_COMPILE_SEC=$(awk -F': ' '
        function parse_time(time_str) {
            split(time_str, parts, ":");
            if (length(parts) == 3) {
                split(parts[3], sec_parts, ".");
                return parts[1]*3600 + parts[2]*60 + sec_parts[1] + (sec_parts[2]/100);
            } else if (length(parts) == 2) {
                split(parts[2], sec_parts, ".");
                return parts[1]*60 + sec_parts[1] + (sec_parts[2]/100);
            }
            return time_str;
        }
        {
            sec = parse_time($2);
            sum += sec;
        }
        END {
            printf "%.2f", sum;
        }
    ' "${COMPILE_LOG}")
    
    echo "总编译时间: ${TOTAL_COMPILE_SEC} 秒"
    
    # 计算近似并发度
    if [ -n "${SCRIPT_TOTAL_SEC}" ] && [ "${SCRIPT_TOTAL_SEC}" -gt 0 ]; then
        # 计算实际并发度（总编译时间/脚本运行时间）
        CONCURRENCY=$(awk "BEGIN {printf \"%.2f\", ${TOTAL_COMPILE_SEC}/${SCRIPT_TOTAL_SEC}}")
        # 从环境变量或默认值获取最大并发数
        MAX_JOBS=${MAX_JOBS:-64}
        echo "脚本运行总时间: ${SCRIPT_TOTAL_MIN}分${SCRIPT_TOTAL_SEC_REMAINDER}秒 (${SCRIPT_TOTAL_SEC}秒)"
        echo "最大并发数: ${MAX_JOBS} (make -j${MAX_JOBS})"
        echo "实际并发度: ${CONCURRENCY} (总编译时间/脚本运行时间，理想情况下应接近最大并发数)"
    fi
else
    echo "警告: 未找到编译时间日志或日志为空"
    echo "可能原因:"
    echo "  1. 所有文件都是最新的，没有重新编译"
    echo "  2. Makefile 修改未能正确包装编译器"
    echo ""
    echo "建议:"
    echo "  1. 运行 'make clean' 后重新编译"
    echo "  2. 检查 ${COMPILE_LOG} 文件内容"
fi

echo ""

if [ -f "${LINK_LOG}" ] && [ -s "${LINK_LOG}" ]; then
    # 过滤掉非时间信息行
    FILTERED_LINK_LOG=$(mktemp)
    grep -E "^LD .+: [0-9]+:[0-9]{2}\.[0-9]{2}$" "${LINK_LOG}" > "${FILTERED_LINK_LOG}" 2>/dev/null || true
    [ -s "${FILTERED_LINK_LOG}" ] && mv "${FILTERED_LINK_LOG}" "${LINK_LOG}" || rm -f "${FILTERED_LINK_LOG}"
    
    echo "=== 链接时间统计 ==="
    if [ -s "${LINK_LOG}" ]; then
        # 计算总链接时间
        TOTAL_LINK_SEC=$(awk -F': ' '
            function parse_time(time_str) {
                split(time_str, parts, ":");
                if (length(parts) == 3) {
                    split(parts[3], sec_parts, ".");
                    return parts[1]*3600 + parts[2]*60 + sec_parts[1] + (sec_parts[2]/100);
                } else if (length(parts) == 2) {
                    split(parts[2], sec_parts, ".");
                    return parts[1]*60 + sec_parts[1] + (sec_parts[2]/100);
                }
                return 0;
            }
            {
                sec = parse_time($2);
                sum += sec;
                count++;
            }
            END {
                if (count > 0) {
                    printf "%.2f", sum;
                } else {
                    printf "0.00";
                }
            }
        ' "${LINK_LOG}")
        
        TOTAL_LINK_MIN=$(( ${TOTAL_LINK_SEC%.*} / 60 ))
        TOTAL_LINK_SEC_REMAINDER=$(( ${TOTAL_LINK_SEC%.*} % 60 ))
        echo "总链接时间: ${TOTAL_LINK_MIN}分${TOTAL_LINK_SEC_REMAINDER}秒 (${TOTAL_LINK_SEC}秒)"
        echo ""
        echo "最慢的前10个链接:"
        
        # 保存排序后的链接时间
        LINK_SORTED_LOG="${LOG_DIR}/link_times_sorted.log"
        SORTED_LINK_OUTPUT=$(awk -F': ' '
            function parse_time(time_str) {
                split(time_str, parts, ":");
                if (length(parts) == 3) {
                    split(parts[3], sec_parts, ".");
                    return parts[1]*3600 + parts[2]*60 + sec_parts[1] + (sec_parts[2]/100);
                } else if (length(parts) == 2) {
                    split(parts[2], sec_parts, ".");
                    return parts[1]*60 + sec_parts[1] + (sec_parts[2]/100);
                }
                return time_str;
            }
            {
                sec = parse_time($2);
                printf "%012.3f %s: %s\n", sec, $1, $2;
            }
        ' "${LINK_LOG}" | sort -rn)
        
        # 显示前10个
        echo "$SORTED_LINK_OUTPUT" | head -10 | awk '{
            # 排序后的格式: "000097.890 LD target: 1:37.89"
            # 需要提取: "LD target: 1:37.89"
            # 跳过前13个字符（秒数 + 空格），保留剩余部分
            rest = substr($0, 14);
            printf "  %-60s\n", rest;
        }'
        
        # 保存完整排序结果到文件
        echo "$SORTED_LINK_OUTPUT" | awk '{
            rest = substr($0, 14);
            print rest;
        }' > "${LINK_SORTED_LOG}"
        echo ""
        echo "排序后的完整链接时间已保存到: ${LINK_SORTED_LOG}"
    else
        echo "警告: 链接时间日志为空"
    fi
else
    echo "警告: 未找到链接时间日志或日志为空"
fi

echo ""
echo "详细日志保存在: ${LOG_DIR}/"
echo "  - 统计信息摘要: ${SUMMARY_LOG}"
echo "  - 编译时间（原始）: ${COMPILE_LOG}"
echo "  - 编译时间（排序）: ${LOG_DIR}/compile_times_sorted.log"
if [ -f "${LINK_LOG}" ] && [ -s "${LINK_LOG}" ]; then
    echo "  - 链接时间: ${LINK_LOG}"
    if [ -f "${LOG_DIR}/link_times_sorted.log" ]; then
        echo "  - 链接时间（排序）: ${LOG_DIR}/link_times_sorted.log"
    fi
else
    echo "  - 链接时间: 未找到（可能没有链接操作或链接检测失败）"
    echo "  - 调试日志: ${DEBUG_LOG}（可查看链接检测详情）"
fi
echo "  - 编译详情（警告、错误）: ${BUILD_INFO_LOG}"
echo "  - 调试日志: ${DEBUG_LOG}"
echo ""
echo "提示: 要恢复原始 CMakeCache.txt，运行:"
echo "  cp ${BUILD_DIR}/CMakeCache.txt.bak ${BUILD_DIR}/CMakeCache.txt"
echo "  然后重新运行: cd ${BUILD_DIR} && cmake -DBUILD_LOGTAIL_UT=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON .."
} | tee "${SUMMARY_LOG}"

