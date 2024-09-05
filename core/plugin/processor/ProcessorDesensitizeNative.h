/*
 * Copyright 2023 iLogtail Authors
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

#include <re2/re2.h>

#include "pipeline/plugin/interface/Processor.h"

namespace logtail {

class ProcessorDesensitizeNative : public Processor {
public:
    static const std::string sName;

    enum class DesensitizeMethod { MD5_OPTION, CONST_OPTION };

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config) override;
    void Process(PipelineEventGroup& logGroup) override;

    // Source field name.
    std::string mSourceKey;
    // Desensitization method. Optional values include:
    // ● const: Replace sensitive content with constants.
    // ● md5: Replace the corresponding content with the MD5 value of the sensitive content.
    DesensitizeMethod mMethod;
    // A constant string used to replace sensitive content.
    std::string mReplacingString;
    // Prefix regular expression for sensitive content.
    std::string mContentPatternBeforeReplacedString;
    // Regular expression for sensitive content.
    std::string mReplacedContentPattern;
    // Whether to replace all matching sensitive content.
    bool mReplacingAll = true;

protected:
    bool IsSupportedEvent(const PipelineEventPtr& e) const override;

private:
    void ProcessEvent(PipelineEventPtr& e);
    void CastOneSensitiveWord(std::string* value);

    std::shared_ptr<re2::RE2> mRegex;

    CounterPtr mProcDesensitizeRecodesTotal;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ProcessorParseApsaraNativeUnittest;
#endif
};

} // namespace logtail
