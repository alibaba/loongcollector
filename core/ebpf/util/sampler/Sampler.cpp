// Copyright 2023 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "Sampler.h"

namespace logtail {
namespace ebpf {

// we only use the least 56 bit of spanID
constexpr uint64_t MaxAdjustedCount = ((uint64_t)1 << 56);
constexpr uint64_t LeastHalfTraceIDThreasholdMask = (MaxAdjustedCount - 1);
constexpr int NumHexDigits = 56 / 4;
constexpr int NumSampledBytes = 56 / 8;

uint64_t TraceIDToRandomness(const std::array<uint8_t, 16>& traceID) {
    uint64_t half = (static_cast<uint64_t>(traceID[15])) | (static_cast<uint64_t>(traceID[14]) << 8)
        | (static_cast<uint64_t>(traceID[13]) << 16) | (static_cast<uint64_t>(traceID[12]) << 24)
        | (static_cast<uint64_t>(traceID[11]) << 32) | (static_cast<uint64_t>(traceID[10]) << 40)
        | (static_cast<uint64_t>(traceID[9]) << 48) | (static_cast<uint64_t>(traceID[8]) << 56);

    return half & LeastHalfTraceIDThreasholdMask;
}


constexpr double MinSamplingProbability = (double)1.0 / double(MaxAdjustedCount);
constexpr uint64_t AlwaysSampleThreasHold = 0;
constexpr uint64_t NeverSampleThreasHold = MaxAdjustedCount;

uint64_t ProbabilityToThreshold(double fraction) {
    if (fraction < MinSamplingProbability) {
        return AlwaysSampleThreasHold;
    }
    if (fraction > 1) {
        return AlwaysSampleThreasHold;
    }
    uint64_t scaled = uint64_t((double)MaxAdjustedCount * fraction);
    return MaxAdjustedCount - scaled;
}

double ThresholdToProbability(uint64_t threshold) {
    return double(MaxAdjustedCount - threshold) / double(MaxAdjustedCount);
}

RatioSampler::RatioSampler(const double fraction, const uint64_t thresHold)
    : fraction_(fraction), thresHold_(thresHold) {
}

bool RatioSampler::ShouldSample(const std::array<uint8_t, 16>& traceID) const {
    auto rand = TraceIDToRandomness(traceID);
    return rand >= thresHold_;
}

HashRatioSampler::HashRatioSampler(const double fraction) : RatioSampler(fraction, ProbabilityToThreshold(fraction)) {
}

} // namespace ebpf
} // namespace logtail
