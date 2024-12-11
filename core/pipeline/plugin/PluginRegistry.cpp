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

#include "pipeline/plugin/PluginRegistry.h"

#include <dirent.h>
#include <dlfcn.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "app_config/AppConfig.h"
#include "common/Flags.h"
#include "plugin/flusher/blackhole/FlusherBlackHole.h"
#include "plugin/flusher/file/FlusherFile.h"
#include "plugin/flusher/sls/FlusherSLS.h"
#ifdef __ENTERPRISE__
#include "plugin/flusher/sls/EnterpriseFlusherSLSMonitor.h"
#endif
#include "plugin/input/InputContainerStdio.h"
#include "plugin/input/InputFile.h"
#include "plugin/input/InputPrometheus.h"
#if defined(__linux__) && !defined(__ANDROID__)
#include "plugin/input/InputFileSecurity.h"
#include "plugin/input/InputInternalMetrics.h"
#include "plugin/input/InputNetworkObserver.h"
#include "plugin/input/InputNetworkSecurity.h"
#include "plugin/input/InputProcessSecurity.h"
#endif
#include "logger/Logger.h"
#include "pipeline/plugin/creator/CProcessor.h"
#include "pipeline/plugin/creator/DynamicCProcessorCreator.h"
#include "pipeline/plugin/creator/StaticFlusherCreator.h"
#include "pipeline/plugin/creator/StaticInputCreator.h"
#include "pipeline/plugin/creator/StaticProcessorCreator.h"
#include "plugin/processor/ProcessorDesensitizeNative.h"
#include "plugin/processor/ProcessorFilterNative.h"
#include "plugin/processor/ProcessorParseApsaraNative.h"
#include "plugin/processor/ProcessorParseDelimiterNative.h"
#include "plugin/processor/ProcessorParseJsonNative.h"
#include "plugin/processor/ProcessorParseRegexNative.h"
#include "plugin/processor/ProcessorParseTimestampNative.h"
#include "plugin/processor/inner/ProcessorMergeMultilineLogNative.h"
#include "plugin/processor/inner/ProcessorParseContainerLogNative.h"
#include "plugin/processor/inner/ProcessorPromParseMetricNative.h"
#include "plugin/processor/inner/ProcessorPromRelabelMetricNative.h"
#include "plugin/processor/inner/ProcessorSplitLogStringNative.h"
#include "plugin/processor/inner/ProcessorSplitMultilineLogStringNative.h"
#include "plugin/processor/inner/ProcessorTagNative.h"
#if defined(__linux__) && !defined(__ANDROID__) && !defined(__EXCLUDE_SPL__)
#include "plugin/processor/ProcessorSPL.h"
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

unique_ptr<InputInstance> PluginRegistry::CreateInput(const string& name,
                                                      const PluginInstance::PluginMeta& pluginMeta) {
    return unique_ptr<InputInstance>(static_cast<InputInstance*>(Create(INPUT_PLUGIN, name, pluginMeta).release()));
}

unique_ptr<ProcessorInstance> PluginRegistry::CreateProcessor(const string& name,
                                                              const PluginInstance::PluginMeta& pluginMeta) {
    return unique_ptr<ProcessorInstance>(
        static_cast<ProcessorInstance*>(Create(PROCESSOR_PLUGIN, name, pluginMeta).release()));
}

unique_ptr<FlusherInstance> PluginRegistry::CreateFlusher(const string& name,
                                                          const PluginInstance::PluginMeta& pluginMeta) {
    return unique_ptr<FlusherInstance>(
        static_cast<FlusherInstance*>(Create(FLUSHER_PLUGIN, name, pluginMeta).release()));
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
    RegisterInputCreator(new StaticInputCreator<InputPrometheus>());
    RegisterInputCreator(new StaticInputCreator<InputInternalMetrics>(), true);
#if defined(__linux__) && !defined(__ANDROID__)
    RegisterInputCreator(new StaticInputCreator<InputContainerStdio>());
    RegisterInputCreator(new StaticInputCreator<InputFileSecurity>(), true);
    RegisterInputCreator(new StaticInputCreator<InputNetworkObserver>(), true);
    RegisterInputCreator(new StaticInputCreator<InputNetworkSecurity>(), true);
    RegisterInputCreator(new StaticInputCreator<InputProcessSecurity>(), true);
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
    RegisterProcessorCreator(new StaticProcessorCreator<ProcessorPromParseMetricNative>());
    RegisterProcessorCreator(new StaticProcessorCreator<ProcessorPromRelabelMetricNative>());
#if defined(__linux__) && !defined(__ANDROID__) && !defined(__EXCLUDE_SPL__)
    if (BOOL_FLAG(enable_processor_spl)) {
        RegisterProcessorCreator(new StaticProcessorCreator<ProcessorSPL>());
    }
#endif

    RegisterFlusherCreator(new StaticFlusherCreator<FlusherSLS>());
    RegisterFlusherCreator(new StaticFlusherCreator<FlusherBlackHole>());
    RegisterFlusherCreator(new StaticFlusherCreator<FlusherFile>());
#ifdef __ENTERPRISE__
    RegisterFlusherCreator(new StaticFlusherCreator<FlusherSLSMonitor>());
#endif
}

void PluginRegistry::LoadDynamicPlugins(const set<string>& plugins) {
    if (plugins.empty()) {
        return;
    }
    string error;
    // 动态插件加载
    auto pluginDir = AppConfig::GetInstance()->GetProcessExecutionDir() + "/plugins";
    for (auto& pluginType : plugins) {
        DynamicLibLoader loader;
        if (!loader.LoadDynLib(pluginType, error, pluginDir)) {
            LOG_ERROR(sLogger, ("open plugin", pluginType)("error", error));
            continue;
        }
        PluginCreator* creator = LoadProcessorPlugin(loader, pluginType);
        if (creator) {
            RegisterProcessorCreator(creator);
            continue;
        }
    }
}

void PluginRegistry::RegisterInputCreator(PluginCreator* creator, bool isSingleton) {
    RegisterCreator(INPUT_PLUGIN, creator, isSingleton);
}

void PluginRegistry::RegisterProcessorCreator(PluginCreator* creator) {
    RegisterCreator(PROCESSOR_PLUGIN, creator, false);
}

void PluginRegistry::RegisterFlusherCreator(PluginCreator* creator, bool isSingleton) {
    RegisterCreator(FLUSHER_PLUGIN, creator, isSingleton);
}

PluginCreator* PluginRegistry::LoadProcessorPlugin(DynamicLibLoader& loader, const string pluginType) {
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
                  ("load plugin", pluginType)("error", "plugin interface version mismatch")(
                      "expected", PROCESSOR_INTERFACE_VERSION)("actual", plugin->version));
        return nullptr;
    }
    return new DynamicCProcessorCreator(plugin, loader.Release());
}

void PluginRegistry::RegisterCreator(PluginCat cat, PluginCreator* creator, bool isSingleton) {
    if (!creator) {
        return;
    }
    mPluginDict.emplace(PluginKey(cat, creator->Name()),
                        PluginCreatorWithInfo(shared_ptr<PluginCreator>(creator), isSingleton));
}

unique_ptr<PluginInstance>
PluginRegistry::Create(PluginCat cat, const string& name, const PluginInstance::PluginMeta& pluginMeta) {
    unique_ptr<PluginInstance> ins;
    auto creatorEntry = mPluginDict.find(PluginKey(cat, name));
    if (creatorEntry != mPluginDict.end()) {
        ins = creatorEntry->second.first->Create(pluginMeta);
    }
    return ins;
}

bool PluginRegistry::IsGlobalSingletonInputPlugin(const string& name) const {
    return IsGlobalSingleton(INPUT_PLUGIN, name);
}

bool PluginRegistry::IsGlobalSingleton(PluginCat cat, const string& name) const {
    auto creatorEntry = mPluginDict.find(PluginKey(cat, name));
    if (creatorEntry != mPluginDict.end()) {
        return creatorEntry->second.second;
    }
    return false;
}

} // namespace logtail
