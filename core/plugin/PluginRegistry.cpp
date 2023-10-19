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

#include "plugin/PluginRegistry.h"

#include <dlfcn.h>
#include <unistd.h>
#include <dirent.h>

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>

#include "app_config/AppConfig.h"
// #include "flusher/FlusherSLS.h"
// #include "input/InputFile.h"
// #include "input/InputObserverNetwork.h"
#ifdef __ENTERPRISE__
#include "input/InputStream.h"
#endif
#include "logger/Logger.h"
#include "plugin/creator/CProcessor.h"
#include "plugin/creator/DynamicCProcessorCreator.h"
#include "plugin/creator/StaticFlusherCreator.h"
#include "plugin/creator/StaticInputCreator.h"
#include "plugin/creator/StaticProcessorCreator.h"
#include "processor/ProcessorDesensitizeNative.h"
#include "processor/ProcessorFilterNative.h"
#include "processor/ProcessorParseApsaraNative.h"
#include "processor/ProcessorParseDelimiterNative.h"
#include "processor/ProcessorParseJsonNative.h"
#include "processor/ProcessorParseRegexNative.h"
#include "processor/ProcessorParseTimestampNative.h"
#include "processor/ProcessorSplitLogStringNative.h"
#include "processor/ProcessorSplitRegexNative.h"
#include "processor/ProcessorTagNative.h"

namespace logtail {

PluginRegistry::PluginRegistry() {
    mGoPlugins = {""};
}

void PluginRegistry::LoadPlugins() {
    LoadStaticPlugins();
    auto& plugins = AppConfig::GetInstance()->GetDynamicPlugins();
    LoadDynamicPlugins(plugins);
}

void PluginRegistry::UnloadPlugins() {
    // for (auto& kv : mPluginDict) {
    // if (node->plugin_type() == PLUGIN_TYPE_DYNAMIC) {
    //     CPluginRegistryItem* registry = reinterpret_cast<CPluginRegistryItem*>(node);
    //     if (strcmp(registry->mPlugin->language, "Go") == 0) {
    //         destroy_go_plugin_interface(registry->_handle,
    //         const_cast<plugin_interface_t*>(registry->mPlugin));
    //     }
    // }
    //     UnregisterCreator(kv.second.get());
    // }
    mPluginDict.clear();
}

std::unique_ptr<InputInstance> PluginRegistry::CreateInput(const std::string& name, const std::string& pluginId) {
    return std::unique_ptr<InputInstance>(static_cast<InputInstance*>(Create(INPUT_PLUGIN, name, pluginId).release()));
}

std::unique_ptr<ProcessorInstance> PluginRegistry::CreateProcessor(const std::string& name,
                                                                   const std::string& pluginId) {
    return std::unique_ptr<ProcessorInstance>(
        static_cast<ProcessorInstance*>(Create(PROCESSOR_PLUGIN, name, pluginId).release()));
}

std::unique_ptr<FlusherInstance> PluginRegistry::CreateFlusher(const std::string& name, const std::string& pluginId) {
    return std::unique_ptr<FlusherInstance>(
        static_cast<FlusherInstance*>(Create(FLUSHER_PLUGIN, name, pluginId).release()));
}

bool PluginRegistry::IsValidGoPlugin(const std::string& name) {
    return mGoPlugins.find(name) != mGoPlugins.end();
}

void PluginRegistry::LoadStaticPlugins() {
    // RegisterInputCreator(new StaticInputCreator<InputFile>());
    // RegisterInputCreator(new StaticInputCreator<InputObserverNetwork>());
#ifdef __ENTERPRISE__
    RegisterInputCreator(new StaticInputCreator<InputStream>());
#endif

    RegisterProcessorCreator(new StaticProcessorCreator<ProcessorSplitLogStringNative>());
    RegisterProcessorCreator(new StaticProcessorCreator<ProcessorSplitRegexNative>());
    RegisterProcessorCreator(new StaticProcessorCreator<ProcessorParseApsaraNative>());
    RegisterProcessorCreator(new StaticProcessorCreator<ProcessorParseDelimiterNative>());
    RegisterProcessorCreator(new StaticProcessorCreator<ProcessorDesensitizeNative>());
    RegisterProcessorCreator(new StaticProcessorCreator<ProcessorParseJsonNative>());
    RegisterProcessorCreator(new StaticProcessorCreator<ProcessorParseRegexNative>());
    RegisterProcessorCreator(new StaticProcessorCreator<ProcessorParseTimestampNative>());
    RegisterProcessorCreator(new StaticProcessorCreator<ProcessorTagNative>());
    RegisterProcessorCreator(new StaticProcessorCreator<ProcessorFilterNative>());

    // RegisterFlusherCreator(new StaticFlusherCreator<FlusherSLS>());
}

void PluginRegistry::LoadDynamicPlugins(const std::set<std::string>& plugins) {
    if (plugins.empty()) {
        return;
    }
    std::string error;
    auto pluginDir = AppConfig::GetInstance()->GetProcessExecutionDir() + "/plugins";
    for (auto& pluginName : plugins) {
        DynamicLibLoader loader;
        if (!loader.LoadDynLib(pluginName, error, pluginDir)) {
            LOG_ERROR(sLogger, ("open plugin", pluginName)("error", error));
            continue;
        }
        PluginCreator* creator = LoadProcessorPlugin(loader, pluginName);
        if (creator) {
            RegisterProcessorCreator(creator);
            continue;
        }
    }
}

void PluginRegistry::RegisterInputCreator(PluginCreator* creator) {
    RegisterCreator(INPUT_PLUGIN, creator);
}

void PluginRegistry::RegisterProcessorCreator(PluginCreator* creator) {
    RegisterCreator(PROCESSOR_PLUGIN, creator);
}

void PluginRegistry::RegisterFlusherCreator(PluginCreator* creator) {
    RegisterCreator(FLUSHER_PLUGIN, creator);
}

PluginCreator* PluginRegistry::LoadProcessorPlugin(DynamicLibLoader& loader, const std::string pluginName) {
    std::string error;
    processor_interface_t* plugin = (processor_interface_t*)loader.LoadMethod("processor_interface", error);
    // if (!error.empty()) {
    //     loader.LoadMethod("x_cgo_init", error)
    //     if (error.empty()) { // try go plugin
    //         plugin = create_go_plugin_interface(handle);
    //     }
    // }
    if (!error.empty() || !plugin) {
        LOG_ERROR(sLogger, ("load method", "plugin_interface")("error", error));
        return nullptr;
    }
    if (plugin->version != PROCESSOR_INTERFACE_VERSION) {
        LOG_ERROR(sLogger,
                  ("load plugin", pluginName)("error", "plugin interface version mismatch")(
                      "expected", PROCESSOR_INTERFACE_VERSION)("actual", plugin->version));
        return nullptr;
    }
    return new DynamicCProcessorCreator(plugin, loader.Release());
}

void PluginRegistry::RegisterCreator(PluginCat cat, PluginCreator* creator) {
    if (!creator) {
        return;
    }
    mPluginDict.emplace(PluginKey(cat, creator->Name()), std::shared_ptr<PluginCreator>(creator));
}

std::unique_ptr<PluginInstance>
PluginRegistry::Create(PluginCat cat, const std::string& name, const std::string& pluginId) {
    std::unique_ptr<PluginInstance> ins;
    auto creatorEntry = mPluginDict.find(PluginKey(cat, name));
    if (creatorEntry != mPluginDict.end()) {
        ins = creatorEntry->second->Create(pluginId);
    }
    return ins;
}

} // namespace logtail
