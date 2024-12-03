#pragma once


#include <string>

#include "common/http/HttpResponse.h"
#include "models/StringView.h"

namespace logtail {

std::string URLEncode(const std::string& value);

std::string SecondToDuration(uint64_t duration);
uint64_t DurationToSecond(const std::string& duration);

uint64_t SizeToByte(const std::string& size);

bool IsValidMetric(const StringView& line);
void SplitStringView(const std::string& s, char delimiter, std::vector<StringView>& result);
bool IsNumber(const std::string& str);

uint64_t GetRandSleepMilliSec(const std::string& key, uint64_t intervalSeconds, uint64_t currentMilliSeconds);

namespace prom {
    std::string NetworkCodeToState(NetworkCode code);
    std::string HttpCodeToState(uint64_t code);
}

} // namespace logtail
