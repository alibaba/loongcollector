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

#include "plugin/PluginRegistry.h"

#include <dirent.h>
#include <dlfcn.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "app_config/AppConfig.h"
#include "common/Flags.h"
#include "flusher/blackhole/FlusherBlackHole.h"
#include "flusher/sls/FlusherSLS.h"
#include "input/InputContainerStdio.h"
#include "input/InputFile.h"
#if defined(__linux__) && !defined(__ANDROID__)
#include "input/InputObserverNetwork.h"
#ifdef __ENTERPRISE__
#include "input/InputStream.h"
#endif
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
#include "processor/inner/ProcessorMergeMultilineLogNative.h"
#include "processor/inner/ProcessorParseContainerLogNative.h"
#include "processor/inner/ProcessorSplitLogStringNative.h"
#include "processor/inner/ProcessorSplitMultilineLogStringNative.h"
#include "processor/inner/ProcessorTagNative.h"
#if defined(__linux__) && !defined(__ANDROID__) && !defined(__EXCLUDE_SPL__)
#include "processor/ProcessorSPL.h"
#endif


DEFINE_FLAG_BOOL(enable_processor_spl, "", true);

using namespace std;

namespace logtail {

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

unique_ptr<InputInstance> PluginRegistry::CreateInput(const string& name, const string& pluginId) {
    return unique_ptr<InputInstance>(static_cast<InputInstance*>(Create(INPUT_PLUGIN, name, pluginId).release()));
}

unique_ptr<ProcessorInstance> PluginRegistry::CreateProcessor(const string& name, const string& pluginId) {
    return unique_ptr<ProcessorInstance>(
        static_cast<ProcessorInstance*>(Create(PROCESSOR_PLUGIN, name, pluginId).release()));
}

unique_ptr<FlusherInstance> PluginRegistry::CreateFlusher(const string& name, const string& pluginId) {
    return unique_ptr<FlusherInstance>(static_cast<FlusherInstance*>(Create(FLUSHER_PLUGIN, name, pluginId).release()));
}

bool PluginRegistry::IsValidGoPlugin(const string& name) const {
    // If the plugin is not a C++ plugin, iLogtail core considers it is a go plugin.
    // Go PluginManager validates the go plugins instead of C++ core.
    return !IsValidNativeInputPlugin(name) && !IsValidNativeProcessorPlugin(name) && !IsValidNativeFlusherPlugin(name);
}

bool PluginRegistry::IsValidNativeInputPlugin(const string& name) const {
    return mPluginDict.find(PluginKey(INPUT_PLUGIN, name)) != mPluginDict.end();
}

bool PluginRegistry::IsValidNativeProcessorPlugin(const string& name) const {
    return mPluginDict.find(PluginKey(PROCESSOR_PLUGIN, name)) != mPluginDict.end();
}

bool PluginRegistry::IsValidNativeFlusherPlugin(const string& name) const {
    return mPluginDict.find(PluginKey(FLUSHER_PLUGIN, name)) != mPluginDict.end();
}

void PluginRegistry::LoadStaticPlugins() {
    RegisterInputCreator(new StaticInputCreator<InputFile>());
#if defined(__linux__) && !defined(__ANDROID__)
    RegisterInputCreator(new StaticInputCreator<InputContainerStdio>());
    RegisterInputCreator(new StaticInputCreator<InputObserverNetwork>());
#ifdef __ENTERPRISE__
    RegisterInputCreator(new StaticInputCreator<InputStream>());
#endif
#endif

    RegisterProcessorCreator(new StaticProcessorCreator<ProcessorSplitLogStringNative>());
    RegisterProcessorCreator(new StaticProcessorCreator<ProcessorSplitMultilineLogStringNative>());
    RegisterProcessorCreator(new StaticProcessorCreator<ProcessorMergeMultilineLogNative>());
    RegisterProcessorCreator(new StaticProcessorCreator<ProcessorParseContainerLogNative>());
    RegisterProcessorCreator(new StaticProcessorCreator<ProcessorTagNative>());

    RegisterProcessorCreator(new StaticProcessorCreator<ProcessorParseApsaraNative>());
    RegisterProcessorCreator(new StaticProcessorCreator<ProcessorParseDelimiterNative>());
    RegisterProcessorCreator(new StaticProcessorCreator<ProcessorDesensitizeNative>());
    RegisterProcessorCreator(new StaticProcessorCreator<ProcessorParseJsonNative>());
    RegisterProcessorCreator(new StaticProcessorCreator<ProcessorParseRegexNative>());
    RegisterProcessorCreator(new StaticProcessorCreator<ProcessorParseTimestampNative>());
    RegisterProcessorCreator(new StaticProcessorCreator<ProcessorFilterNative>());
#if defined(__linux__) && !defined(__ANDROID__) && !defined(__EXCLUDE_SPL__)
    if (BOOL_FLAG(enable_processor_spl)) {
        RegisterProcessorCreator(new StaticProcessorCreator<ProcessorSPL>());
    }
#endif

    RegisterFlusherCreator(new StaticFlusherCreator<FlusherSLS>());
    RegisterFlusherCreator(new StaticFlusherCreator<FlusherBlackHole>());
}

void PluginRegistry::LoadDynamicPlugins(const set<string>& plugins) {
    if (plugins.empty()) {
        return;
    }
    string error;
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

PluginCreator* PluginRegistry::LoadProcessorPlugin(DynamicLibLoader& loader, const string pluginName) {
    string error;
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
    mPluginDict.emplace(PluginKey(cat, creator->Name()), shared_ptr<PluginCreator>(creator));
}

unique_ptr<PluginInstance> PluginRegistry::Create(PluginCat cat, const string& name, const string& pluginId) {
    unique_ptr<PluginInstance> ins;
    auto creatorEntry = mPluginDict.find(PluginKey(cat, name));
    if (creatorEntry != mPluginDict.end()) {
        ins = creatorEntry->second->Create(pluginId);
    }
    return ins;
}

} // namespace logtail