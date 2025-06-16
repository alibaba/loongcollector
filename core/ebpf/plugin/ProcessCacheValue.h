
// Copyright 2025 LoongCollector Authors
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

#include <coolbpf/security/data_msg.h>
#include <cstdint>

#include <mutex>

#include "ContainerInfo.h"
#include "common/StringView.h"
#include "common/memory/SourceBuffer.h"
#include "ebpf/type/table/ProcessTable.h"
#include "ebpf/type/table/StaticDataRow.h"

namespace logtail {

class ProcessCacheValue {
public:
    enum class LifeStage { kInUse, kDeletePending, kDeleteReady, kDeleted };

    ProcessCacheValue* CloneContents();

    template <const ebpf::DataElement& key>
    const StringView& Get() const {
        return mContents.Get<key>();
    }

    template <const ebpf::DataElement& key>
    void SetContentNoCopy(const StringView& val) {
        mContents.SetNoCopy<key>(StringView(val.data(), val.size()));
    }
    template <const ebpf::DataElement& key>
    void SetContent(const StringView& val) {
        mContents.Set<key>(val);
    }
    template <const ebpf::DataElement& key>
    void SetContent(const std::string& val) {
        mContents.Set<key>(val);
    }

    template <const ebpf::DataElement& key>
    void SetContent(const char* data, size_t len) {
        mContents.Set<key>(data, len);
    }

    template <const ebpf::DataElement& key>
    void SetContent(int32_t val) {
        mContents.Set<key>(val);
    }
    template <const ebpf::DataElement& key>
    void SetContent(uint32_t val) {
        mContents.Set<key>(val);
    }
    template <const ebpf::DataElement& key>
    void SetContent(int64_t val) {
        mContents.Set<key>(val);
    }
    template <const ebpf::DataElement& key>
    void SetContent(uint64_t val) {
        mContents.Set<key>(val);
    }

    template <const ebpf::DataElement& key>
    void SetContent(long long val) {
        mContents.Set<key>(int64_t(val));
    }

    template <const ebpf::DataElement& key>
    void SetContent(unsigned long long val) {
        mContents.Set<key>(uint64_t(val));
    }

    void StoreContainerInfoUnsafe(std::shared_ptr<ContainerMeta> containerInfo) {
        mContainerInfo = std::move(containerInfo);
    }

    void StoreContainerInfo(std::shared_ptr<ContainerMeta> containerInfo) {
        std::lock_guard<std::mutex> lock(mContainerInfoMutex);
        mContainerInfo = std::move(containerInfo);
    }

    std::shared_ptr<ContainerMeta> LoadContainerInfo() const {
        std::lock_guard<std::mutex> lock(mContainerInfoMutex);
        return mContainerInfo;
    }

    void StoreK8sPodInfoUnsafe(std::shared_ptr<K8sPodInfo> k8sPodInfo) { mK8sPodInfo = std::move(k8sPodInfo); }

    void StoreK8sPodInfo(std::shared_ptr<K8sPodInfo> k8sPodInfo) {
        std::lock_guard<std::mutex> lock(mK8sPodInfoMutex);
        mK8sPodInfo = std::move(k8sPodInfo);
    }

    std::shared_ptr<K8sPodInfo> LoadK8sPodInfo() const {
        std::lock_guard<std::mutex> lock(mK8sPodInfoMutex);
        return mK8sPodInfo;
    }

    std::shared_ptr<SourceBuffer> GetSourceBuffer() { return mContents.GetSourceBuffer(); }

    int RefCount() { return mRefCount; }

    int IncRef() { return ++mRefCount; }
    int DecRef() { return --mRefCount; }

    enum LifeStage LifeStage() { return mLifeStage; }

    void SetLifeStage(enum LifeStage lifeStage) { mLifeStage = lifeStage; }

    uint32_t mPPid = 0;
    uint64_t mPKtime = 0;

private:
    ebpf::StaticDataRow<&ebpf::kProcessCacheTable> mContents;
    mutable std::mutex mContainerInfoMutex;
    std::shared_ptr<ContainerMeta> mContainerInfo;
    mutable std::mutex mK8sPodInfoMutex;
    std::shared_ptr<K8sPodInfo> mK8sPodInfo;
    std::atomic_int mRefCount = 0;
    std::atomic<enum LifeStage> mLifeStage = LifeStage::kInUse;
};

} // namespace logtail
