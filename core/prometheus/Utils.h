#pragma once


#include <string>

#include "models/StringView.h"

namespace logtail {

std::string URLEncode(const std::string& value);

std::string SecondToDuration(uint64_t duration);
uint64_t DurationToSecond(const std::string& duration);

bool IsValidMetric(const StringView& line);
void SplitStringView(const std::string& s, char delimiter, std::vector<StringView>& result);

uint64_t GetRandSleepMilliSec(const std::string& key, uint64_t intervalSeconds, uint64_t currentMilliSeconds);
} // namespace logtail
