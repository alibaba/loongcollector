#pragma once

namespace logtail {
template <class T>
class ArrayView {
 private:
  const T* const elements_;
  const size_t size_;

 public:
  constexpr ArrayView() : elements_(nullptr), size_(0) {}
  template <std::size_t N>
  constexpr ArrayView(const T (&a)[N]) : elements_(a), size_(N) {}
  constexpr ArrayView(const T* ptr, size_t size) : elements_(ptr), size_(size) {}
  template <std::size_t N>
  constexpr ArrayView(const std::array<T, N>& arr) : elements_(arr.data()), size_(arr.size()) {}

  constexpr size_t size() const { return size_; }
  constexpr const T& operator[](size_t i) const { return elements_[i]; }

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
  iterator begin() const { return iterator(elements_); }
  iterator end() const { return iterator(elements_ + size_); }
};

}
