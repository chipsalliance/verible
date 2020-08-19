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

#include "common/analysis/syntax_tree_search_test_utils.h"
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

TEST(EmptyCSTTest, FactsTreeExtractor) {
  constexpr absl::string_view code_text = "";
  constexpr absl::string_view file_name = "verilog.v";
  int exit_status = 0;
  bool parse_ok = false;

  const auto expected = T({
      {
          Anchor(file_name, 0, code_text.size()),
          Anchor(code_text, 0, code_text.size()),
      },
      IndexingFactType ::kFile,
  });

  const auto facts_tree =
      ExtractOneFile(code_text, file_name, exit_status, parse_ok);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(EmptyModuleTest, FactsTreeExtractor) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"module ", {kTag, "foo"}, "; endmodule: ", {kTag, "foo"}}};

  constexpr absl::string_view file_name = "verilog.v";
  int exit_status = 0;
  bool parse_ok = false;

  const auto expected = T(
      {
          {
              Anchor(file_name, 0, kTestCase.code.size()),
              Anchor(kTestCase.code, 0, kTestCase.code.size()),
          },
          IndexingFactType ::kFile,
      },
      T({
          {
              Anchor(kTestCase.expected_tokens[1], kTestCase.code),
              Anchor(kTestCase.expected_tokens[3], kTestCase.code),
          },
          IndexingFactType::kModule,
      }));

  const auto facts_tree =
      ExtractOneFile(kTestCase.code, file_name, exit_status, parse_ok);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(OneModuleInstanceTest, FactsTreeExtractor) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {{"module ",
                                                        {kTag, "bar"},
                                                        "; endmodule: ",
                                                        {kTag, "bar"},
                                                        " module ",
                                                        {kTag, "foo"},
                                                        "; ",
                                                        {kTag, "bar"},
                                                        " ",
                                                        {kTag, "b1"},
                                                        "(); endmodule: ",
                                                        {kTag, "foo"}}};

  constexpr absl::string_view file_name = "verilog.v";
  int exit_status = 0;
  bool parse_ok = false;

  const auto expected = T(
      {
          {
              Anchor(file_name, 0, kTestCase.code.size()),
              Anchor(kTestCase.code, 0, kTestCase.code.size()),
          },
          IndexingFactType ::kFile,
      },
      T({
          {
              Anchor(kTestCase.expected_tokens[1], kTestCase.code),
              Anchor(kTestCase.expected_tokens[3], kTestCase.code),
          },
          IndexingFactType::kModule,
      }),
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[11], kTestCase.code),
              },
              IndexingFactType::kModule,
          },
          T({
              {
                  Anchor(kTestCase.expected_tokens[7], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[9], kTestCase.code),
              },
              IndexingFactType ::kModuleInstance,
          })));

  const auto facts_tree =
      ExtractOneFile(kTestCase.code, file_name, exit_status, parse_ok);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}  // namespace

TEST(TwoModuleInstanceTest, FactsTreeExtractor) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {{"module ",
                                                        {kTag, "bar"},
                                                        "; endmodule: ",
                                                        {kTag, "bar"},
                                                        " module ",
                                                        {kTag, "foo"},
                                                        "; ",
                                                        {kTag, "bar"},
                                                        " ",
                                                        {kTag, "b1"},
                                                        "(); ",
                                                        {kTag, "bar"},
                                                        " ",
                                                        {kTag, "b2"},
                                                        "(); endmodule: ",
                                                        {kTag, "foo"}}};

  constexpr absl::string_view file_name = "verilog.v";
  int exit_status = 0;
  bool parse_ok = false;

  const auto expected = T(
      {
          {
              Anchor(file_name, 0, kTestCase.code.size()),
              Anchor(kTestCase.code, 0, kTestCase.code.size()),
          },
          IndexingFactType ::kFile,
      },
      T({
          {
              Anchor(kTestCase.expected_tokens[1], kTestCase.code),
              Anchor(kTestCase.expected_tokens[3], kTestCase.code),
          },
          IndexingFactType::kModule,
      }),
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[15], kTestCase.code),
              },
              IndexingFactType::kModule,
          },
          T({
              {
                  Anchor(kTestCase.expected_tokens[7], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[9], kTestCase.code),
              },
              IndexingFactType ::kModuleInstance,
          }),
          T({
              {
                  Anchor(kTestCase.expected_tokens[11], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[13], kTestCase.code),
              },
              IndexingFactType ::kModuleInstance,
          })));

  const auto facts_tree =
      ExtractOneFile(kTestCase.code, file_name, exit_status, parse_ok);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}  // namespace

TEST(MultipleModuleInstancesInTheSameDeclarationTest, FactsTreeExtractor) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {{"module ",
                                                        {kTag, "bar"},
                                                        "; endmodule: ",
                                                        {kTag, "bar"},
                                                        " module ",
                                                        {kTag, "foo"},
                                                        "; ",
                                                        {kTag, "bar"},
                                                        " ",
                                                        {kTag, "b1"},
                                                        "(), ",
                                                        {kTag, "b2"},
                                                        "(); endmodule: ",
                                                        {kTag, "foo"}}};

  constexpr absl::string_view file_name = "verilog.v";
  int exit_status = 0;
  bool parse_ok = false;

  const auto expected = T(
      {
          {
              Anchor(file_name, 0, kTestCase.code.size()),
              Anchor(kTestCase.code, 0, kTestCase.code.size()),
          },
          IndexingFactType ::kFile,
      },
      T({
          {
              Anchor(kTestCase.expected_tokens[1], kTestCase.code),
              Anchor(kTestCase.expected_tokens[3], kTestCase.code),
          },
          IndexingFactType::kModule,
      }),
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[13], kTestCase.code),
              },
              IndexingFactType::kModule,
          },
          T({
              {
                  Anchor(kTestCase.expected_tokens[7], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[9], kTestCase.code),
              },
              IndexingFactType ::kModuleInstance,
          }),
          T({
              {
                  Anchor(kTestCase.expected_tokens[7], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[11], kTestCase.code),
              },
              IndexingFactType ::kModuleInstance,
          })));

  const auto facts_tree =
      ExtractOneFile(kTestCase.code, file_name, exit_status, parse_ok);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}  // namespace

TEST(ModuleWithPortsTest, FactsTreeExtractor) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {{"module ",
                                                        {kTag, "foo"},
                                                        "(input ",
                                                        {kTag, "a"},
                                                        ", output ",
                                                        {kTag, "b"},
                                                        "); endmodule: ",
                                                        {kTag, "foo"}}};

  constexpr absl::string_view file_name = "verilog.v";
  int exit_status = 0;
  bool parse_ok = false;

  const auto expected = T(
      {
          {
              Anchor(file_name, 0, kTestCase.code.size()),
              Anchor(kTestCase.code, 0, kTestCase.code.size()),
          },
          IndexingFactType ::kFile,
      },
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[7], kTestCase.code),
              },
              IndexingFactType::kModule,
          },
          T({
              {
                  Anchor(kTestCase.expected_tokens[3], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          T({
              {
                  Anchor(kTestCase.expected_tokens[5], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          })));

  const auto facts_tree =
      ExtractOneFile(kTestCase.code, file_name, exit_status, parse_ok);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(ModuleInstanceWithPortsTest, FactsTreeExtractor) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {{"module ",
                                                        {kTag, "bar"},
                                                        "(input ",
                                                        {kTag, "x"},
                                                        ", output ",
                                                        {kTag, "y"},
                                                        "); endmodule: ",
                                                        {kTag, "bar"},
                                                        " module ",
                                                        {kTag, "foo"},
                                                        "(input ",
                                                        {kTag, "x"},
                                                        ", output ",
                                                        {kTag, "y"},
                                                        "); ",
                                                        {kTag, "bar"},
                                                        " ",
                                                        {kTag, "b1"},
                                                        "(",
                                                        {kTag, "x"},
                                                        ", ",
                                                        {kTag, "y"},
                                                        "); endmodule: ",
                                                        {kTag, "foo"}}};

  constexpr absl::string_view file_name = "verilog.v";
  int exit_status = 0;
  bool parse_ok = false;

  const auto expected = T(
      {
          {
              Anchor(file_name, 0, kTestCase.code.size()),
              Anchor(kTestCase.code, 0, kTestCase.code.size()),
          },
          IndexingFactType ::kFile,
      },
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[7], kTestCase.code),
              },
              IndexingFactType::kModule,
          },
          T({
              {
                  Anchor(kTestCase.expected_tokens[3], kTestCase.code),
              },
              IndexingFactType ::kVariableDefinition,
          }),
          T({
              {
                  Anchor(kTestCase.expected_tokens[5], kTestCase.code),
              },
              IndexingFactType ::kVariableDefinition,
          })),
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[9], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[23], kTestCase.code),
              },
              IndexingFactType::kModule,
          },
          T({
              {
                  Anchor(kTestCase.expected_tokens[11], kTestCase.code),
              },
              IndexingFactType ::kVariableDefinition,
          }),
          T({
              {
                  Anchor(kTestCase.expected_tokens[13], kTestCase.code),
              },
              IndexingFactType ::kVariableDefinition,
          }),
          T({
              {
                  Anchor(kTestCase.expected_tokens[15], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[17], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[19], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[21], kTestCase.code),
              },
              IndexingFactType ::kModuleInstance,
          })));

  const auto facts_tree =
      ExtractOneFile(kTestCase.code, file_name, exit_status, parse_ok);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}  // namespace

TEST(WireTest, FactsTreeExtractor) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {{"module ",
                                                        {kTag, "foo"},
                                                        "(); wire ",
                                                        {kTag, "a"},
                                                        "; endmodule: ",
                                                        {kTag, "foo"}}};

  constexpr absl::string_view file_name = "verilog.v";
  int exit_status = 0;
  bool parse_ok = false;

  const auto expected = T(
      {
          {
              Anchor(file_name, 0, kTestCase.code.size()),
              Anchor(kTestCase.code, 0, kTestCase.code.size()),
          },
          IndexingFactType ::kFile,
      },
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[5], kTestCase.code),
              },
              IndexingFactType::kModule,
          },
          T({
              {Anchor(kTestCase.expected_tokens[3], kTestCase.code)},
              IndexingFactType ::kVariableDefinition,
          })));

  const auto facts_tree =
      ExtractOneFile(kTestCase.code, file_name, exit_status, parse_ok);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

}  // namespace
}  // namespace kythe
}  // namespace verilog
