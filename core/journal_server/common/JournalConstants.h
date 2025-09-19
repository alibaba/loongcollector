#pragma once

#include <map>
#include <string>

namespace logtail {

/**
 * @brief Journal相关的常量和映射表
 * 
 * 包含从Go版本移植的syslog设施类型和优先级转换映射
 */
class JournalConstants {
public:
    // Syslog设施转换映射表
    static const std::map<std::string, std::string> kSyslogFacilityString;
    
    // 优先级转换映射表
    static const std::map<std::string, std::string> kPriorityConversionMap;
};

} // namespace logtail 