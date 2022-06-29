/*
 * Copyright 2022 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
#include <vector>
#include <deque>
#include <algorithm>
#include <sstream>

namespace logtail {

class SlidingWindowCounter {
    std::vector<size_t> mWindowSizeVec;
    std::deque<size_t> mStats;

public:
    SlidingWindowCounter(const std::vector<size_t>& windowSizeVec) : mWindowSizeVec(windowSizeVec) {
        std::sort(mWindowSizeVec.begin(), mWindowSizeVec.end());
        mStats = std::deque<size_t>(mWindowSizeVec.back(), 0);
    }

    std::string Add(size_t value) {
        mStats.push_front(value);
        mStats.pop_back();

        std::ostringstream outStat;
        size_t windowIndex = 0;
        size_t val = 0;
        for (size_t idx = 0; idx < mStats.size(); ++idx) {
            // Inside current window, sum.
            if (idx < mWindowSizeVec[windowIndex]) {
                val += mStats[idx];
                continue;
            }
            // Switch to next window.
            if (idx == mWindowSizeVec[windowIndex]) {
                outputWindowValue(outStat, windowIndex, val);
                val += mStats[idx];
                windowIndex++;
            }
        }
        outputWindowValue(outStat, windowIndex, val);
        return outStat.str();
    }

private:
    inline void outputWindowValue(std::ostringstream& out, size_t windowIdx, size_t val) {
        if (windowIdx > 0) {
            out << ",";
        }
        out << val;
    }
};

SlidingWindowCounter CreateLoadCounter() {
    return SlidingWindowCounter(std::vector<size_t>{1, 5, 15});
}

} // namespace logtail
