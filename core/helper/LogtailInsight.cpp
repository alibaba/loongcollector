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

#include "LogtailInsight.h"
#include <stddef.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <json/json.h>
#include "log_pb/sls_logs.pb.h"
#include "common/TimeUtil.h"

#ifdef ENABLE_COMPATIBLE_MODE
extern "C" {
#include <string.h>
asm(".symver memcpy, memcpy@GLIBC_2.2.5");
void* __wrap_memcpy(void* dest, const void* src, size_t n) {
    return memcpy(dest, src, n);
}
}
#endif

namespace logtail {

LogtailInsight* LogtailInsight::s_instance = NULL;

struct CmdHeader {
    int32_t type;
    int32_t len;
};

/* Create a client endpoint and connect to a server.   Returns fd if all OK, <0 on error. */
int unix_socket_conn(const char* servername) {
    int fd;
    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) /* create a UNIX domain stream socket */
    {
        cout << "init unix socket error " << strerror(errno) << endl;
        return -1;
    }
    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
        cout << "init unix socket error " << strerror(errno) << endl;
        close(fd);
        return (-1);
    }
    int rval;
    struct sockaddr_un un;
    bzero(&un, sizeof(un));
    un.sun_family = AF_UNIX;
    strcpy(un.sun_path, servername);
    if (connect(fd, (struct sockaddr*)&un, sizeof(un)) < 0) {
        rval = -4;
    } else {
        return fd;
    }
    cout << "server : " << servername << " connect socket address error " << strerror(errno) << endl;
    close(fd);
    return rval;
}

int RecvAndWait(int fd, char* data, int len, int waitSec) {
    int bodySize = 0;
    uint32_t beforeTime = time(NULL);
    while (bodySize < len) {
        int recvLen = recv(fd, data + bodySize, len - bodySize, MSG_DONTWAIT);
        if (recvLen <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                if (time(NULL) - beforeTime > (uint32_t)waitSec) {
                    cout << "recv from fd timeout : " << strerror(errno) << ", rst : " << recvLen
                         << ", expect size : " << len << endl;
                    return -1;
                }
                usleep(10 * 1000);
            } else {
                cout << "recv from fd error, error : " << strerror(errno) << ", rst : " << recvLen
                     << ", expect size : " << len << endl;
                return recvLen;
            }
        } else {
            bodySize += recvLen;
        }
    }
    return bodySize;
}

int RecvLogGroup(int fd, sls_logs::LogGroup* logGroup) {
    CmdHeader header;
    int headerSize = RecvAndWait(fd, (char*)&header, sizeof(header), 2);
    if (headerSize != 8) {
        cout << "recv header error" << endl;
        return -1;
    }
    if (header.type != 0x19 || header.len <= 0 || header.len > 100 * 1024 * 1024) {
        cout << "recv error header, type " << header.type << ", len : " << header.len << endl;
        return -1;
    }
    char* data = (char*)malloc(header.len);
    int bodySize = RecvAndWait(fd, data, header.len, 1);
    if (bodySize != header.len) {
        cout << "recv body error" << endl;
        free(data);
        return -1;
    }
    if (!logGroup->ParseFromArray(data, bodySize)) {
        cout << "parse log group from data error" << endl;
        free(data);
        return -1;
    }
    free(data);
    return 0;
}

void PrintLog(const sls_logs::LogGroup& logGroup, bool jsonOutPut, bool detail) {
    if (jsonOutPut) {
        Json::Value rootValue;
        for (int i = 0; i < logGroup.logs_size(); ++i) {
            Json::Value logValue;
            const sls_logs::Log& log = logGroup.logs(i);
            for (int j = 0; j < log.contents_size(); ++j) {
                const sls_logs::Log_Content& content = log.contents(j);
                logValue[content.key()] = content.value();
            }
            rootValue[ToString(i)] = logValue;
        }
        cout << rootValue.toStyledString() << endl;
    } else {
        if (detail) {
            for (int i = 0; i < logGroup.logs_size(); ++i) {
                const sls_logs::Log& log = logGroup.logs(i);
                for (int j = 0; j < log.contents_size(); ++j) {
                    const sls_logs::Log_Content& content = log.contents(j);
                    cout << content.key() << ": " << content.value() << endl;
                }
                cout << endl;
            }
        } else {
            for (int i = 0; i < logGroup.logs_size(); ++i) {
                const sls_logs::Log& log = logGroup.logs(i);
                for (int j = 0; j < log.contents_size(); ++j) {
                    const sls_logs::Log_Content& content = log.contents(j);
                    if (content.key() == "isFinished") {
                        cout << content.value() << endl;
                    }
                }
            }
        }
    }
}

void LogtailInsight::GetTimeStr(const string& allStr, string& timeStr) {
    size_t leftPos = allStr.find('[');
    size_t rightPos = allStr.find(']');
    if (leftPos != string::npos && rightPos != string::npos && leftPos < rightPos) {
        timeStr = allStr.substr(leftPos + 1, rightPos - leftPos - 1);
    }
    mTimeString = timeStr;
    mTimeValue = StringToDatetime(mTimeString);
}

int LogtailInsight::ExecuteCommand(const string& serverAddress,
                                   const string& cmdType,
                                   const std::vector<string>& paramList,
                                   bool jsonOutPut,
                                   bool detailFlag) {
    int fd = unix_socket_conn(serverAddress.c_str());
    if (fd < 0) {
        return -1;
    }
    sls_logs::Log cmdLog;
    cmdLog.set_time(time(NULL));
    sls_logs::Log_Content* cmdTypeContent = cmdLog.add_contents();
    cmdTypeContent->set_key("type");
    cmdTypeContent->set_value(cmdType);
    for (size_t i = 0; i < paramList.size(); ++i) {
        sls_logs::Log_Content* cmdParam = cmdLog.add_contents();
        cmdParam->set_key("param");
        cmdParam->set_value(paramList[i]);
    }
    string cmdLogString = cmdLog.SerializePartialAsString();
    // send header
    CmdHeader header;
    header.type = 0x19;
    header.len = cmdLogString.size();

    send(fd, (const char*)(&header), sizeof(header), 0);
    int sendSize = send(fd, cmdLogString.c_str(), cmdLogString.size(), 0);
    if (sendSize < 0) {
        close(fd);
        cout << "send data to domain socker error , server : " << serverAddress << ", error : " << errno << endl;
        return -1;
    }

    sls_logs::LogGroup logGroup;
    if (RecvLogGroup(fd, &logGroup) == 0) {
        PrintLog(logGroup, jsonOutPut, detailFlag);
        close(fd);
        return 0;
    }
    close(fd);
    return -1;
}

void* LogtailInsight::ForcedExitTimer(void* arg) {
    int waitMilliSeconds = (*(int*)arg) * 1000;
    uint64_t startTimeMs = GetCurrentTimeInMilliSeconds();
    while (true) {
        if (GetCurrentTimeInMilliSeconds() - startTimeMs > (uint64_t)waitMilliSeconds) {
            exit(-1);
        }
        usleep(100 * 1000);
    }

    return 0;
}

LogtailInsight::LogtailInsight() {
    mTimeValue = 0;
}

LogtailInsight::~LogtailInsight() {
}

} // namespace logtail

int main(int argc, char* argv[]) {
    if (argc < 3 || argv[1] != string("status")) {
        cout << ("invalid param, use status -h for help.") << endl;
        return 10;
    }

    auto pInsight = logtail::LogtailInsight::GetInstance();

    argv += 1;
    argc -= 1;

    string opt = argv[1];
    string project;
    string logstore;
    string filename;
    string index = "1";
    string endIndex;

    int rst = 0;
    if (opt == "all") {
        string index = "1";
        if (argc > 2) {
            index = argv[2];
        }
        rst = pInsight->InitStatus(index);
        if (rst != 0) {
            return rst;
        }
        cout << pInsight->GetLogtailStatus();
    } else if (opt == "detail") {
        if (argc > 2) {
            index = argv[2];
        }
        rst = pInsight->InitStatus(index);
        if (rst != 0) {
            return rst;
        }
        cout << pInsight->GetLogtailDetail();
    }

    else if (opt == "active") {
        bool listLogstore = true;
        if (argc > 5 && string(argv[2]) == "--logfile") {
            listLogstore = false;
            index = argv[3];
            project = argv[4];
            logstore = argv[5];
        } else if (argc > 3 && string(argv[2]) == "--logstore") {
            listLogstore = true;
            index = argv[3];
        } else if (argc > 2) {
            index = argv[2];
        }

        rst = pInsight->InitProfile(index);
        if (rst != 0) {
            return rst;
        }
        if (listLogstore) {
            cout << pInsight->ListAllLogstore();
        } else {
            cout << pInsight->listAllFiles(project, logstore);
        }
    } else if (opt == "logstore") {
        if (argc < 5 || (argv[2][0] == '-' && argc < 6)) {
            cout << ("invalid param, use -h for help.") << endl;
            return 10;
        }
        int format = 0;
        if (string(argv[2]).find("--format=") != string::npos) {
            format = string(argv[2]).substr(9) == "json" ? 1 : 0;
            argv += 1;
        }
        index = argv[2];
        project = argv[3];
        logstore = argv[4];
        rst = pInsight->InitProfile(index);
        if (rst != 0) {
            return rst;
        }
        if (!format) {
            cout << pInsight->GetLogStoreProfileSimple(project, logstore);
        } else {
            cout << pInsight->GetLogStoreProfileJson(project, logstore);
        }
    }

    else if (opt == "logfile") {
        if (argc < 6 || (argv[2][0] == '-' && argc < 7)) {
            cout << ("invalid param, use -h for help.") << endl;
            return 10;
        }
        int format = 0;
        if (string(argv[2]).find("--format=") != string::npos) {
            format = string(argv[2]).substr(9) == "json" ? 1 : 0;
            argv += 1;
        }
        index = argv[2];
        project = argv[3];
        logstore = argv[4];
        filename = argv[5];
        rst = pInsight->InitProfile(index);
        if (rst != 0) {
            return rst;
        }
        if (!format) {
            cout << pInsight->GetFileProfileSimple(project, logstore, filename);
        } else {
            cout << pInsight->GetFileProfileJson(project, logstore, filename);
        }
    } else if (opt == "history") {
        if (argc < 6) {
            cout << ("invalid param, use -h for help.") << endl;
            return 10;
        }
        index = argv[2];
        endIndex = argv[3];
        project = argv[4];
        logstore = argv[5];
        if (argc > 6) {
            filename = argv[6];
            return pInsight->QueryLogfileRange(project, logstore, filename, index, endIndex);
        } else {
            return pInsight->QueryLogstoreRange(project, logstore, index, endIndex);
        }
    } else if (opt == "command") {
        if (argc < 3) {
            cout << ("invalid param, use -h for help.") << endl;
            return 10;
        }
        string cmdType = argv[2];
        vector<string> cmdParamList;
        string serverAddress("/tmp/logtail.sock");
        bool jsonOutFlag = false, detailFlag = false;
        for (int i = 3; i < argc; ++i) {
            string value(argv[i]);
            const static string serverAddressParam = "--server-address=";
            const static string formatJson = "--format=json";
            if (value.find(serverAddressParam) == 0) {
                serverAddress = value.substr(serverAddressParam.size());
            } else if (formatJson == value) {
                jsonOutFlag = true;
            } else {
                if (value == "detail") {
                    detailFlag = true;
                }
                cmdParamList.push_back(value);
            }
        }

        pthread_t tid;
        int waitSeconds = 5;
        pthread_create(&tid, NULL, logtail::LogtailInsight::ForcedExitTimer, (void*)&waitSeconds);
        return pInsight->ExecuteCommand(serverAddress, cmdType, cmdParamList, jsonOutFlag, detailFlag);
    } else if (opt == "--help" || opt == "-h" || opt == "help") {
        cout << "logtail insight, version : 0.1.0\n\n";
        cout << "commond list :\n";
        cout << "       status all [index] \n             get logtail running status \n";
        cout << "       status active [--logstore | --logfile] index [project] [logstore] \n             list all "
                "active logstore | logfile. if use --logfile, please add project and logstore. default --logstore\n";
        cout << "       status logstore [--format=line | json] index project logstore \n             get logstore "
                "status with line or json style. default --format=line \n";
        cout << "       status logfile [--format=line | json] index project logstore fileFullPath \n             get "
                "log file status with line or json style. default --format=line \n";
        cout << "       status history beginIndex endIndex  project logstore [fileFullPath] \n             query "
                "logstore | logfile history status.  \n\n";
        cout << "index :   from 1 to 60. in all, it means last $(index) minutes; in active/logstore/logfile/history, "
                "it means last $(index)*10 minutes \n";
    } else {
        cout << ("invalid param, use -h for help.") << endl;
        rst = 10;
    }

    return rst;
}
