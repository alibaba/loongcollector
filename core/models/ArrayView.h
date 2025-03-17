// Copyright 2023 iLogtail Authors
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

#include <array>

namespace logtail {
template <class T>
class ArrayView {
private:
    const T* const mElements;
    const size_t mSize;

public:
    constexpr ArrayView() : mElements(nullptr), mSize(0) {}
    template <std::size_t N>
    constexpr ArrayView(const T (&a)[N]) : mElements(a), mSize(N) {}
    constexpr ArrayView(const T* ptr, size_t size) : mElements(ptr), mSize(size) {}
    template <std::size_t N>
    constexpr ArrayView(const std::array<T, N>& arr) : mElements(arr.data()), mSize(arr.size()) {}

    constexpr size_t size() const { return mSize; }
    constexpr const T& operator[](size_t i) const { return mElements[i]; }

    class iterator {
    public:
        iterator(const T* ptr) : ptr(ptr) {}
        iterator operator++() {
            ++ptr;
            return *this;
        }
        bool operator!=(const iterator& other) const { return ptr != other.ptr; }
        const T& operator*() const { return *ptr; }

    private:
        const T* ptr;
    };
    iterator begin() const { return iterator(mElements); }
    iterator end() const { return iterator(mElements + mSize); }
};

} // namespace logtail
