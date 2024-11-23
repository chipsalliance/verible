// Copyright 2017-2020 The Verible Authors.
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

#include "verible/common/strings/obfuscator.h"

#include <string>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "verible/common/strings/random.h"
#include "verible/common/util/bijective-map.h"
#include "verible/common/util/logging.h"

namespace verible {
namespace {

static char Rot13(char c) {
  if ((c >= 'a' && c <= 'm') || (c >= 'A' && c <= 'M')) return c + 13;
  if ((c >= 'n' && c <= 'z') || (c >= 'N' && c <= 'Z')) return c - 13;
  return c;
}

// Non-random generator, just for the sake of testing.
static std::string RotateGenerator(absl::string_view input) {
  std::string s(input);
  for (auto &ch : s) ch = Rot13(ch);
  return s;
}

TEST(ObfuscatorTest, Construction) {
  Obfuscator ob(RotateGenerator);
  EXPECT_TRUE(ob.GetTranslator().empty());
}

TEST(ObfuscatorTest, Transform) {
  Obfuscator ob(RotateGenerator);
  const auto &tran = ob.GetTranslator();
  // repeat same string
  for (int i = 0; i < 2; ++i) {
    const auto str = ob("cat");
    EXPECT_EQ(str, "png");
    EXPECT_EQ(tran.size(), 1);
    EXPECT_EQ(*ABSL_DIE_IF_NULL(tran.find_forward("cat")), "png");
    EXPECT_EQ(*ABSL_DIE_IF_NULL(tran.find_reverse("png")), "cat");
  }
  for (int i = 0; i < 2; ++i) {
    const auto str = ob("Dog");
    EXPECT_EQ(str, "Qbt");
    EXPECT_EQ(tran.size(), 2);
    EXPECT_EQ(*ABSL_DIE_IF_NULL(tran.find_forward("cat")), "png");
    EXPECT_EQ(*ABSL_DIE_IF_NULL(tran.find_reverse("png")), "cat");
    EXPECT_EQ(*ABSL_DIE_IF_NULL(tran.find_forward("Dog")), "Qbt");
    EXPECT_EQ(*ABSL_DIE_IF_NULL(tran.find_reverse("Qbt")), "Dog");
  }
}

TEST(ObfuscatorTest, Encode) {
  Obfuscator ob(RotateGenerator);
  ob.encode("cat", "sheep");
  const auto &tran = ob.GetTranslator();
  EXPECT_EQ(tran.size(), 1);
  EXPECT_EQ(*ABSL_DIE_IF_NULL(tran.find_forward("cat")), "sheep");
  EXPECT_EQ(*ABSL_DIE_IF_NULL(tran.find_reverse("sheep")), "cat");
  // repeat same string
  for (int i = 0; i < 2; ++i) {
    const auto str = ob("cat");
    EXPECT_EQ(str, "sheep");
    EXPECT_EQ(tran.size(), 1);
    EXPECT_EQ(*ABSL_DIE_IF_NULL(tran.find_forward("cat")), "sheep");
    EXPECT_EQ(*ABSL_DIE_IF_NULL(tran.find_reverse("sheep")), "cat");
  }
  for (int i = 0; i < 2; ++i) {
    const auto str = ob("dog");
    EXPECT_EQ(str, "qbt");
    EXPECT_EQ(tran.size(), 2);
    EXPECT_EQ(*ABSL_DIE_IF_NULL(tran.find_forward("cat")), "sheep");
    EXPECT_EQ(*ABSL_DIE_IF_NULL(tran.find_reverse("sheep")), "cat");
    EXPECT_EQ(*ABSL_DIE_IF_NULL(tran.find_forward("dog")), "qbt");
    EXPECT_EQ(*ABSL_DIE_IF_NULL(tran.find_reverse("qbt")), "dog");
  }
}

TEST(ObfuscatorTest, DecodeMode) {
  Obfuscator ob(RotateGenerator);
  EXPECT_FALSE(ob.is_decoding());
  ob.set_decode_mode(true);
  EXPECT_TRUE(ob.is_decoding());
  ob.set_decode_mode(false);
  EXPECT_FALSE(ob.is_decoding());
}

TEST(ObfuscatorTest, DecodeModeDoesNotAddEntries) {
  Obfuscator ob(RotateGenerator);
  ob.set_decode_mode(true);
  EXPECT_TRUE(ob.is_decoding());
  EXPECT_TRUE(ob.GetTranslator().empty());
  EXPECT_EQ(ob("dog"), "dog");  // unrecognized id, unchanged
  EXPECT_TRUE(ob.GetTranslator().empty());

  // can still manually add entries
  EXPECT_TRUE(ob.encode("shark", "snake"));
  EXPECT_EQ(ob.GetTranslator().size(), 1);
  EXPECT_EQ(ob("snake"), "shark");  // decoded
  EXPECT_EQ(ob.GetTranslator().size(), 1);
  EXPECT_EQ(ob("cow"), "cow");
  EXPECT_EQ(ob.GetTranslator().size(), 1);
}

TEST(ObfuscatorTest, SaveMap) {
  Obfuscator ob(RotateGenerator);
  EXPECT_EQ(ob.save(), "");
  EXPECT_EQ(ob("cat"), "png");
  EXPECT_EQ(ob.save(), "cat png\n");
  EXPECT_EQ(ob("zzz"), "mmm");
  EXPECT_EQ(ob.save(), "cat png\nzzz mmm\n");
}

TEST(ObfuscatorTest, LoadMap) {
  {
    Obfuscator ob(RotateGenerator);
    const auto status = ob.load("");
    EXPECT_TRUE(status.ok()) << status.message();
  }
  {
    Obfuscator ob(RotateGenerator);
    const auto status = ob.load("cat dog");
    EXPECT_TRUE(status.ok()) << status.message();
    EXPECT_EQ(ob("cat"), "dog");
  }
  {
    Obfuscator ob(RotateGenerator);
    const auto status = ob.load("cat dog\n");
    EXPECT_TRUE(status.ok()) << status.message();
    EXPECT_EQ(ob("cat"), "dog");
  }
  {
    Obfuscator ob(RotateGenerator);
    const auto status = ob.load("  cat dog  \r\n");
    EXPECT_TRUE(status.ok()) << status.message();
    EXPECT_EQ(ob("cat"), "dog");
  }
  {
    Obfuscator ob(RotateGenerator);
    const auto status = ob.load("cat\n");  // malformed line
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  }
}

TEST(IdentifierObfuscatorTest, Transform) {
  IdentifierObfuscator ob(RandomEqualLengthIdentifier);
  const auto &tran = ob.GetTranslator();
  // repeat same string
  for (int i = 0; i < 2; ++i) {
    const auto str = ob("cat");  // str is random
    EXPECT_EQ(tran.size(), 1);
    EXPECT_EQ(*ABSL_DIE_IF_NULL(tran.find_forward("cat")), str);
    EXPECT_EQ(*ABSL_DIE_IF_NULL(tran.find_reverse(std::string(str))), "cat");
  }
  for (int i = 0; i < 2; ++i) {
    const auto str = ob("dog");  // str is random
    EXPECT_EQ(tran.size(), 2);
    EXPECT_EQ(*ABSL_DIE_IF_NULL(tran.find_forward("dog")), str);
    EXPECT_EQ(*ABSL_DIE_IF_NULL(tran.find_reverse(std::string(str))), "dog");
  }
  for (int i = 0; i < 2; ++i) {
    const auto str = ob("cat");  // str is random
    EXPECT_EQ(tran.size(), 2);
    EXPECT_EQ(*ABSL_DIE_IF_NULL(tran.find_forward("cat")), str);
    EXPECT_EQ(*ABSL_DIE_IF_NULL(tran.find_reverse(std::string(str))), "cat");
  }
}

TEST(IdentifierObfuscatorTest, EncodeInvalid) {
  IdentifierObfuscator ob(RandomEqualLengthIdentifier);
  EXPECT_DEATH(ob.encode("cat", "sheep"), "");  // mismatch length
}

TEST(IdentifierObfuscatorTest, EncodeValidTransform) {
  IdentifierObfuscator ob(RandomEqualLengthIdentifier);
  ob.encode("cat", "cow");
  const auto &tran = ob.GetTranslator();
  // repeat same string
  for (int i = 0; i < 2; ++i) {
    const auto str = ob("cat");
    EXPECT_EQ(str, "cow");
    EXPECT_EQ(tran.size(), 1);
    EXPECT_EQ(*ABSL_DIE_IF_NULL(tran.find_forward("cat")), str);
    EXPECT_EQ(*ABSL_DIE_IF_NULL(tran.find_reverse(std::string(str))), "cat");
  }
  for (int i = 0; i < 2; ++i) {
    const auto str = ob("Dog");  // str is random
    EXPECT_EQ(tran.size(), 2);
    EXPECT_EQ(*ABSL_DIE_IF_NULL(tran.find_forward("Dog")), str);
    EXPECT_EQ(*ABSL_DIE_IF_NULL(tran.find_reverse(std::string(str))), "Dog");
  }
  for (int i = 0; i < 2; ++i) {
    const auto str = ob("cat");
    EXPECT_EQ(str, "cow");
    EXPECT_EQ(tran.size(), 2);
    EXPECT_EQ(*ABSL_DIE_IF_NULL(tran.find_forward("cat")), str);
    EXPECT_EQ(*ABSL_DIE_IF_NULL(tran.find_reverse(std::string(str))), "cat");
  }
}

}  // namespace
}  // namespace verible
