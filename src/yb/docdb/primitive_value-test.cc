// Copyright (c) YugaByte, Inc.

#include "yb/docdb/primitive_value.h"

#include <limits>
#include <map>

#include "yb/util/test_macros.h"
#include "yb/util/test_util.h"
#include "yb/util/bytes_formatter.h"
#include "yb/gutil/strings/substitute.h"

using std::map;
using std::string;
using std::numeric_limits;
using strings::Substitute;

namespace yb {
namespace docdb {

namespace {

void EncodeAndDecode(const PrimitiveValue& primitive_value) {
  KeyBytes key_bytes = primitive_value.ToKeyBytes();
  PrimitiveValue decoded;
  rocksdb::Slice slice = key_bytes.AsSlice();
  ASSERT_OK_PREPEND(
      decoded.DecodeFromKey(&slice),
      Substitute(
          "Could not decode key bytes obtained by encoding primitive value $0: $1",
          primitive_value.ToString(), key_bytes.ToString()));
  ASSERT_TRUE(slice.empty())
      << "Not all bytes consumed when encoding/decoding primitive value "
      << primitive_value.ToString() << ": "
      << slice.size() << " bytes left."
      << "Key bytes: " << key_bytes.ToString() << ".";
  ASSERT_EQ(primitive_value.ToString(), decoded.ToString())
      << "String representation of decoded value is different from that of the original value.";
}

void TestEncoding(const char* expected_str, const PrimitiveValue& primitive_value) {
  ASSERT_STR_EQ_VERBOSE_TRIMMED(expected_str, primitive_value.ToKeyBytes().ToString());
}

}  // unnamed namespace

TEST(PrimitiveValueTest, TestToString) {
  ASSERT_EQ("\"foo\"", PrimitiveValue("foo").ToString());
  ASSERT_EQ("\"foo\\\"\\x00\\x01\\x02\\\"bar\"",
      PrimitiveValue(string("foo\"\x00\x01\x02\"bar", 11)).ToString());

  ASSERT_EQ("123456789000", PrimitiveValue(123456789000l).ToString());
  ASSERT_EQ("-123456789000", PrimitiveValue(-123456789000l).ToString());
  ASSERT_EQ("9223372036854775807",
      PrimitiveValue(numeric_limits<int64_t>::max()).ToString());
  ASSERT_EQ("-9223372036854775808",
      PrimitiveValue(numeric_limits<int64_t>::min()).ToString());

  ASSERT_EQ("3.1415", PrimitiveValue::Double(3.1415).ToString());
  ASSERT_EQ("100.0", PrimitiveValue::Double(100.0).ToString());
  ASSERT_EQ("1.000000E-100", PrimitiveValue::Double(1e-100).ToString());

  ASSERT_EQ("ArrayIndex(123)", PrimitiveValue::ArrayIndex(123).ToString());
  ASSERT_EQ("ArrayIndex(-123)", PrimitiveValue::ArrayIndex(-123).ToString());

  ASSERT_EQ("HT(1002003004005006007)",
      PrimitiveValue(HybridTime(1002003004005006007L)).ToString());

  // HybridTimes use an unsigned 64-bit integer as an internal representation.
  ASSERT_EQ("HT(0)", PrimitiveValue(HybridTime(0)).ToString());
  ASSERT_EQ("HT(Max)", PrimitiveValue(HybridTime(numeric_limits<uint64_t>::max())).ToString());
  ASSERT_EQ("HT(Max)", PrimitiveValue(HybridTime(-1)).ToString());

  ASSERT_EQ("UInt16Hash(65535)",
            PrimitiveValue::UInt16Hash(numeric_limits<uint16_t>::max()).ToString());
  ASSERT_EQ("UInt16Hash(65535)", PrimitiveValue::UInt16Hash(-1).ToString());
  ASSERT_EQ("UInt16Hash(0)", PrimitiveValue::UInt16Hash(0).ToString());

  ASSERT_EQ("ColumnId(2147483647)",
            PrimitiveValue(ColumnId(numeric_limits<int32_t>::max())).ToString());
  ASSERT_EQ("ColumnId(0)",
            PrimitiveValue(ColumnId(0)).ToString());

  ASSERT_EQ("SystemColumnId(2147483647)",
            PrimitiveValue::SystemColumnId(ColumnId(numeric_limits<int32_t>::max())).ToString());
  ASSERT_EQ("SystemColumnId(0)",
            PrimitiveValue::SystemColumnId(ColumnId(0)).ToString());

  // Negative column ids are not allowed.
  EXPECT_EXIT(ColumnId(-1), ::testing::KilledBySignal(SIGABRT), "Check failed.*");

  ColumnId col;
  EXPECT_EXIT({col = static_cast<ColumnIdRep>(-1);}, ::testing::KilledBySignal(SIGABRT),
              "Check failed.*");
}

TEST(PrimitiveValueTest, TestRoundTrip) {
  for (auto primitive_value : {
      PrimitiveValue("foo"),
      PrimitiveValue(string("foo\0bar\x01", 8)),
      PrimitiveValue(123L),
      PrimitiveValue(HybridTime(1000L)),
      PrimitiveValue(ColumnId(numeric_limits<ColumnIdRep>::max())),
      PrimitiveValue(ColumnId(0)),
      PrimitiveValue::SystemColumnId(ColumnId(numeric_limits<ColumnIdRep>::max())),
      PrimitiveValue::SystemColumnId(ColumnId(0)),
  }) {
    EncodeAndDecode(primitive_value);
  }
}

TEST(PrimitiveValueTest, TestEncoding) {
  TestEncoding(R"#("$foo\x00\x00")#", PrimitiveValue("foo"));
  TestEncoding(R"#("$foo\x00\x01bar\x01\x00\x00")#", PrimitiveValue(string("foo\0bar\x01", 8)));
  TestEncoding(R"#("I\x80\x00\x00\x00\x00\x00\x00{")#", PrimitiveValue(123L));
  TestEncoding(R"#("I\x00\x00\x00\x00\x00\x00\x00\x00")#",
      PrimitiveValue(std::numeric_limits<int64_t>::min()));
  TestEncoding(R"#("I\xff\xff\xff\xff\xff\xff\xff\xff")#",
      PrimitiveValue(std::numeric_limits<int64_t>::max()));
}

TEST(PrimitiveValueTest, TestCompareStringsWithEmbeddedZeros) {
  const auto zero_char = PrimitiveValue(string("\x00", 1));
  const auto two_zero_chars = PrimitiveValue(string("\x00\x00", 2));

  ASSERT_EQ(zero_char, zero_char);
  ASSERT_EQ(two_zero_chars, two_zero_chars);

  ASSERT_LT(zero_char, two_zero_chars);
  ASSERT_GT(two_zero_chars, zero_char);
  ASSERT_NE(zero_char, two_zero_chars);
  ASSERT_NE(two_zero_chars, zero_char);

  ASSERT_FALSE(zero_char < zero_char);
  ASSERT_FALSE(zero_char > zero_char);
  ASSERT_FALSE(two_zero_chars < two_zero_chars);
  ASSERT_FALSE(two_zero_chars > two_zero_chars);
  ASSERT_FALSE(two_zero_chars < zero_char);
  ASSERT_FALSE(zero_char > two_zero_chars);
}


TEST(PrimitiveValueTest, TestPrimitiveValuesAsMapKeys) {
  map<PrimitiveValue, string> m;
  const PrimitiveValue key2("key2");
  const PrimitiveValue key1("key1");
  ASSERT_TRUE(m.emplace(key2, "value2").second);
  ASSERT_EQ(1, m.count(key2));
  ASSERT_NE(m.find(key2), m.end());
  ASSERT_TRUE(m.emplace(key1, "value1").second);
  ASSERT_EQ(1, m.count(key1));
  ASSERT_NE(m.find(key1), m.end());
}

TEST(PrimitiveValueTest, TestCorruption) {
  // No column id specified.
  KeyBytes key_bytes;
  key_bytes.AppendValueType(ValueType::kColumnId);
  rocksdb::Slice slice = key_bytes.AsSlice();
  PrimitiveValue decoded;
  ASSERT_TRUE(decoded.DecodeFromKey(&slice).IsCorruption());

  // Invalid varint.
  key_bytes.AppendInt64(std::numeric_limits<int64_t>::max());
  ASSERT_TRUE(decoded.DecodeFromKey(&slice).IsCorruption());
}

TEST(PrimitiveValueTest, TestVarintStorage) {
  // Verify varint occupies the appropriate amount of bytes.
  KeyBytes key_bytes;
  key_bytes.AppendColumnId(ColumnId(127));
  ASSERT_EQ(1, key_bytes.AsSlice().size());

  // 2 bytes for > 127 (total 3 = 1 + 2)
  key_bytes.AppendColumnId(ColumnId(128));
  ASSERT_EQ(3, key_bytes.AsSlice().size());

  key_bytes.Clear();
  key_bytes.AppendColumnId(ColumnId(std::numeric_limits<int32_t>::max()));
  ASSERT_EQ(5, key_bytes.AsSlice().size());
}

TEST(PrimitiveValueTest, TestRandomComparableColumnId) {
  // Seed with current time.
  std::srand(std::time(0));
  for (int i = 0; i < 1000; i++) {
    ColumnIdRep column_id1 = std::rand() % (std::numeric_limits<ColumnIdRep>::max());
    ColumnIdRep column_id2 = std::rand() % (std::numeric_limits<ColumnIdRep>::max());
    KeyBytes key_bytes1;
    KeyBytes key_bytes2;
    key_bytes1.AppendColumnId(ColumnId(column_id1));
    key_bytes2.AppendColumnId(ColumnId(column_id2));
    rocksdb::Slice slice1 = key_bytes1.AsSlice();
    rocksdb::Slice slice2 = key_bytes2.AsSlice();
    if (column_id1 > column_id2) {
      ASSERT_LT(0, slice1.compare(slice2)) << strings::Substitute("Failed for values $0, $1",
                                                                  column_id1, column_id2);
    } else if (column_id1 < column_id2) {
      ASSERT_GT(0, slice1.compare(slice2)) << strings::Substitute("Failed for values $0, $1",
                                                                  column_id1, column_id2);
    } else {
      ASSERT_EQ(0, slice1.compare(slice2)) << strings::Substitute("Failed for values $0, $1",
                                                                  column_id1, column_id2);
    }
  }
}

}  // namespace docdb
}  // namespace yb
