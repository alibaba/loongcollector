// Copyright 2022 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "EventListener_Windows.h"

namespace logtail {

EventListener::~EventListener() {
}

bool EventListener::Init() {
    return true;
}

bool EventListener::IsInit() {
    return true;
}

void EventListener::Destroy() {
}

bool EventListener::IsValidID(int id) {
    return id >= 0;
}

int EventListener::AddWatch(const char* dir) {
    static int counter = 0;
    auto ret = counter++;
    return (ret >= 0) ? ret : 0;
}

bool EventListener::RemoveWatch(int wd) {
    return 0;
}

int32_t EventListener::ReadEvents(std::vector<Event*>& eventVec) {
    return 0;
}

} // namespace logtail