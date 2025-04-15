// Copyright 2025 iLogtail Authors
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

namespace logtail::ebpf {

// we only use the least 56 bit of spanID
constexpr uint64_t kMaxAdjustedCount = ((uint64_t)1 << 56);
constexpr uint64_t kLeastHalfTraceIDThreasholdMask = (kMaxAdjustedCount - 1);

uint64_t TraceID64ToRandomness(const std::array<uint64_t, 2>& traceID) {
    return traceID[1] & kLeastHalfTraceIDThreasholdMask;
}


constexpr double kMinSamplingProbability = (double)1.0 / double(kMaxAdjustedCount);
constexpr uint64_t kAlwaysSampleThresHold = 0;
constexpr uint64_t kNeverSampleThresHold = kMaxAdjustedCount;

uint64_t ProbabilityToThreshold(double fraction) {
    if (fraction <= kMinSamplingProbability) {
        return kNeverSampleThresHold;
    }
    if (fraction >= 1) {
        return kAlwaysSampleThresHold;
    }
    auto scaled = uint64_t((double)kMaxAdjustedCount * fraction);
    return kMaxAdjustedCount - scaled;
}

double ThresholdToProbability(uint64_t threshold) {
    return double(kMaxAdjustedCount - threshold) / double(kMaxAdjustedCount);
}

RatioSampler::RatioSampler(const double fraction, const uint64_t thresHold)
    : mFraction(fraction), mThresHold(thresHold) {
}

bool RatioSampler::ShouldSample(const std::array<uint64_t, 2>& traceID) const {
    auto rand = TraceID64ToRandomness(traceID);
    return rand >= mThresHold;
}

HashRatioSampler::HashRatioSampler(const double fraction) : RatioSampler(fraction, ProbabilityToThreshold(fraction)) {
}

} // namespace logtail::ebpf
