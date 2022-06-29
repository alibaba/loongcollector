// Copyright 2022 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "CrashBackTraceUtil.h"
#include <cstdlib>
#include <cstdio>
#if defined(__linux__)
#define UNW_LOCAL_ONLY
#include <cxxabi.h>
#include <libunwind/libunwind.h>
#elif defined(_MSC_VER)
#include <breakpad/client/windows/handler/exception_handler.h>
#include <direct.h>
#endif
#include "logger/Logger.h"
#include "RuntimeUtil.h"
#include "Flags.h"

DEFINE_FLAG_STRING(crash_stack_file_name, "crash stack back trace file name", "backtrace.dat");

namespace logtail {

#if defined(__linux__)
bool g_crash_process_flag = false;
FILE* g_crashBackTraceFilePtr = NULL;
#elif defined(_MSC_VER)
std::unique_ptr<google_breakpad::ExceptionHandler> g_handler = nullptr;
#endif

#if defined(__linux__)
void CrashBackTrace(int signum) {
    if (g_crash_process_flag) {
        // prevent recursive crash
        _exit(10);
    }
    g_crash_process_flag = true;
    if (g_crashBackTraceFilePtr == NULL) {
        _exit(10);
    }
    fprintf(g_crashBackTraceFilePtr, "signal : %d \n", signum);
    unw_cursor_t cursor;
    unw_context_t context;

    // Initialize cursor to current frame for local unwinding.
    unw_getcontext(&context);
    unw_init_local(&cursor, &context);

    // Unwind frames one by one, going up the frame stack.
    while (unw_step(&cursor) > 0) {
        unw_word_t offset, pc;
        unw_get_reg(&cursor, UNW_REG_IP, &pc);
        if (pc == 0) {
            break;
        }
        fprintf(g_crashBackTraceFilePtr, "0x%lx: ", pc);

        char sym[256];
        memset(sym, 0, sizeof(sym));
        if (unw_get_proc_name(&cursor, sym, sizeof(sym), &offset) == 0) {
            char* nameptr = sym;
            //int status;
            // do not call __cxa_demangle, this will call malloc, and if program crash in malloc(tcmalloc), then it will case dead lock
            //char* demangled = abi::__cxa_demangle(sym, NULL, NULL, &status);
            //if (status == 0) {
            //    nameptr = demangled;
            //}
            fprintf(g_crashBackTraceFilePtr, " (%s+0x%lx)\n", nameptr, offset);
            // no need to call free
            //std::free(demangled);
        }
    }
    fclose(g_crashBackTraceFilePtr);
    _exit(10);
}
#elif defined(_MSC_VER)
bool FilterCallbackFunc(void* context, EXCEPTION_POINTERS* exinfo, MDRawAssertionInfo* assertion) {
    printf("FilterCallbackFunc is called\n");
    return true;
}

// Rename '@dump_path@minidump_id.dmp' to STRING_FLAG(crash_stack_file_name).
bool MinidumpCallbackFunc(const wchar_t* dump_path,
                          const wchar_t* minidump_id,
                          void* context,
                          EXCEPTION_POINTERS* exinfo,
                          MDRawAssertionInfo* assertion,
                          bool succeeded) {
    printf("MinidumpCallbackFunc is called\n");
    auto trgFilePath = GetProcessExecutionDir() + STRING_FLAG(crash_stack_file_name);
    if (0 == _access(trgFilePath.c_str(), 0)) {
        if (remove(trgFilePath.c_str()) != 0) {
            printf("Remove existing target file %s failed: %d", trgFilePath.c_str(), errno);
            return false;
        }
    } else if (errno != ENOENT) {
        printf("Access target file %s failed: %d", trgFilePath.c_str(), errno);
        return false;
    }

    auto srcFilePathW = std::wstring(dump_path) + minidump_id + L".dmp";
    std::string srcFilePath(srcFilePathW.begin(), srcFilePathW.end());
    if (rename(srcFilePath.c_str(), trgFilePath.c_str())) {
        printf("Rename %s to %s failed: %d", srcFilePath.c_str(), trgFilePath.c_str(), errno);
        return false;
    }

    return true;
}
#endif

void InitCrashBackTrace() {
#if defined(__linux__)
    g_crashBackTraceFilePtr = fopen((GetProcessExecutionDir() + STRING_FLAG(crash_stack_file_name)).c_str(), "w");
    if (g_crashBackTraceFilePtr == NULL) {
        APSARA_LOG_ERROR(sLogger, ("unable to open stack back trace file", strerror(errno)));
        return;
    }
    signal(SIGSEGV, CrashBackTrace); // SIGSEGV    11       Core Invalid memory reference
    signal(SIGABRT, CrashBackTrace); // SIGABRT     6       Core Abort signal from
#elif defined(_MSC_VER)
    if (g_handler != nullptr)
        return;
    _mkdir("dumps");
    g_handler.reset(new google_breakpad::ExceptionHandler(
        L"dumps\\", FilterCallbackFunc, MinidumpCallbackFunc, NULL, google_breakpad::ExceptionHandler::HANDLER_ALL));
#endif
}

std::string GetCrashBackTrace() {
    auto stackFilePath = GetProcessExecutionDir() + STRING_FLAG(crash_stack_file_name);
    FILE* pStackFile = fopen(stackFilePath.c_str(), "rb");
    if (pStackFile == NULL) {
        return "";
    }

#if defined(_MSC_VER)
    const int MAX_FILE_SIZE = 1024 * 1024;
    // Windows needs to check the dump size.
    struct stat fileStat;
    if (fstat(_fileno(pStackFile), &fileStat) != 0) {
        LOG_WARNING(sLogger, ("fstat failed", stackFilePath)("errno", errno));
        fclose(pStackFile);
        return "";
    }
    if (fileStat.st_size > MAX_FILE_SIZE) {
        LOG_WARNING(sLogger, ("A dump larger than 1MB", fileStat.st_size));
        fclose(pStackFile);
        return "";
    }
    std::vector<char> bufVec(MAX_FILE_SIZE + 1, '\0');
    char* buf = bufVec.data();
#elif defined(__linux__)
    const int MAX_FILE_SIZE = 1024 * 10;
    char buf[MAX_FILE_SIZE + 1];
    memset(buf, 0, MAX_FILE_SIZE + 1);
#endif

    auto len = fread(buf, 1, MAX_FILE_SIZE, pStackFile);
    fclose(pStackFile);
    remove(stackFilePath.c_str());
    return std::string(buf, len);
}

} // namespace logtail
