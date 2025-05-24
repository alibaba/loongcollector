// Copyright 2025 LoongCollector Authors
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

#pragma once

#include <cstdint>
#include <cstring>

#include <algorithm>

#include "coolbpf/security/bpf_process_event_type.h"
#include "coolbpf/security/data_msg.h"

msg_execve_event CreateStubExecveEvent() {
    msg_execve_event event{};
    event.common.op = MSG_OP_EXECVE;
    event.process.pid = 1234;
    event.process.ktime = 123456789;
    event.process.uid = 0;
    event.creds.caps.permitted = 0x11;
    event.creds.caps.effective = 0x33;
    event.creds.caps.inheritable = 0x22;
    event.parent.pid = 5678;
    event.parent.ktime = 567891234;
    return event;
}

void FillExecveEventNoClone(msg_execve_event& event) {
    constexpr char args[] = "/usr/bin/ls\0-l\0/root/one more thing\0/root";
    constexpr uint32_t argsSize = sizeof(args) - 1;
    memcpy(event.buffer + SIZEOF_EVENT, args, argsSize);
    event.process.size = argsSize + SIZEOF_EVENT;

    event.cleanup_key.pid = 1234;
    event.cleanup_key.ktime = 123456780;
}

void FillExecveEventLongFilename(msg_execve_event& event, struct msg_data& msgData) {
    // fill msg_data
    msgData.id.pid = event.process.pid;
    msgData.id.time = event.process.ktime;
    std::fill_n(msgData.arg, 255, 'a');
    msgData.common.op = MSG_OP_DATA;
    msgData.common.ktime = event.process.ktime;
    msgData.common.size = offsetof(struct msg_data, arg) + 255;

    // fill data_event_desc
    auto* desc = reinterpret_cast<data_event_desc*>(event.buffer + SIZEOF_EVENT);
    desc->error = 0;
    desc->pad = 0;
    desc->size = 256;
    desc->leftover = 1;
    desc->id.pid = event.process.pid;
    desc->id.time = event.process.ktime;
    // fill arguments and cwd
    constexpr char args[] = "-l\0/root/one more thing\0";
    constexpr uint32_t argsSize = sizeof(args) - 1;
    memcpy(event.buffer + SIZEOF_EVENT + sizeof(data_event_desc), args, argsSize);
    event.process.size = argsSize + sizeof(data_event_desc) + SIZEOF_EVENT;

    event.process.flags |= EVENT_DATA_FILENAME | EVENT_ROOT_CWD;

    event.cleanup_key.pid = 1234;
    event.cleanup_key.ktime = 123456780;
}
