#include "JournalConstants.h"

namespace logtail {

// Syslog设施转换映射表（来自Go版本）
const std::map<std::string, std::string> JournalConstants::kSyslogFacilityString = {
    {"0",  "kernel"},
    {"1",  "user"},
    {"2",  "mail"},
    {"3",  "daemon"},
    {"4",  "auth"},
    {"5",  "syslog"},
    {"6",  "line printer"},
    {"7",  "network news"},
    {"8",  "uucp"},
    {"9",  "clock daemon"},
    {"10", "security/auth"},
    {"11", "ftp"},
    {"12", "ntp"},
    {"13", "log audit"},
    {"14", "log alert"},
    {"15", "clock daemon"},
    {"16", "local0"},
    {"17", "local1"},
    {"18", "local2"},
    {"19", "local3"},
    {"20", "local4"},
    {"21", "local5"},
    {"22", "local6"},
    {"23", "local7"}
};

// 优先级转换映射表（来自Go版本）
const std::map<std::string, std::string> JournalConstants::kPriorityConversionMap = {
    {"0", "emergency"},
    {"1", "alert"},
    {"2", "critical"},
    {"3", "error"},
    {"4", "warning"},
    {"5", "notice"},
    {"6", "informational"},
    {"7", "debug"}
};

} // namespace logtail 