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

#include "common/strings/obfuscator.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "common/util/bijective_map.h"

namespace verible {
namespace {

static char Rot13(char c) {
  if ((c >= 'a' && c <= 'm') || (c >= 'A' && c <= 'M'))
    return c + 13;
  else if ((c >= 'n' && c <= 'z') || (c >= 'N' && c <= 'Z'))
    return c - 13;
  else
    return c;
}

// Non-random generator, just for the sake of testing.
static std::string RotateGenerator(absl::string_view input) {
  std::string s(input);
  for (auto& ch : s) ch = Rot13(ch);
  return s;
}

TEST(ObfuscatorTest, Construction) {
  Obfuscator ob(RotateGenerator);
  EXPECT_TRUE(ob.GetTranslator().empty());
}

TEST(ObfuscatorTest, Transform) {
  Obfuscator ob(RotateGenerator);
  const auto& tran = ob.GetTranslator();
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
  const auto& tran = ob.GetTranslator();
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

TEST(IdentifierObfuscatorTest, Transform) {
  IdentifierObfuscator ob;
  const auto& tran = ob.GetTranslator();
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
  IdentifierObfuscator ob;
  EXPECT_DEATH(ob.encode("cat", "sheep"), "");  // mismatch length
}

TEST(IdentifierObfuscatorTest, EncodeValidTransform) {
  IdentifierObfuscator ob;
  ob.encode("cat", "cow");
  const auto& tran = ob.GetTranslator();
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
