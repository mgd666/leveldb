// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "util/arena.h"

namespace leveldb {

// 初始化大小，4KB
static const int kBlockSize = 4096;

Arena::Arena()
    : alloc_ptr_(nullptr), alloc_bytes_remaining_(0), memory_usage_(0) {}

Arena::~Arena() {
  for (size_t i = 0; i < blocks_.size(); i++) {
    delete[] blocks_[i];
  }
}

// Step 2
char* Arena::AllocateFallback(size_t bytes) {
  /**
  大于1/4，直接申请，但是alloc_ptr_和alloc_bytes_remaining_都不变
  但是blocks_里会存入指向这一块新申请内存区域的指针（这个操作在AllocateNewBlock中）
  */
  if (bytes > kBlockSize / 4) {
    // Object is more than a quarter of our block size.  Allocate it separately
    // to avoid wasting too much space in leftover bytes.
    char* result = AllocateNewBlock(bytes);
    return result;
  }

  // We waste the remaining space in the current block.
  alloc_ptr_ = AllocateNewBlock(kBlockSize);
  alloc_bytes_remaining_ = kBlockSize;

  char* result = alloc_ptr_;
  alloc_ptr_ += bytes;
  alloc_bytes_remaining_ -= bytes;
  return result;
}

// 对齐分配
/**
因为SkipList中会涉及一些原子操作，所以AllocateAligned分配的内存
需要和指针的大小（一般是8字节）对齐
其余逻辑都是相同的
对齐：分配内存的起始地址是指针大小的倍数
*/
char* Arena::AllocateAligned(size_t bytes) {
  // void*的大小是和编译目标平台的内存空间相关，这里确保不论你是32位系统还是64位系统都是按8字节对齐的
  const int align = (sizeof(void*) > 8) ? sizeof(void*) : 8;
  // 判断align是不是2的n次幂
  static_assert((align & (align - 1)) == 0,
                "Pointer size should be a power of 2");
                /**
                数学计算：取模运算（位运算）x % (2^n) === x & (2^(n-1))
                除法比较费时，在高性能计算中需要转换成位运算
                */
                //将alloc_ptr_强转成无负号整形，并计算alloc_ptr_ % align
  size_t current_mod = reinterpret_cast<uintptr_t>(alloc_ptr_) & (align - 1);
  // 计算内存对齐还需要向前移动的大小（这些就是内存碎片）
  size_t slop = (current_mod == 0 ? 0 : align - current_mod);
  // 总需要的就是申请的+内存碎片
  size_t needed = bytes + slop;
  char* result;
  if (needed <= alloc_bytes_remaining_) {
    result = alloc_ptr_ + slop;
    alloc_ptr_ += needed;
    alloc_bytes_remaining_ -= needed;
  } else {
    // AllocateFallback always returned aligned memory
    result = AllocateFallback(bytes);
  }
  assert((reinterpret_cast<uintptr_t>(result) & (align - 1)) == 0);
  return result;
}

// Step 3
char* Arena::AllocateNewBlock(size_t block_bytes) {
  char* result = new char[block_bytes];
  // blocks_中存入指向内存块的指针
  blocks_.push_back(result);
  /**
  memory_usages_为整个内存池使用内存的总大小（注意：除去alloc_ptr_和alloc_bytes_remaining_数据成员的大小）
  add的是要分配的字节，以及一个指针的大小（vector中存的是指针）
  TODO: std::memory_order_relaxed(有关一致性的，后面看)
  */
  memory_usage_.fetch_add(block_bytes + sizeof(char*),
                          std::memory_order_relaxed);
  return result;
}

}  // namespace leveldb
