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

#include "verilog/tools/kythe/indexing_facts_tree_extractor.h"

#include "common/text/concrete_syntax_tree.h"
#include "gtest/gtest.h"
#include "verilog/analysis/verilog_analyzer.h"

#undef EXPECT_OK
#define EXPECT_OK(value) EXPECT_TRUE((value).ok())

namespace verilog {
namespace kythe {
namespace {

typedef IndexingFactNode T;

TEST(EqualOperatorTest, FactsTreeExtractor) {
  constexpr absl::string_view code_text = "";
  constexpr absl::string_view file_name = "verilog.v";

  const auto expected =
      T({{Anchor(file_name, 0, code_text.size())}, IndexingFactType ::kFile});

  const auto tree =
      T({{Anchor(file_name, 0, code_text.size())}, IndexingFactType ::kFile});

  const auto tree2 = T({{Anchor(file_name, 0, 556)}, IndexingFactType ::kFile});

  const auto tree3 = T({{Anchor(file_name, 0, 4589), Anchor(file_name, 0, 987)},
                        IndexingFactType ::kFile});

  const auto tree4 =
      T({{Anchor(file_name, 0, code_text.size())}, IndexingFactType ::kFile},
        T({{Anchor(absl::string_view("foo"), 7, 10),
            Anchor(absl::string_view("foo"), 23, 26)},
           IndexingFactType::kModule}));

  const auto result_pair = DeepEqual(tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;

  const auto result_pair2 = DeepEqual(tree2, expected);
  EXPECT_NE(result_pair2.left, nullptr) << *result_pair2.left;
  EXPECT_NE(result_pair2.right, nullptr) << *result_pair2.right;

  const auto result_pair3 = DeepEqual(tree3, expected);
  EXPECT_NE(result_pair3.left, nullptr) << *result_pair3.left;
  EXPECT_NE(result_pair3.right, nullptr) << *result_pair3.right;

  const auto result_pair4 = DeepEqual(tree4, expected);
  EXPECT_NE(result_pair4.left, nullptr) << *result_pair4.left;
  EXPECT_NE(result_pair4.right, nullptr) << *result_pair4.right;
}

TEST(ExtractOneFileTest, FactsTreeExtractor) {
  constexpr absl::string_view code_text = "";
  constexpr absl::string_view file_name = "verilog.v";
  int exit_status = 0;
  bool parse_ok = false;

  const auto expected =
      T({{Anchor(file_name, 0, code_text.size())}, IndexingFactType ::kFile});

  const auto facts_tree =
      ExtractOneFile(code_text, file_name, exit_status, parse_ok);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(EmptyCSTTest, FactsTreeExtractor) {
  constexpr absl::string_view code_text = "";
  constexpr absl::string_view file_name = "verilog.v";
  int exit_status = 0;
  bool parse_ok = false;

  const auto expected =
      T({{Anchor(file_name, 0, code_text.size())}, IndexingFactType ::kFile});

  const auto facts_tree =
      ExtractOneFile(code_text, file_name, exit_status, parse_ok);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(EmptyModuleTest, FactsTreeExtractor) {
  constexpr absl::string_view code_text = "module foo; endmodule: foo";
  constexpr absl::string_view file_name = "verilog.v";
  int exit_status = 0;
  bool parse_ok = false;

  const auto expected =
      T({{Anchor(file_name, 0, code_text.size())}, IndexingFactType ::kFile},
        T({{Anchor(absl::string_view("foo"), 7, 10),
            Anchor(absl::string_view("foo"), 23, 26)},
           IndexingFactType::kModule}));

  const auto facts_tree =
      ExtractOneFile(code_text, file_name, exit_status, parse_ok);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(OneModuleInstanceTest, FactsTreeExtractor) {
  constexpr absl::string_view code_text =
      "module bar; endmodule: bar module foo; bar b1(); endmodule: foo";
  constexpr absl::string_view file_name = "verilog.v";
  int exit_status = 0;
  bool parse_ok = false;

  const auto expected =
      T({{Anchor(file_name, 0, code_text.size())}, IndexingFactType ::kFile},
        T({{Anchor(absl::string_view("bar"), 7, 10),
            Anchor(absl::string_view("bar"), 23, 26)},
           IndexingFactType::kModule}),
        T({{Anchor(absl::string_view("foo"), 34, 37),
            Anchor(absl::string_view("foo"), 60, 63)},
           IndexingFactType::kModule},
          T({{Anchor(absl::string_view("bar"), 39, 42),
              Anchor(absl::string_view("b1"), 43, 45)},
             IndexingFactType ::kModuleInstance})));

  const auto facts_tree =
      ExtractOneFile(code_text, file_name, exit_status, parse_ok);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}  // namespace

TEST(TwoModuleInstanceTest, FactsTreeExtractor) {
  constexpr absl::string_view code_text =
      "module bar; endmodule: bar module foo; bar b1(); bar b2(); endmodule: "
      "foo";
  constexpr absl::string_view file_name = "verilog.v";
  int exit_status = 0;
  bool parse_ok = false;

  const auto expected =
      T({{Anchor(file_name, 0, code_text.size())}, IndexingFactType ::kFile},
        T({{Anchor(absl::string_view("bar"), 7, 10),
            Anchor(absl::string_view("bar"), 23, 26)},
           IndexingFactType::kModule}),
        T({{Anchor(absl::string_view("foo"), 34, 37),
            Anchor(absl::string_view("foo"), 70, 73)},
           IndexingFactType::kModule},
          T({{Anchor(absl::string_view("bar"), 39, 42),
              Anchor(absl::string_view("b1"), 43, 45)},
             IndexingFactType ::kModuleInstance}),
          T({{Anchor(absl::string_view("bar"), 49, 52),
              Anchor(absl::string_view("b2"), 53, 55)},
             IndexingFactType ::kModuleInstance})));

  const auto facts_tree =
      ExtractOneFile(code_text, file_name, exit_status, parse_ok);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}  // namespace

}  // namespace
}  // namespace kythe
}  // namespace verilog
