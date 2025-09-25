#pragma once

#include <map>
#include <string>
#include <vector>

namespace logtail {

/**
 * @brief Journal相关的常量和映射表
 * 
 * 包含从Go版本移植的syslog设施类型和优先级转换映射
 * 以及单元名称处理相关的常量
 */
class JournalConstants {
public:
    // Syslog设施转换映射表
    static const std::map<std::string, std::string> kSyslogFacilityString;
    
    // 优先级转换映射表
    static const std::map<std::string, std::string> kPriorityConversionMap;
    
    // 过滤单元名称处理常量 (from Go implementation)
    static constexpr size_t kUnitNameMax = 256;
    static constexpr const char* kGlobChars = "*?[";
    static constexpr const char* kUppercaseLetters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static constexpr const char* kLowercaseLetters = "abcdefghijklmnopqrstuvwxyz";
    static constexpr const char* kDigits = "0123456789";
    
    static const std::string kLetters;
    static const std::string kValidChars;
    static const std::string kValidCharsWithAt;
    static const std::string kValidCharsGlob;
    
    // System unit fields and types
    static const std::vector<std::string> kSystemUnits;
    static const std::vector<std::string> kUnitTypes;
};

} // namespace logtail 