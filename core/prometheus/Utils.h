#pragma once


#include <string>
namespace logtail {

std::string URLEncode(const std::string& value);

std::string SecondToDuration(uint64_t duration);
uint64_t DurationToSecond(const std::string& duration);
} // namespace logtail
