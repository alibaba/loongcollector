#include "prometheus/Utils.h"

#include <xxhash/xxhash.h>

#include <iomanip>

#include "common/StringTools.h"
#include "http/HttpResponse.h"
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
    // check duration format <duration>s or <duration>m
    if (duration.size() <= 1 || !IsNumber(duration.substr(0, duration.size() - 1))) {
        return 0;
    }
    if (EndWith(duration, "s")) {
        return stoll(duration.substr(0, duration.find('s')));
    }
    if (EndWith(duration, "m")) {
        return stoll(duration.substr(0, duration.find('m'))) * 60;
    }
    return 0;
}

// <size>: a size in bytes, e.g. 512MB. A unit is required. Supported units: B, KB, MB, GB, TB, PB, EB.
uint64_t SizeToByte(const std::string& size) {
    auto inputSize = size;
    uint64_t res = 0;
    if (size.empty()) {
        res = 0;
    } else if (EndWith(inputSize, "KiB") || EndWith(inputSize, "K") || EndWith(inputSize, "KB")) {
        if (!IsNumber(inputSize.substr(0, inputSize.find('K')))) {
            return 0;
        }
        inputSize = inputSize.substr(0, inputSize.find('K'));
        res = stoll(inputSize) * 1024;
    } else if (EndWith(inputSize, "MiB") || EndWith(inputSize, "M") || EndWith(inputSize, "MB")) {
        if (!IsNumber(inputSize.substr(0, inputSize.find('M')))) {
            return 0;
        }
        inputSize = inputSize.substr(0, inputSize.find('M'));
        res = stoll(inputSize) * 1024 * 1024;
    } else if (EndWith(inputSize, "GiB") || EndWith(inputSize, "G") || EndWith(inputSize, "GB")) {
        if (!IsNumber(inputSize.substr(0, inputSize.find('G')))) {
            return 0;
        }
        inputSize = inputSize.substr(0, inputSize.find('G'));
        res = stoll(inputSize) * 1024 * 1024 * 1024;
    } else if (EndWith(inputSize, "TiB") || EndWith(inputSize, "T") || EndWith(inputSize, "TB")) {
        if (!IsNumber(inputSize.substr(0, inputSize.find('T')))) {
            return 0;
        }
        inputSize = inputSize.substr(0, inputSize.find('T'));
        res = stoll(inputSize) * 1024 * 1024 * 1024 * 1024;
    } else if (EndWith(inputSize, "PiB") || EndWith(inputSize, "P") || EndWith(inputSize, "PB")) {
        if (!IsNumber(inputSize.substr(0, inputSize.find('P')))) {
            return 0;
        }
        inputSize = inputSize.substr(0, inputSize.find('P'));
        res = stoll(inputSize) * 1024 * 1024 * 1024 * 1024 * 1024;
    } else if (EndWith(inputSize, "EiB") || EndWith(inputSize, "E") || EndWith(inputSize, "EB")) {
        if (!IsNumber(inputSize.substr(0, inputSize.find('E')))) {
            return 0;
        }
        inputSize = inputSize.substr(0, inputSize.find('E'));
        res = stoll(inputSize) * 1024 * 1024 * 1024 * 1024 * 1024 * 1024;
    } else if (EndWith(inputSize, "B")) {
        if (!IsNumber(inputSize.substr(0, inputSize.find('B')))) {
            return 0;
        }
        inputSize = inputSize.substr(0, inputSize.find('B'));
        res = stoll(inputSize);
    }
    return res;
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

bool IsNumber(const std::string& str) {
    return !str.empty() && str.find_first_not_of("0123456789") == std::string::npos;
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

namespace prom {

    std::string NetworkCodeToState(NetworkCode code) {
        static map<uint64_t, string> sNetworkCodeMap = {{NetworkCode::Ok, "OK"},
                                                        {NetworkCode::ConnectionFailed, "ERR_CONN_FAILED"},
                                                        {NetworkCode::RemoteAccessDenied, "ERR_ACCESS_DENIED"},
                                                        {NetworkCode::Timeout, "ERR_TIMEOUT"},
                                                        {NetworkCode::SSLConnectError, "ERR_SSL_CONN_ERR"},
                                                        {NetworkCode::SSLCertError, "ERR_SSL_CERT_ERR"},
                                                        {NetworkCode::SSLOtherProblem, "ERR_SSL_OTHER_PROBLEM"},
                                                        {NetworkCode::SendDataFailed, "ERR_SEND_DATA_FAILED"},
                                                        {NetworkCode::RecvDataFailed, "ERR_RECV_DATA_FAILED"},
                                                        {NetworkCode::Other, "ERR_UNKNOWN"}};
        static string sCurlOther = "ERR_UNKNOWN";
        if (sNetworkCodeMap.count(code)) {
            return sNetworkCodeMap[code];
        }
        return sCurlOther;
    }

    std::string HttpCodeToState(uint64_t code) {
        if (code > 1000) {
            return "ERR_HTTP_UNKNOWN";
        }
        if (code == 200) {
            return "OK";
        }
        string statePrefix = "ERR_HTTP_";
        return statePrefix + ToString(code);
    }

} // namespace prom
} // namespace logtail
