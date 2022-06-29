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

#ifndef __OAS_SHENNONG_GLOBAL_PARA_H__
#define __OAS_SHENNONG_GLOBAL_PARA_H__

#include <string>
#include "Lock.h"

namespace logtail {
class LogtailGlobalPara {
private:
    std::string mTopic;
    SpinLock mSpinLock;
    volatile bool mSigtermFlag;

    LogtailGlobalPara() { mSigtermFlag = false; }

    LogtailGlobalPara(const LogtailGlobalPara&);

public:
    static LogtailGlobalPara* Instance() {
        static LogtailGlobalPara sGlobalPara;
        return &sGlobalPara;
    }

    bool GetSigtermFlag() { return mSigtermFlag; }

    void SetSigtermFlag(bool sigterm) { mSigtermFlag = sigterm; }

    void SetTopic(const std::string& topic) {
        ScopedSpinLock lock(mSpinLock);
        mTopic = topic;
    }

    std::string GetTopic() {
        ScopedSpinLock lock(mSpinLock);
        return mTopic;
    }
};

} // namespace logtail
#endif
