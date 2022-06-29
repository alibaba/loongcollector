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
#include <string>
#include <vector>

namespace logtail {

class Event;

class EventListener {
public:
    ~EventListener();

    static EventListener* GetInstance() {
        static EventListener eventListener;
        return &eventListener;
    }

    bool Init();
    bool IsInit();
    void Destroy();

    static bool IsValidID(int id);

    int AddWatch(const char* dir);
    bool RemoveWatch(int wd);

    int32_t ReadEvents(std::vector<Event*>& eventVec);

private:
    EventListener() = default;
};

} // namespace logtail