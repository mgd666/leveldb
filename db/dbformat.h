// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_DB_DBFORMAT_H_
#define STORAGE_LEVELDB_DB_DBFORMAT_H_

#include <cstddef>
#include <cstdint>
#include <string>

#include "leveldb/comparator.h"
#include "leveldb/db.h"
#include "leveldb/filter_policy.h"
#include "leveldb/slice.h"
#include "leveldb/table_builder.h"
#include "util/coding.h"
#include "util/logging.h"

namespace leveldb {

// Grouping of constants.  We may want to make some of these
// parameters set via options.
namespace config {
static const int kNumLevels = 7;

// Level-0 compaction is started when we hit this many files.
static const int kL0_CompactionTrigger = 4;

// Soft limit on number of level-0 files.  We slow down writes at this point.
static const int kL0_SlowdownWritesTrigger = 8;

// Maximum number of level-0 files.  We stop writes at this point.
static const int kL0_StopWritesTrigger = 12;

// Maximum level to which a new compacted memtable is pushed if it
// does not create overlap.  We try to push to level 2 to avoid the
// relatively expensive level 0=>1 compactions and to avoid some
// expensive manifest file operations.  We do not push all the way to
// the largest level since that can generate a lot of wasted disk
// space if the same key space is being repeatedly overwritten.
static const int kMaxMemCompactLevel = 2;

// Approximate gap in bytes between samples of data read during iteration.
static const int kReadBytesPeriod = 1048576;

}  // namespace config

class InternalKey;

// Value types encoded as the last component of internal keys.
// DO NOT CHANGE THESE ENUM VALUES: they are embedded in the on-disk
// data structures.
enum ValueType { 
  kTypeDeletion = 0x0, //删除
  kTypeValue = 0x1     //数据
};
// kValueTypeForSeek defines the ValueType that should be passed when
// constructing a ParsedInternalKey object for seeking to a particular
// sequence number (since we sort sequence numbers in decreasing order
// and the value type is embedded as the low 8 bits in the sequence
// number in internal keys, we need to use the highest-numbered
// ValueType, not the lowest).

// 用于查找（在查找对象时，对象不能删除，因此kValueType是KTypeValue
static const ValueType kValueTypeForSeek = kTypeValue;

/**
SequenceNumber 是一个无符号64位整形的值，leveldb每添加/修改一次记录都会触发其+1
*/
typedef uint64_t SequenceNumber;

// We leave eight bits empty at the bottom so a type and sequence#
// can be packed together into 64-bits.

/**
0x1ull:u-unsigned 无符号；l-long 长整型
ll就是64位整型。整个0x1ull代表的含义是无符号64位整型常量1，用16进制表示
*/
static const SequenceNumber kMaxSequenceNumber = ((0x1ull << 56) - 1);
/**
用二进制表示就是：0000 0000 1111 1111 1111 1111 1111 1111
如果seq大于kMaxSequenceNumber，左移8位的话会移出
（看dbformat.cc的PackSequenceAndType函数）
*/

struct ParsedInternalKey {
  Slice user_key;
  SequenceNumber sequence;
  ValueType type;

  ParsedInternalKey() {}  // Intentionally left uninitialized (for speed)
  ParsedInternalKey(const Slice& u, const SequenceNumber& seq, ValueType t)
      : user_key(u), sequence(seq), type(t) {}
  std::string DebugString() const;
};

// Return the length of the encoding of "key".
inline size_t InternalKeyEncodingLength(const ParsedInternalKey& key) {
  return key.user_key.size() + 8;
}

// Append the serialization of "key" to *result.
void AppendInternalKey(std::string* result, const ParsedInternalKey& key);

// Attempt to parse an internal key from "internal_key".  On success,
// stores the parsed data in "*result", and returns true.
//
// On error, returns false, leaves "*result" in an undefined state.
bool ParseInternalKey(const Slice& internal_key, ParsedInternalKey* result);

// Returns the user key portion of an internal key.
// 提取user_key（除去8位后的都是user_key)
inline Slice ExtractUserKey(const Slice& internal_key) {
  assert(internal_key.size() >= 8);
  return Slice(internal_key.data(), internal_key.size() - 8);
}

// A comparator for internal keys that uses a specified comparator for
// the user key portion and breaks ties by decreasing sequence number.
class InternalKeyComparator : public Comparator {
 private:
  const Comparator* user_comparator_;

 public:
  explicit InternalKeyComparator(const Comparator* c) : user_comparator_(c) {}
  const char* Name() const override;
  int Compare(const Slice& a, const Slice& b) const override;
  void FindShortestSeparator(std::string* start,
                             const Slice& limit) const override;
  void FindShortSuccessor(std::string* key) const override;

  const Comparator* user_comparator() const { return user_comparator_; }

  int Compare(const InternalKey& a, const InternalKey& b) const;
};

// Filter policy wrapper that converts from internal keys to user keys
class InternalFilterPolicy : public FilterPolicy {
 private:
  const FilterPolicy* const user_policy_;

 public:
  explicit InternalFilterPolicy(const FilterPolicy* p) : user_policy_(p) {}
  const char* Name() const override;
  void CreateFilter(const Slice* keys, int n, std::string* dst) const override;
  bool KeyMayMatch(const Slice& key, const Slice& filter) const override;
};

// Modules in this directory should keep internal keys wrapped inside
// the following class instead of plain strings so that we do not
// incorrectly use string comparisons instead of an InternalKeyComparator.
class InternalKey {
 private:
  // 把Slice变成了string类型
  std::string rep_;

 public:
  InternalKey() {}  // Leave rep_ as empty to indicate it is invalid
  // 可以看到内部键是如何被构造的
  // Slice（外部键），顺序号，值类型
  InternalKey(const Slice& user_key, SequenceNumber s, ValueType t) {
    // 内部键生成函数
    AppendInternalKey(&rep_, ParsedInternalKey(user_key, s, t));
  }

  // 解码内部键函数
  // 注意：传入的slice并不是User_key(外部键),而是InternalKey.Encode()编码而来的
  bool DecodeFrom(const Slice& s) {
    // 直接拷贝到rep_里
    rep_.assign(s.data(), s.size());
    return !rep_.empty();
  }
  // 把内部键编码成Slice格式，其实就是用Slice封装一下
  Slice Encode() const {
    assert(!rep_.empty());
    // 这里直接返回的stirng，
    return rep_;
  }

  Slice user_key() const { 
    return ExtractUserKey(rep_); 
  }

  void SetFrom(const ParsedInternalKey& p) {
    rep_.clear();
    AppendInternalKey(&rep_, p);
  }

  void Clear() { rep_.clear(); }

  std::string DebugString() const;
};

inline int InternalKeyComparator::Compare(const InternalKey& a,
                                          const InternalKey& b) const {
  return Compare(a.Encode(), b.Encode());
}

inline bool ParseInternalKey(const Slice& internal_key,
                             ParsedInternalKey* result) {
  const size_t n = internal_key.size();
  if (n < 8) return false;
  uint64_t num = DecodeFixed64(internal_key.data() + n - 8);
  uint8_t c = num & 0xff;
  result->sequence = num >> 8;
  result->type = static_cast<ValueType>(c);
  result->user_key = Slice(internal_key.data(), n - 8);
  return (c <= static_cast<uint8_t>(kTypeValue));
}

// A helper class useful for DBImpl::Get()
// 因为查找可能需要查找memtable和sst，因此需要存储的内容要包含多种存储结构的键值
class LookupKey {
 public:
  // Initialize *this for looking up user_key at a snapshot with
  // the specified sequence number.
  LookupKey(const Slice& user_key, SequenceNumber sequence);

  LookupKey(const LookupKey&) = delete;
  LookupKey& operator=(const LookupKey&) = delete;

  ~LookupKey();

  // Return a key suitable for lookup in a MemTable.
  Slice memtable_key() const { return Slice(start_, end_ - start_); }

  // Return an internal key (suitable for passing to an internal iterator)
  Slice internal_key() const { return Slice(kstart_, end_ - kstart_); }

  // Return the user key
  Slice user_key() const { return Slice(kstart_, end_ - kstart_ - 8); }

 private:
  // We construct a char array of the form:
  //    klength  varint32               <-- start_
  //    userkey  char[klength]          <-- kstart_
  //    tag      uint64
  //                                    <-- end_
  // The array is a suitable MemTable key.
  // The suffix starting with "userkey" can be used as an InternalKey.
  const char* start_;
  const char* kstart_;
  const char* end_;
  char space_[200];  // Avoid allocation for short keys
};

// 析构函数，因为有申请内存的可能性
inline LookupKey::~LookupKey() {
    // 判断内存是否为堆上申请的，如果是就释放
  if (start_ != space_) delete[] start_;
}

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_DBFORMAT_H_
