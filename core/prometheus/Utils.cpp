#include "prometheus/Utils.h"

#include <xxhash/xxhash.h>

#include <iomanip>

#include "common/StringTools.h"
#include "models/StringView.h"

using namespace std;

namespace logtail {
string URLEncode(const string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << hex;

    for (char c : value) {
        // Keep alphanumeric characters and other safe characters intact
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
            continue;
        }
        // Any other characters are percent-encoded
        escaped << '%' << setw(2) << int((unsigned char)c);
    }

    return escaped.str();
}

std::string SecondToDuration(uint64_t duration) {
    return (duration % 60 == 0) ? ToString(duration / 60) + "m" : ToString(duration) + "s";
}

uint64_t DurationToSecond(const std::string& duration) {
    if (EndWith(duration, "s")) {
        return stoll(duration.substr(0, duration.find('s')));
    }
    if (EndWith(duration, "m")) {
        return stoll(duration.substr(0, duration.find('m'))) * 60;
    }
    return 60;
}

bool IsValidMetric(const StringView& line) {
    for (auto c : line) {
        if (c == ' ' || c == '\t') {
            continue;
        }
        if (c == '#') {
            return false;
        }
        return true;
    }
    return false;
}

void SplitStringView(const std::string& s, char delimiter, std::vector<StringView>& result) {
    size_t start = 0;
    size_t end = 0;

    while ((end = s.find(delimiter, start)) != std::string::npos) {
        result.emplace_back(s.data() + start, end - start);
        start = end + 1;
    }
    if (start < s.size()) {
        result.emplace_back(s.data() + start, s.size() - start);
    }
}

uint64_t GetRandSleepMilliSec(const std::string& key, uint64_t intervalSeconds, uint64_t currentMilliSeconds) {
    // Pre-compute the inverse of the maximum value of uint64_t
    static constexpr double sInverseMaxUint64 = 1.0 / static_cast<double>(std::numeric_limits<uint64_t>::max());

    uint64_t h = XXH64(key.c_str(), key.length(), 0);

    // Normalize the hash to the range [0, 1]
    double normalizedH = static_cast<double>(h) * sInverseMaxUint64;

    // Scale the normalized hash to milliseconds
    auto randSleep = static_cast<uint64_t>(std::ceil(intervalSeconds * 1000.0 * normalizedH));

    // calculate sleep window start offset, apply random sleep
    uint64_t sleepOffset = currentMilliSeconds % (intervalSeconds * 1000ULL);
    if (randSleep < sleepOffset) {
        randSleep += intervalSeconds * 1000ULL;
    }
    randSleep -= sleepOffset;
    return randSleep;
}
} // namespace logtail
