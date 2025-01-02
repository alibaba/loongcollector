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

#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include <memory>
#include <unordered_map>

namespace logtail {
namespace ebpf {

class Sampler {
public:
    [[nodiscard]] virtual bool ShouldSample(const std::array<uint8_t, 16>&) const =0;

    virtual ~Sampler() = default;
};

class RatioSampler : public Sampler {
protected:
    RatioSampler(double fraction, uint64_t thresHold);

public:
    [[nodiscard]] bool ShouldSample(const std::array<uint8_t, 16> &traceID) const override;

private:
    double fraction_;
    uint64_t thresHold_;
#ifdef APSARA_UNIT_TEST_MAIN
    friend class SamplerUnittest;
#endif
};

class HashRatioSampler : public RatioSampler {
public:
    explicit HashRatioSampler(double fraction);

#ifdef APSARA_UNIT_TEST_MAIN
    friend class SamplerUnittest;
#endif
};

}
}
