#include "prometheus/Utils.h"

#include <iomanip>
#include <ios>
#include <iostream>
#include <sstream>

#include "common/StringTools.h"

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

} // namespace logtail
