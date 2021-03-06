// Copyright (c) 2017 Google Inc.
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

// Contains utils for reading, writing and debug printing bit streams.

#include <map>
#include <sstream>
#include <string>
#include <unordered_map>

#include "util/huffman_codec.h"
#include "util/bit_stream.h"
#include "gmock/gmock.h"

namespace {

using spvutils::HuffmanCodec;
using spvutils::BitsToStream;

const std::map<std::string, uint32_t>& GetTestSet() {
  static const std::map<std::string, uint32_t> hist = {
    {"a", 4},
    {"e", 7},
    {"f", 3},
    {"h", 2},
    {"i", 3},
    {"m", 2},
    {"n", 2},
    {"s", 2},
    {"t", 2},
    {"l", 1},
    {"o", 2},
    {"p", 1},
    {"r", 1},
    {"u", 1},
    {"x", 1},
  };

  return hist;
}

class TestBitReader {
 public:
  TestBitReader(const std::string& bits) : bits_(bits) {}

  bool ReadBit(bool* bit) {
    if (pos_ < bits_.length()) {
      *bit = bits_[pos_++] == '0' ? false : true;
      return true;
    }
    return false;
  }

 private:
  std::string bits_;
  size_t pos_ = 0;
};

TEST(Huffman, PrintTree) {
  HuffmanCodec<std::string> huffman(GetTestSet());
  std::stringstream ss;
  huffman.PrintTree(ss);

  const std::string expected = std::string(R"(
15-----7------e
       8------4------a
              4------2------m
                     2------n
19-----8------4------2------o
                     2------s
              4------2------t
                     2------1------l
                            1------p
       11-----5------2------1------r
                            1------u
                     3------f
              6------3------i
                     3------1------x
                            2------h
)").substr(1);

  EXPECT_EQ(expected, ss.str());
}

TEST(Huffman, PrintTable) {
  HuffmanCodec<std::string> huffman(GetTestSet());
  std::stringstream ss;
  huffman.PrintTable(ss);

  const std::string expected = std::string(R"(
e 7 11
a 4 101
i 3 0001
f 3 0010
t 2 0101
s 2 0110
o 2 0111
n 2 1000
m 2 1001
h 2 00000
x 1 00001
u 1 00110
r 1 00111
p 1 01000
l 1 01001
)").substr(1);

  EXPECT_EQ(expected, ss.str());
}

TEST(Huffman, TestValidity) {
  HuffmanCodec<std::string> huffman(GetTestSet());
  const auto& encoding_table = huffman.GetEncodingTable();
  std::vector<std::string> codes;
  for (const auto& entry : encoding_table) {
    codes.push_back(BitsToStream(entry.second.first, entry.second.second));
  }

  std::sort(codes.begin(), codes.end());

  ASSERT_LT(codes.size(), 20u) << "Inefficient test ahead";

  for (size_t i = 0; i < codes.size(); ++i) {
    for (size_t j = i + 1; j < codes.size(); ++j) {
      ASSERT_FALSE(codes[i] == codes[j].substr(0, codes[i].length()))
          << codes[i] << " is prefix of " << codes[j];
    }
  }
}

TEST(Huffman, TestEncode) {
  HuffmanCodec<std::string> huffman(GetTestSet());

  uint64_t bits = 0;
  size_t num_bits = 0;

  EXPECT_TRUE(huffman.Encode("e", &bits, &num_bits));
  EXPECT_EQ(2u, num_bits);
  EXPECT_EQ("11", BitsToStream(bits, num_bits));

  EXPECT_TRUE(huffman.Encode("a", &bits, &num_bits));
  EXPECT_EQ(3u, num_bits);
  EXPECT_EQ("101", BitsToStream(bits, num_bits));

  EXPECT_TRUE(huffman.Encode("x", &bits, &num_bits));
  EXPECT_EQ(5u, num_bits);
  EXPECT_EQ("00001", BitsToStream(bits, num_bits));

  EXPECT_FALSE(huffman.Encode("y", &bits, &num_bits));
}

TEST(Huffman, TestDecode) {
  HuffmanCodec<std::string> huffman(GetTestSet());
  TestBitReader bit_reader("01001""0001""1000""00110""00001""00");
  auto read_bit = [&bit_reader](bool* bit) {
    return bit_reader.ReadBit(bit);
  };

  std::string decoded;

  ASSERT_TRUE(huffman.DecodeFromStream(read_bit, &decoded));
  EXPECT_EQ("l", decoded);

  ASSERT_TRUE(huffman.DecodeFromStream(read_bit, &decoded));
  EXPECT_EQ("i", decoded);

  ASSERT_TRUE(huffman.DecodeFromStream(read_bit, &decoded));
  EXPECT_EQ("n", decoded);

  ASSERT_TRUE(huffman.DecodeFromStream(read_bit, &decoded));
  EXPECT_EQ("u", decoded);

  ASSERT_TRUE(huffman.DecodeFromStream(read_bit, &decoded));
  EXPECT_EQ("x", decoded);

  ASSERT_FALSE(huffman.DecodeFromStream(read_bit, &decoded));
}

TEST(Huffman, TestDecodeNumbers) {
  const std::map<uint32_t, uint32_t> hist = { {1, 10}, {2, 5}, {3, 15} };
  HuffmanCodec<uint32_t> huffman(hist);

  TestBitReader bit_reader("1""1""01""00""01""1");
  auto read_bit = [&bit_reader](bool* bit) {
    return bit_reader.ReadBit(bit);
  };

  uint32_t decoded;

  ASSERT_TRUE(huffman.DecodeFromStream(read_bit, &decoded));
  EXPECT_EQ(3u, decoded);

  ASSERT_TRUE(huffman.DecodeFromStream(read_bit, &decoded));
  EXPECT_EQ(3u, decoded);

  ASSERT_TRUE(huffman.DecodeFromStream(read_bit, &decoded));
  EXPECT_EQ(2u, decoded);

  ASSERT_TRUE(huffman.DecodeFromStream(read_bit, &decoded));
  EXPECT_EQ(1u, decoded);

  ASSERT_TRUE(huffman.DecodeFromStream(read_bit, &decoded));
  EXPECT_EQ(2u, decoded);

  ASSERT_TRUE(huffman.DecodeFromStream(read_bit, &decoded));
  EXPECT_EQ(3u, decoded);
}

}  // anonymous namespace
