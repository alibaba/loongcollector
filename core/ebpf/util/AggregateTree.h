// Copyright 2025 iLogtail Authors
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

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/memory/SourceBuffer.h"
#include "logger/Logger.h"

namespace logtail {

template <class Data, class Value, class KeyType, bool NeedSourceBuffer>
class AggTree;

template <class Data, class KeyType>
class AggNode {
public:
    AggNode(const std::shared_ptr<SourceBuffer>& sourceBuffer = nullptr) : mSourceBuffer(sourceBuffer) {}
    std::unordered_map<KeyType, std::unique_ptr<AggNode>> mChild;
    std::shared_ptr<SourceBuffer> mSourceBuffer;
    std::unique_ptr<Data> mData;
};

template <class Data, class Value, class KeyType, bool NeedSourceBuffer>
class AggTree {
private:
    size_t mMaxNodes = 0UL;

    size_t mNodeCount = 0UL;

    std::unique_ptr<AggNode<Data, KeyType>> mRootNode;

    std::function<void(std::unique_ptr<Data>& base, const Value& n)> mAggregateFunc;

    std::function<std::unique_ptr<Data>(const Value& n, std::shared_ptr<SourceBuffer>& sourceBuffer)> mBuildFunc;
#ifdef APSARA_UNIT_TEST_MAIN
    friend class eBPFServerUnittest;
#endif
public:
    AggTree(size_t maxNodes,
            const std::function<void(std::unique_ptr<Data>&, const Value&)>& aggregateFunc,
            const std::function<std::unique_ptr<Data>(const Value& n, std::shared_ptr<SourceBuffer>& sourceBuffer)>&
                buildFunc)
        : mMaxNodes(maxNodes),
          mRootNode(std::make_unique<AggNode<Data, KeyType>>()),
          mAggregateFunc(aggregateFunc),
          mBuildFunc(buildFunc) {}

    AggTree(AggTree<Data, Value, KeyType, NeedSourceBuffer>&& other) noexcept
        : mMaxNodes(other.mMaxNodes),
          mNodeCount(other.mNodeCount),
          mRootNode(std::move(other.mRootNode)),
          mAggregateFunc(other.mAggregateFunc),
          mBuildFunc(other.mBuildFunc) {}

    AggTree& operator=(AggTree<Data, Value, KeyType, NeedSourceBuffer>&& other) noexcept {
        mMaxNodes = other.mMaxNodes;
        mNodeCount = other.mNodeCount;
        mRootNode = std::move(other.mRootNode);
        mAggregateFunc = other.mAggregateFunc;
        mBuildFunc = other.mBuildFunc;
        return *this;
    }

    AggTree<Data, Value, KeyType, NeedSourceBuffer> GetAndReset() {
        AggTree<Data, Value, KeyType, NeedSourceBuffer> res = std::move(*this);
        Reset();
        return res;
    }

    template <class ContainerType>
    bool Aggregate(const Value& d, const ContainerType& aggKeys) {
        auto p = mRootNode.get();
        for (auto& val : aggKeys) {
            auto result = p->mChild.find(val);
            if (result == p->mChild.end() && mNodeCount >= mMaxNodes) {
                // when we exceed the maximum limit, we will drop new metrics
                LOG_ERROR(sLogger, ("maximum limit exceeded", mMaxNodes));
                return false;
            }
            if (result == p->mChild.end()) {
                auto newNode = std::make_unique<AggNode<Data, KeyType>>();
                if (!p->mSourceBuffer) {
                    if (NeedSourceBuffer) {
                        // level1 nodes will setup new sourcebuffer ...
                        newNode->mSourceBuffer = std::make_shared<SourceBuffer>();
                    }
                } else {
                    // level2 or lower nodes will hold the ref of level1 node's
                    newNode->mSourceBuffer = p->mSourceBuffer;
                }
                auto ptr = newNode.get();
                p->mChild[val] = std::move(newNode);
                p = ptr;
                mNodeCount++;
            } else {
                p = result->second.get();
            }
        }
        if (!p->mData) {
            // generate new node ...
            p->mData = mBuildFunc(d, p->mSourceBuffer);
        }
        mAggregateFunc(p->mData, d);
        return true;
    }

    std::vector<AggNode<Data, KeyType>*> GetNodesWithAggDepth(size_t i) {
        std::vector<AggNode<Data, KeyType>*> ans;
        GetNodes(1, mRootNode, i, ans);
        return ans;
    }

    void ForEach(const std::function<void(const Data*)>& call) { ForEach(mRootNode.get(), call); }

    void Reset() {
        // mRootNode = std::make_unique<AggNode<Data, KeyType>>(nullptr);
        mRootNode = std::make_unique<AggNode<Data, KeyType>>();
        mNodeCount = 0;
    }

    void ForEach(const AggNode<Data, KeyType>* root, const std::function<void(const Data*)>& call) {
        if (root == nullptr) {
            return;
        }
        if (root->mData != nullptr) {
            call(root->mData.get());
        }
        for (auto& entry : root->mChild) {
            ForEach(entry.second.get(), call);
        }
    }

    [[nodiscard]] size_t NodeCount() const { return mNodeCount; }

private:
    void GetNodes(size_t depth,
                  const std::unique_ptr<AggNode<Data, KeyType>>& root,
                  size_t targetDepth,
                  std::vector<AggNode<Data, KeyType>*>& ans) {
        if (depth >= targetDepth) {
            // we are done, add mChild to ans and return
            for (auto& c : root->mChild) {
                ans.push_back(c.second.get());
            }
            return;
        }
        // go to next depth
        for (auto& c : root->mChild) {
            GetNodes(depth + 1, c.second, targetDepth, ans);
        }
    }
};

// template <typename T, typename U>
// using StringAggTree = AggTree<T, U, std::string, false>;

// template <typename T>
// using StringAggNode = AggNode<T, std::string, false>;

template <typename T, typename U>
using SIZETAggTree = AggTree<T, U, size_t, false>;

template <typename T, typename U>
using SIZETAggTreeWithSourceBuffer = AggTree<T, U, size_t, true>;

// template <typename T>
// using SIZETAggNode = AggNode<T, size_t, false>;

} // namespace logtail
