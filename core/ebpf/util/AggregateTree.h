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

#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <functional>

namespace logtail {

template<class Data, class Value, class KeyType>
class AggTree;

template<class Data, class KeyType>
class AggNode {
public:
    std::unordered_map<KeyType, std::unique_ptr<AggNode>> child;
    std::unique_ptr<Data> data;
};

template<class Data, class Value, class KeyType>
class AggTree {
private:

    size_t max_nodes;

    size_t now_nodes = 0;

    std::unique_ptr<AggNode<Data, KeyType>> root_node_;

    std::function<void(std::unique_ptr<Data> &base, const Value &n)> aggregate_;

    std::function<std::unique_ptr<Data>(const Value &n)> generate_;
#ifdef APSARA_UNIT_TEST_MAIN
    friend class eBPFServerUnittest;
#endif
public:

    AggTree(size_t max_nodes,
            const std::function<void(std::unique_ptr<Data> &, const Value &)> &aggregate, 
            const std::function<std::unique_ptr<Data>(const Value &n)> &generate)
            : max_nodes(max_nodes), aggregate_(aggregate), generate_(generate),
              root_node_(std::make_unique<AggNode<Data, KeyType>>()) {}

    AggTree(AggTree<Data, Value, KeyType> &&other) noexcept
            : max_nodes(other.max_nodes),
              root_node_(std::move(other.root_node_)),
              aggregate_(other.aggregate_),
              generate_(other.generate_),
              now_nodes(other.now_nodes) { other.Clear(); }

    AggTree& operator=(AggTree&& other) noexcept {
        if (this != &other) {
            max_nodes = other.max_nodes;
            root_node_ = std::move(other.root_node_);
            aggregate_ = std::move(other.aggregate_);
            generate_ = std::move(other.generate_);
            now_nodes = other.now_nodes;
        }
        return *this;
    }

    template<class ContainerType>
    bool Aggregate(const Value &d, const ContainerType &agg_keys) {
        auto p = root_node_.get();
        for (auto &val: agg_keys) {
            auto result = p->child.find(val);
            if (result == p->child.end() && now_nodes >= max_nodes) {
                // when we exceed the maximum limit, we will drop new metrics
                return false;
            }
            if (result == p->child.end()) {
                auto new_node = std::make_unique<AggNode<Data, KeyType>>();
                auto ptr = new_node.get();
                p->child[val] = std::move(new_node);
                p = ptr;
                now_nodes++;
            } else {
                p = result->second.get();
            }
        }
        if (!p->data) {
            // generate new node ... 
            p->data = generate_(d);
        }
        aggregate_(p->data, d);
        return true;
    }

    std::vector<AggNode<Data, KeyType> *>
    GetNodesWithAggDepth(size_t i) {
        std::vector<AggNode<Data, KeyType> *> ans;
        GetNodes(1, root_node_, i, ans);
        return ans;
    }


    void ForEach(const std::function<void(const Data *)> &call) {
        ForEach(root_node_.get(), call);
    }

    void Clear() {
        root_node_ = std::make_unique<AggNode<Data, KeyType>>();
        now_nodes = 0;
    }

    void ForEach(const AggNode<Data, KeyType> *root,
                 const std::function<void(const Data *)> &call) {
        if (root == nullptr) {
            return;
        }
        if (root->data != nullptr) {
            call(root->data.get());
        }
        for (auto &entry: root->child) {
            ForEach(entry.second.get(), call);
        }
    }

    [[nodiscard]] size_t NodeCount() const {
        return now_nodes;
    }

private:
    void GetNodes(size_t depth, const std::unique_ptr<AggNode<Data, KeyType>> &root, size_t target_depth,
                  std::vector<AggNode<Data, KeyType> *> &ans) {
        if (depth >= target_depth) {
            // we are done, add child to ans and return
            for (auto &c: root->child) {
                ans.push_back(c.second.get());
            }
            return;
        }
        // go to next depth
        for (auto &c: root->child) {
            GetNodes(depth + 1, c.second, target_depth, ans);
        }
    }

};

template<typename T, typename U>
using StringAggTree = AggTree<T, U, std::string>;

template<typename T>
using StringAggNode = AggNode<T, std::string>;

template<typename T, typename U>
using SIZETAggTree = AggTree<T, U, size_t>;

template<typename T>
using SIZETAggNode = AggNode<T, size_t>;

}
