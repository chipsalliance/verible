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

#include "gtest/gtest.h"
#include "common/analysis/syntax_tree_search_test_utils.h"
#include "common/text/concrete_syntax_tree.h"
#include "verilog/analysis/verilog_analyzer.h"

#undef EXPECT_OK
#define EXPECT_OK(value) EXPECT_TRUE((value).ok())

namespace verilog {
namespace kythe {
namespace {

typedef IndexingFactNode T;

TEST(FactsTreeExtractor, EqualOperatorTest) {
  constexpr absl::string_view code_text = "";
  constexpr absl::string_view file_name = "verilog.v";

  const IndexingFactNode expected({
      {
          Anchor(file_name, 0, code_text.size()),
      },
      IndexingFactType ::kFile,
  });

  const IndexingFactNode tree({
      {
          Anchor(file_name, 0, code_text.size()),
      },
      IndexingFactType ::kFile,
  });

  const IndexingFactNode tree2({
      {
          Anchor(file_name, 0, 556),
      },
      IndexingFactType ::kFile,
  });

  const IndexingFactNode tree3({
      {
          Anchor(file_name, 0, 4589),
          Anchor(file_name, 0, 987),
      },
      IndexingFactType ::kFile,
  });

  const IndexingFactNode tree4(
      {
          {
              Anchor(file_name, 0, code_text.size()),
          },
          IndexingFactType ::kFile,
      },
      T({
          {
              Anchor(absl::string_view("foo"), 7, 10),
              Anchor(absl::string_view("foo"), 23, 26),
          },
          IndexingFactType::kModule,
      }));

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

TEST(FactsTreeExtractor, EmptyCSTTest) {
  constexpr absl::string_view code_text = "";
  constexpr absl::string_view file_name = "verilog.v";
  int exit_status = 0;
  bool parse_ok = false;

  const IndexingFactNode expected({
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

TEST(FactsTreeExtractor, EmptyModuleTest) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"module ", {kTag, "foo"}, ";\n endmodule: ", {kTag, "foo"}}};

  constexpr absl::string_view file_name = "verilog.v";
  int exit_status = 0;
  bool parse_ok = false;

  const IndexingFactNode expected(
      {
          {
              Anchor(file_name, 0, kTestCase.code.size()),
              Anchor(kTestCase.code, 0, kTestCase.code.size()),
          },
          IndexingFactType ::kFile,
      },
      // refers to module foo.
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

TEST(FactsTreeExtractor, OneModuleInstanceTest) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {{"module ",
                                                        {kTag, "bar"},
                                                        ";\n endmodule: ",
                                                        {kTag, "bar"},
                                                        "\nmodule ",
                                                        {kTag, "foo"},
                                                        ";\n ",
                                                        {kTag, "bar"},
                                                        " ",
                                                        {kTag, "b1"},
                                                        "();\n endmodule: ",
                                                        {kTag, "foo"}}};

  constexpr absl::string_view file_name = "verilog.v";
  int exit_status = 0;
  bool parse_ok = false;

  const IndexingFactNode expected(
      {
          {
              Anchor(file_name, 0, kTestCase.code.size()),
              Anchor(kTestCase.code, 0, kTestCase.code.size()),
          },
          IndexingFactType ::kFile,
      },
      // refers to module bar.
      T({
          {
              Anchor(kTestCase.expected_tokens[1], kTestCase.code),
              Anchor(kTestCase.expected_tokens[3], kTestCase.code),
          },
          IndexingFactType::kModule,
      }),
      // refers to module foo.
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[11], kTestCase.code),
              },
              IndexingFactType::kModule,
          },
          // refers to bar b1().
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

TEST(FactsTreeExtractor, TwoModuleInstanceTest) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {{"module ",
                                                        {kTag, "bar"},
                                                        ";\n endmodule: ",
                                                        {kTag, "bar"},
                                                        "\nmodule ",
                                                        {kTag, "foo"},
                                                        "; ",
                                                        {kTag, "bar"},
                                                        " ",
                                                        {kTag, "b1"},
                                                        "();\n",
                                                        {kTag, "bar"},
                                                        " ",
                                                        {kTag, "b2"},
                                                        "();\nendmodule: ",
                                                        {kTag, "foo"}}};

  constexpr absl::string_view file_name = "verilog.v";
  int exit_status = 0;
  bool parse_ok = false;

  const IndexingFactNode expected(
      {
          {
              Anchor(file_name, 0, kTestCase.code.size()),
              Anchor(kTestCase.code, 0, kTestCase.code.size()),
          },
          IndexingFactType ::kFile,
      },
      // refers to module bar.
      T({
          {
              Anchor(kTestCase.expected_tokens[1], kTestCase.code),
              Anchor(kTestCase.expected_tokens[3], kTestCase.code),
          },
          IndexingFactType::kModule,
      }),
      // refers to module foo.
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[15], kTestCase.code),
              },
              IndexingFactType::kModule,
          },
          // refers to bar b1().
          T({
              {
                  Anchor(kTestCase.expected_tokens[7], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[9], kTestCase.code),
              },
              IndexingFactType ::kModuleInstance,
          }),
          // refers to bar b2().
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

TEST(FactsTreeExtractor, MultipleModuleInstancesInTheSameDeclarationTest) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {{"module ",
                                                        {kTag, "bar"},
                                                        ";\nendmodule: ",
                                                        {kTag, "bar"},
                                                        "\nmodule ",
                                                        {kTag, "foo"},
                                                        ";\n",
                                                        {kTag, "bar"},
                                                        " ",
                                                        {kTag, "b1"},
                                                        "(), ",
                                                        {kTag, "b2"},
                                                        "();\nendmodule: ",
                                                        {kTag, "foo"}}};

  constexpr absl::string_view file_name = "verilog.v";
  int exit_status = 0;
  bool parse_ok = false;

  const IndexingFactNode expected(
      {
          {
              Anchor(file_name, 0, kTestCase.code.size()),
              Anchor(kTestCase.code, 0, kTestCase.code.size()),
          },
          IndexingFactType ::kFile,
      },
      // refers to module bar.
      T({
          {
              Anchor(kTestCase.expected_tokens[1], kTestCase.code),
              Anchor(kTestCase.expected_tokens[3], kTestCase.code),
          },
          IndexingFactType::kModule,
      }),
      // refers to module foo.
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[13], kTestCase.code),
              },
              IndexingFactType::kModule,
          },
          // refers to bar b1(), b2().
          // bar b1().
          T({
              {
                  Anchor(kTestCase.expected_tokens[7], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[9], kTestCase.code),
              },
              IndexingFactType ::kModuleInstance,
          }),
          // bar b2().
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

TEST(FactsTreeExtractor, ModuleWithPortsTest) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {{"module ",
                                                        {kTag, "foo"},
                                                        "(input ",
                                                        {kTag, "a"},
                                                        ", output ",
                                                        {kTag, "b"},
                                                        ");\nendmodule: ",
                                                        {kTag, "foo"}}};

  constexpr absl::string_view file_name = "verilog.v";
  int exit_status = 0;
  bool parse_ok = false;

  const IndexingFactNode expected(
      {
          {
              Anchor(file_name, 0, kTestCase.code.size()),
              Anchor(kTestCase.code, 0, kTestCase.code.size()),
          },
          IndexingFactType ::kFile,
      },
      // refers to module foo.
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[7], kTestCase.code),
              },
              IndexingFactType::kModule,
          },
          // refers to input a.
          T({
              {
                  Anchor(kTestCase.expected_tokens[3], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          // refers to output b.
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

TEST(FactsTreeExtractor, MultiSignalDeclaration) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {{"module ",
                                                        {kTag, "foo"},
                                                        "(",
                                                        "input ",
                                                        {kTag, "in"},
                                                        ");\n",
                                                        "input ",
                                                        {kTag, "x"},
                                                        ", ",
                                                        {kTag, "y"},
                                                        ";\noutput ",
                                                        {kTag, "z"},
                                                        ";\nendmodule: ",
                                                        {kTag, "foo"}}};

  constexpr absl::string_view file_name = "verilog.v";
  int exit_status = 0;
  bool parse_ok = false;

  const IndexingFactNode expected(
      {
          {
              Anchor(file_name, 0, kTestCase.code.size()),
              Anchor(kTestCase.code, 0, kTestCase.code.size()),
          },
          IndexingFactType ::kFile,
      },
      // refers to module foo.
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[13], kTestCase.code),
              },
              IndexingFactType::kModule,
          },
          // refers to input in.
          T({
              {
                  Anchor(kTestCase.expected_tokens[4], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          // refers to output x.
          T({
              {
                  Anchor(kTestCase.expected_tokens[7], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          // refers to output y
          T({
              {
                  Anchor(kTestCase.expected_tokens[9], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          // refers to output z.
          T({
              {
                  Anchor(kTestCase.expected_tokens[11], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          })));

  const auto facts_tree =
      ExtractOneFile(kTestCase.code, file_name, exit_status, parse_ok);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, ModuleInstanceWithPortsTest) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {{"module ",
                                                        {kTag, "bar"},
                                                        "(input ",
                                                        {kTag, "x"},
                                                        ", output ",
                                                        {kTag, "y"},
                                                        ");\nendmodule: ",
                                                        {kTag, "bar"},
                                                        "\nmodule ",
                                                        {kTag, "foo"},
                                                        "(input ",
                                                        {kTag, "x"},
                                                        ", output ",
                                                        {kTag, "y"},
                                                        ");\n ",
                                                        {kTag, "bar"},
                                                        " ",
                                                        {kTag, "b1"},
                                                        "(",
                                                        {kTag, "x"},
                                                        ", ",
                                                        {kTag, "y"},
                                                        ");\nendmodule: ",
                                                        {kTag, "foo"}}};

  constexpr absl::string_view file_name = "verilog.v";
  int exit_status = 0;
  bool parse_ok = false;

  const IndexingFactNode expected(
      {
          {
              Anchor(file_name, 0, kTestCase.code.size()),
              Anchor(kTestCase.code, 0, kTestCase.code.size()),
          },
          IndexingFactType ::kFile,
      },
      // refers to module bar.
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[7], kTestCase.code),
              },
              IndexingFactType::kModule,
          },
          // refers to input x.
          T({
              {
                  Anchor(kTestCase.expected_tokens[3], kTestCase.code),
              },
              IndexingFactType ::kVariableDefinition,
          }),
          // refers to output y.
          T({
              {
                  Anchor(kTestCase.expected_tokens[5], kTestCase.code),
              },
              IndexingFactType ::kVariableDefinition,
          })),
      // refers to module foo.
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[9], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[23], kTestCase.code),
              },
              IndexingFactType::kModule,
          },
          // refers to input x.
          T({
              {
                  Anchor(kTestCase.expected_tokens[11], kTestCase.code),
              },
              IndexingFactType ::kVariableDefinition,
          }),
          // refers to output y.
          T({
              {
                  Anchor(kTestCase.expected_tokens[13], kTestCase.code),
              },
              IndexingFactType ::kVariableDefinition,
          }),
          // refers to bar b1(x, y).
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

TEST(FactsTreeExtractor, WireTest) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {{"module ",
                                                        {kTag, "foo"},
                                                        "();\nwire ",
                                                        {kTag, "a"},
                                                        ";\nendmodule: ",
                                                        {kTag, "foo"}}};

  constexpr absl::string_view file_name = "verilog.v";
  int exit_status = 0;
  bool parse_ok = false;

  const IndexingFactNode expected(
      {
          {
              Anchor(file_name, 0, kTestCase.code.size()),
              Anchor(kTestCase.code, 0, kTestCase.code.size()),
          },
          IndexingFactType ::kFile,
      },
      // refers to module foo
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[5], kTestCase.code),
              },
              IndexingFactType::kModule,
          },
          // refers to "wire a"
          T({
              {
                  Anchor(kTestCase.expected_tokens[3], kTestCase.code),
              },
              IndexingFactType ::kVariableDefinition,
          })));

  const auto facts_tree =
      ExtractOneFile(kTestCase.code, file_name, exit_status, parse_ok);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, ClassTest) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"class ", {kTag, "foo"}, ";\nendclass: ", {kTag, "foo"}}};

  constexpr absl::string_view file_name = "verilog.v";
  int exit_status = 0;
  bool parse_ok = false;

  const IndexingFactNode expected(
      {
          {
              Anchor(file_name, 0, kTestCase.code.size()),
              Anchor(kTestCase.code, 0, kTestCase.code.size()),
          },
          IndexingFactType ::kFile,
      },
      // refers to class foo
      T({
          {
              Anchor(kTestCase.expected_tokens[1], kTestCase.code),
              Anchor(kTestCase.expected_tokens[3], kTestCase.code),
          },
          IndexingFactType::kClass,
      }));

  const auto facts_tree =
      ExtractOneFile(kTestCase.code, file_name, exit_status, parse_ok);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, CLassWithinModuleTest) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {{"module ",
                                                        {kTag, "m"},
                                                        "();\nclass ",
                                                        {kTag, "foo"},
                                                        ";\nendclass:",
                                                        {kTag, "foo"},
                                                        ";\nendmodule: ",
                                                        {kTag, "m"}}};

  constexpr absl::string_view file_name = "verilog.v";
  int exit_status = 0;
  bool parse_ok = false;

  const IndexingFactNode expected(
      {
          {
              Anchor(file_name, 0, kTestCase.code.size()),
              Anchor(kTestCase.code, 0, kTestCase.code.size()),
          },
          IndexingFactType ::kFile,
      },
      // refers to module foo
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[7], kTestCase.code),
              },
              IndexingFactType::kModule,
          },
          // refers to "class foo"
          T({
              {
                  Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[5], kTestCase.code),
              },
              IndexingFactType ::kClass,
          })));

  const auto facts_tree =
      ExtractOneFile(kTestCase.code, file_name, exit_status, parse_ok);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, NestedClassTest) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {{
      "class ",
      {kTag, "foo"},
      ";\nclass ",
      {kTag, "bar"},
      ";\nendclass: ",
      {kTag, "bar"},
      "\nendclass: ",
      {kTag, "foo"},
  }};

  constexpr absl::string_view file_name = "verilog.v";
  int exit_status = 0;
  bool parse_ok = false;

  const IndexingFactNode expected(
      {
          {
              Anchor(file_name, 0, kTestCase.code.size()),
              Anchor(kTestCase.code, 0, kTestCase.code.size()),
          },
          IndexingFactType ::kFile,
      },
      // refers to class foo
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[7], kTestCase.code),
              },
              IndexingFactType::kClass,
          },
          // refers to class bar
          T({
              {
                  Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[5], kTestCase.code),
              },
              IndexingFactType ::kClass,
          })));

  const auto facts_tree =
      ExtractOneFile(kTestCase.code, file_name, exit_status, parse_ok);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, OneClassInstanceTest) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"class ",
       {kTag, "bar"},
       ";\n endclass: ",
       {kTag, "bar"},
       "\nmodule ",
       {kTag, "foo"},
       "();\n ",
       {kTag, "bar"},
       " ",
       {kTag, "b1"},
       "= new();\n endmodule: ",
       {kTag, "foo"}}};

  constexpr absl::string_view file_name = "verilog.v";
  int exit_status = 0;
  bool parse_ok = false;

  const IndexingFactNode expected(
      {
          {
              Anchor(file_name, 0, kTestCase.code.size()),
              Anchor(kTestCase.code, 0, kTestCase.code.size()),
          },
          IndexingFactType ::kFile,
      },
      // refers to class bar.
      T({
          {
              Anchor(kTestCase.expected_tokens[1], kTestCase.code),
              Anchor(kTestCase.expected_tokens[3], kTestCase.code),
          },
          IndexingFactType::kClass,
      }),
      // refers to module foo.
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[11], kTestCase.code),
              },
              IndexingFactType::kModule,
          },
          // refers to bar b1().
          T({
              {
                  Anchor(kTestCase.expected_tokens[7], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[9], kTestCase.code),
              },
              IndexingFactType ::kClassInstance,
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
