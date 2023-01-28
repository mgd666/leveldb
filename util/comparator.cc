// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "leveldb/comparator.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <type_traits>

#include "leveldb/slice.h"
#include "util/logging.h"
#include "util/no_destructor.h"

namespace leveldb {

Comparator::~Comparator() = default;

namespace {
class BytewiseComparatorImpl : public Comparator {
 public:
  BytewiseComparatorImpl() = default;
  // 实现获取接口的名字
  const char* Name() const override { return "leveldb.BytewiseComparator"; }

  // 调用的是Slice里自带的compare函数
  // Slice里的compare函数就是使用memcmp函数，按照字节进行比较的
  int Compare(const Slice& a, const Slice& b) const override {
    return a.compare(b);
  }

  void FindShortestSeparator(std::string* start,
                             const Slice& limit) const override {
    // Find length of common prefix
    // 取二者最短的长度，防止越界
    size_t min_length = std::min(start->size(), limit.size());
    size_t diff_index = 0;
    /**
     * 如果是相同则一直找，找到第一个和limit不同的字节的位置
    */
    while ((diff_index < min_length) &&
           ((*start)[diff_index] == limit[diff_index])) {
      diff_index++;
    }

    if (diff_index >= min_length) {
      // Do not shorten if one string is a prefix of the other
    } else {
      // 取出这个字节，并强转成uint8类型
      uint8_t diff_byte = static_cast<uint8_t>((*start)[diff_index]);
      // 不能是0xff，0xff有特殊意义
      // 同时+1后也要小于limit的相应字符
      if (diff_byte < static_cast<uint8_t>(0xff) &&
          diff_byte + 1 < static_cast<uint8_t>(limit[diff_index])) {
        // 将较小的字符串的最后一位+1，并去掉后面的字符
        (*start)[diff_index]++;
        // 就是[*start,limit)区间最短的字符串了
        start->resize(diff_index + 1);
        assert(Compare(*start, limit) < 0);
      }
    }
  }

  /**
   * 所谓在逻辑上变大了，在实际上变小了
   * 传入的是abcg和abe ===> abd，abe
   * 逻辑上变大：字符串最后的字母+1了，变成了abd
   * 实际上变小：字符串长度变短了
  */

  // 找到第一个比*key大的最短字符串
  void FindShortSuccessor(std::string* key) const override {
    // Find first character that can be incremented
    // 获取键的长度
    size_t n = key->size();
    // 遍历键的每个字节
    for (size_t i = 0; i < n; i++) {
      const uint8_t byte = (*key)[i];
      // 找到第一个不是0xff的字节
      if (byte != static_cast<uint8_t>(0xff)) {
        // 把这个位置的字节+1，然后截断
        (*key)[i] = byte + 1;
        key->resize(i + 1);
        return;
      }
    }
    // *key is a run of 0xffs.  Leave it alone.
  }

  
};
}  // namespace

const Comparator* BytewiseComparator() {
  static NoDestructor<BytewiseComparatorImpl> singleton;
  return singleton.get();
}

}  // namespace leveldb
