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

#include "plugin/processor/DynamicCProcessorProxy.h"

namespace logtail {

DynamicCProcessorProxy::DynamicCProcessorProxy(const char* name) : _name(name) {
    _c_ins = new processor_instance_t;
}

DynamicCProcessorProxy::~DynamicCProcessorProxy() {
    _c_ins->plugin->finalize(_c_ins->plugin_state);
    delete _c_ins;
}

bool DynamicCProcessorProxy::Init(const Json::Value& config) {
    return _c_ins->plugin->init(_c_ins, (void*)(&config), (void*)(&GetContext())) == 0;
}

void DynamicCProcessorProxy::Process(PipelineEventGroup& logGroup) {
    _c_ins->plugin->process(_c_ins->plugin_state, &logGroup);
}

bool DynamicCProcessorProxy::IsSupportedEvent(const PipelineEventPtr& /*e*/) const {
    return true;
}

void DynamicCProcessorProxy::SetCProcessor(const processor_interface_t* cprocessor) {
    _c_ins->plugin = cprocessor;
}

} // namespace logtail
