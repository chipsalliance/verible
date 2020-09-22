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

TEST(FactsTreeExtractor, ParseErrorTest) {
  // These inputs are lexically or syntactically invalid.
  constexpr absl::string_view code_texts[] = {
      "9badid foo;\n"       // lexical error
      "final v;\n",         // syntax error
      "module unfinished",  // syntax error
  };
  constexpr absl::string_view file_name = "verilog.v";

  for (const auto& code_text : code_texts) {
    int exit_status = 0;
    bool parse_ok = true;

    const auto facts_tree =
        ExtractOneFile(code_text, file_name, exit_status, parse_ok);
    EXPECT_FALSE(parse_ok) << "code\n" << code_text;
    EXPECT_EQ(exit_status, 1) << "code\n" << code_text;
  }
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
          // refers to bar.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[7], kTestCase.code),
                  },
                  IndexingFactType::kDataTypeReference,
              },
              // refers to bar b1().
              T({
                  {
                      Anchor(kTestCase.expected_tokens[9], kTestCase.code),
                  },
                  IndexingFactType ::kModuleInstance,
              }))));

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
          // refers to bar.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[7], kTestCase.code),
                  },
                  IndexingFactType ::kDataTypeReference,
              },
              // refers to bar b1().
              T({
                  {
                      Anchor(kTestCase.expected_tokens[9], kTestCase.code),
                  },
                  IndexingFactType ::kModuleInstance,
              })),
          // refers to bar.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[11], kTestCase.code),
                  },
                  IndexingFactType ::kDataTypeReference,
              },
              // refers to bar b2().
              T({
                  {
                      Anchor(kTestCase.expected_tokens[13], kTestCase.code),
                  },
                  IndexingFactType ::kModuleInstance,
              }))));

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
          // refers to bar.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[7], kTestCase.code),
                  },
                  IndexingFactType ::kDataTypeReference,
              },
              // refers to b1().
              T({
                  {
                      Anchor(kTestCase.expected_tokens[9], kTestCase.code),
                  },
                  IndexingFactType ::kModuleInstance,
              }),
              // refers to bar b2().
              T({
                  {
                      Anchor(kTestCase.expected_tokens[11], kTestCase.code),
                  },
                  IndexingFactType ::kModuleInstance,
              }))));

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

TEST(FactsTreeExtractor, ModuleWithPortsNonANSIStyleTest) {
  constexpr int kTag = 1;  // value doesn't matter

  // Normally, tools will reject non-ANSI port declarations that are missing
  // their full definitions inside the body like "input a", but here we don't
  // care and are just checking for references, even if they are dangling.
  const verible::SyntaxTreeSearchTestCase kTestCase = {{"module ",
                                                        {kTag, "foo"},
                                                        "(",
                                                        {kTag, "a"},
                                                        ", ",
                                                        {kTag, "b"},
                                                        ", input wire ",
                                                        {kTag, "z"},
                                                        ", ",
                                                        {kTag, "h"},
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
                  Anchor(kTestCase.expected_tokens[11], kTestCase.code),
              },
              IndexingFactType::kModule,
          },
          // refers to  a.
          T({
              {
                  Anchor(kTestCase.expected_tokens[3], kTestCase.code),
              },
              IndexingFactType::kVariableReference,
          }),
          // refers to  b.
          T({
              {
                  Anchor(kTestCase.expected_tokens[5], kTestCase.code),
              },
              IndexingFactType::kVariableReference,
          }),
          // refers to input z.
          T({
              {
                  Anchor(kTestCase.expected_tokens[7], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          // refers to h.
          T({
              {
                  Anchor(kTestCase.expected_tokens[9], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          })));

  const auto facts_tree =
      ExtractOneFile(kTestCase.code, file_name, exit_status, parse_ok);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, ModuleInstanceWithActualNamedPorts) {
  constexpr int kTag = 1;  // value doesn't matter

  const verible::SyntaxTreeSearchTestCase kTestCase = {{"module ",
                                                        {kTag, "foo"},
                                                        "(input ",
                                                        {kTag, "a"},
                                                        ", input ",
                                                        {kTag, "b"},
                                                        ", input wire ",
                                                        {kTag, "z"},
                                                        ", output ",
                                                        {kTag, "h"},
                                                        ");\nendmodule: ",
                                                        {kTag, "foo"},
                                                        "\nmodule ",
                                                        {kTag, "bar"},
                                                        "(input ",
                                                        {kTag, "a"},
                                                        ", ",
                                                        {kTag, "b"},
                                                        ", ",
                                                        {kTag, "c"},
                                                        ", ",
                                                        {kTag, "h"},
                                                        ");\n",
                                                        {kTag, "foo"},
                                                        " ",
                                                        {kTag, "f1"},
                                                        "(.",
                                                        {kTag, "a"},
                                                        "(",
                                                        {kTag, "a"},
                                                        "), .",
                                                        {kTag, "b"},
                                                        "(",
                                                        {kTag, "b"},
                                                        "), .",
                                                        {kTag, "z"},
                                                        "(",
                                                        {kTag, "c"},
                                                        "), .",
                                                        {kTag, "h"},
                                                        ");\nendmodule"}};

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
                  Anchor(kTestCase.expected_tokens[11], kTestCase.code),
              },
              IndexingFactType::kModule,
          },
          // refers to  a.
          T({
              {
                  Anchor(kTestCase.expected_tokens[3], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          // refers to  b.
          T({
              {
                  Anchor(kTestCase.expected_tokens[5], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          // refers to input z.
          T({
              {
                  Anchor(kTestCase.expected_tokens[7], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          // refers to h.
          T({
              {
                  Anchor(kTestCase.expected_tokens[9], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          })),
      // refers to module bar.
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[13], kTestCase.code),
              },
              IndexingFactType::kModule,
          },
          // refers to input a.
          T({
              {
                  Anchor(kTestCase.expected_tokens[15], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          // refers to b.
          T({
              {
                  Anchor(kTestCase.expected_tokens[17], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          // refers to c.
          T({
              {
                  Anchor(kTestCase.expected_tokens[19], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          // refers to h.
          T({
              {
                  Anchor(kTestCase.expected_tokens[21], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          // refers to foo.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[23], kTestCase.code),
                  },
                  IndexingFactType ::kDataTypeReference,
              },
              // refers to f1(.a(a), .b(b), .z(c), .h).
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[25], kTestCase.code),
                      },
                      IndexingFactType ::kModuleInstance,
                  },
                  // refers to .a
                  T(
                      {
                          {
                              Anchor(kTestCase.expected_tokens[27],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kModuleNamedPort,
                      },
                      // refers to a
                      T({
                          {
                              Anchor(kTestCase.expected_tokens[29],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kVariableReference,
                      })),
                  // refers to .b
                  T(
                      {
                          {
                              Anchor(kTestCase.expected_tokens[31],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kModuleNamedPort,
                      },
                      // refers to b
                      T({
                          {
                              Anchor(kTestCase.expected_tokens[33],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kVariableReference,
                      })),
                  // refers to .z
                  T(
                      {
                          {
                              Anchor(kTestCase.expected_tokens[35],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kModuleNamedPort,
                      },
                      // refers to c
                      T({
                          {
                              Anchor(kTestCase.expected_tokens[37],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kVariableReference,
                      })),
                  // refers to .h
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[39], kTestCase.code),
                      },
                      IndexingFactType ::kModuleNamedPort,
                  })))));

  const auto facts_tree =
      ExtractOneFile(kTestCase.code, file_name, exit_status, parse_ok);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, ModuleWithPortsDataTypeForwarding) {
  constexpr int kTag = 1;  // value doesn't matter

  // Normally, tools will reject non-ANSI port declarations that are missing
  // their full definitions inside the body like "input a", but here we don't
  // care and are just checking for references, even if they are dangling.
  const verible::SyntaxTreeSearchTestCase kTestCase = {{"module ",
                                                        {kTag, "foo"},
                                                        "(input wire ",
                                                        {kTag, "a"},
                                                        ", ",
                                                        {kTag, "b"},
                                                        ", output wire ",
                                                        {kTag, "z"},
                                                        ", ",
                                                        {kTag, "h"},
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
                  Anchor(kTestCase.expected_tokens[11], kTestCase.code),
              },
              IndexingFactType::kModule,
          },
          // refers to  a.
          T({
              {
                  Anchor(kTestCase.expected_tokens[3], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          // refers to  b.
          T({
              {
                  Anchor(kTestCase.expected_tokens[5], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          // refers to input z.
          T({
              {
                  Anchor(kTestCase.expected_tokens[7], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          // refers to h.
          T({
              {
                  Anchor(kTestCase.expected_tokens[9], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          })));

  const auto facts_tree =
      ExtractOneFile(kTestCase.code, file_name, exit_status, parse_ok);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, PrimitiveTypeExtraction) {
  constexpr int kTag = 1;  // value doesn't matter

  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"package ",
       {kTag, "pkg"},
       ";\n int ",
       {kTag, "x"},
       ", ",
       {kTag, "y"},
       ";\nlogic ",
       {kTag, "l1"},
       ", ",
       {kTag, "l2"},
       ";\nbit ",
       {kTag, "b1"},
       ", ",
       {kTag, "b2"},
       ";\nstring ",
       {kTag, "s1"},
       ", ",
       {kTag, "s2"},
       ";\nendpackage\nclass ",
       {kTag, "cla"},
       ";\n int ",
       {kTag, "x"},
       ", ",
       {kTag, "y"},
       ";\nlogic ",
       {kTag, "l1"},
       ", ",
       {kTag, "l2"},
       ";\nbit ",
       {kTag, "b1"},
       ", ",
       {kTag, "b2"},
       ";\nstring ",
       {kTag, "s1"},
       ", ",
       {kTag, "s2"},
       ";\nendclass\nfunction int ",
       {kTag, "fun"},
       "();\n int ",
       {kTag, "x"},
       ", ",
       {kTag, "y"},
       ";\nlogic ",
       {kTag, "l1"},
       ", ",
       {kTag, "l2"},
       ";\nbit ",
       {kTag, "b1"},
       ", ",
       {kTag, "b2"},
       ";\nstring ",
       {kTag, "s1"},
       ", ",
       {kTag, "s2"},
       ";\nreturn ",
       {kTag, "x"},
       ";\nendfunction"}};

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
      // refers to package pkg.
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[1], kTestCase.code),
              },
              IndexingFactType::kPackage,
          },
          // refers to x;
          T({
              {
                  Anchor(kTestCase.expected_tokens[3], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          // refers to y;
          T({
              {
                  Anchor(kTestCase.expected_tokens[5], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          // refers to l1;
          T({
              {
                  Anchor(kTestCase.expected_tokens[7], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          // refers to l2;
          T({
              {
                  Anchor(kTestCase.expected_tokens[9], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          // refers to b1;
          T({
              {
                  Anchor(kTestCase.expected_tokens[11], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          // refers to b2;
          T({
              {
                  Anchor(kTestCase.expected_tokens[13], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          // refers to s1;
          T({
              {
                  Anchor(kTestCase.expected_tokens[15], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          // refers to s2;
          T({
              {
                  Anchor(kTestCase.expected_tokens[17], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          })),
      // refers to class cla.
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[19], kTestCase.code),
              },
              IndexingFactType::kClass,
          },
          // refers to x;
          T({
              {
                  Anchor(kTestCase.expected_tokens[21], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          // refers to y;
          T({
              {
                  Anchor(kTestCase.expected_tokens[23], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          // refers to l1;
          T({
              {
                  Anchor(kTestCase.expected_tokens[25], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          // refers to l2;
          T({
              {
                  Anchor(kTestCase.expected_tokens[27], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          // refers to b1;
          T({
              {
                  Anchor(kTestCase.expected_tokens[29], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          // refers to b2;
          T({
              {
                  Anchor(kTestCase.expected_tokens[31], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          // refers to s1;
          T({
              {
                  Anchor(kTestCase.expected_tokens[33], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          // refers to s2;
          T({
              {
                  Anchor(kTestCase.expected_tokens[35], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          })),
      // refers to function fun.
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[37], kTestCase.code),
              },
              IndexingFactType::kFunctionOrTask,
          },
          // refers to x;
          T({
              {
                  Anchor(kTestCase.expected_tokens[39], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          // refers to y;
          T({
              {
                  Anchor(kTestCase.expected_tokens[41], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          // refers to l1;
          T({
              {
                  Anchor(kTestCase.expected_tokens[43], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          // refers to l2;
          T({
              {
                  Anchor(kTestCase.expected_tokens[45], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          // refers to b1;
          T({
              {
                  Anchor(kTestCase.expected_tokens[47], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          // refers to b2;
          T({
              {
                  Anchor(kTestCase.expected_tokens[49], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          // refers to s1;
          T({
              {
                  Anchor(kTestCase.expected_tokens[51], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          // refers to s2;
          T({
              {
                  Anchor(kTestCase.expected_tokens[53], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }),
          // refers to return x;
          T({
              {
                  Anchor(kTestCase.expected_tokens[55], kTestCase.code),
              },
              IndexingFactType::kVariableReference,
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
          // refers to bar.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[15], kTestCase.code),
                  },
                  IndexingFactType ::kDataTypeReference,
              },
              // refers to b1(x, y).
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[17], kTestCase.code),
                      },
                      IndexingFactType ::kModuleInstance,
                  },
                  // refers to x
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[19], kTestCase.code),
                      },
                      IndexingFactType ::kVariableReference,
                  }),
                  // refers to y
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[21], kTestCase.code),
                      },
                      IndexingFactType ::kVariableReference,
                  })))));

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
          // refers to bar.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[7], kTestCase.code),
                  },
                  IndexingFactType ::kDataTypeReference,
              },
              // refers to b1.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[9], kTestCase.code),
                  },
                  IndexingFactType ::kClassInstance,
              }))));

  const auto facts_tree =
      ExtractOneFile(kTestCase.code, file_name, exit_status, parse_ok);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, ClassMemberAccess) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {{
      "class ",          {kTag, "inner"},
      ";\n static int ", {kTag, "x"},
      ";\nendclass\n",   "class ",
      {kTag, "bar"},     ";\n static ",
      {kTag, "inner"},   " ",
      {kTag, "in1"},     " = new();\nendclass: ",
      {kTag, "bar"},     "\nmodule ",
      {kTag, "foo"},     "();\n ",
      {kTag, "bar"},     " ",
      {kTag, "b1"},      "= new();\n initial $display(",
      {kTag, "bar"},     "::",
      {kTag, "in"},      "::",
      {kTag, "x"},       ");\nendmodule: ",
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
      // refers to class inner.
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[1], kTestCase.code),
              },
              IndexingFactType::kClass,
          },
          // refers to int x.
          T({
              {
                  Anchor(kTestCase.expected_tokens[3], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          })),
      // refers to class bar.
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[6], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[12], kTestCase.code),
              },
              IndexingFactType::kClass,
          },
          // refers to inner in1.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[8], kTestCase.code),
                  },
                  IndexingFactType::kDataTypeReference,
              },
              // refers to in1.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[10], kTestCase.code),
                  },
                  IndexingFactType::kClassInstance,
              }))),
      // refers to module foo.
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[14], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[26], kTestCase.code),
              },
              IndexingFactType::kModule,
          },
          // refers to bar.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[16], kTestCase.code),
                  },
                  IndexingFactType ::kDataTypeReference,
              },
              // refers to b1.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[18], kTestCase.code),
                  },
                  IndexingFactType ::kClassInstance,
              })),
          // refers to bar::in::x.
          T({
              {
                  Anchor(kTestCase.expected_tokens[20], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[22], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[24], kTestCase.code),
              },
              IndexingFactType ::kMemberReference,
          })));

  const auto facts_tree =
      ExtractOneFile(kTestCase.code, file_name, exit_status, parse_ok);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, FunctionAndTaskDeclarationNoArgs) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {{
      "function int ",
      {kTag, "foo"},
      "();",
      ";\nendfunction ",
      "task ",
      {kTag, "bar"},
      "();",
      ";\nendtask ",
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
      // refers to function foo
      T({
          {
              Anchor(kTestCase.expected_tokens[1], kTestCase.code),
          },
          IndexingFactType::kFunctionOrTask,
      }),
      // refers to task bar
      T({
          {
              Anchor(kTestCase.expected_tokens[5], kTestCase.code),
          },
          IndexingFactType ::kFunctionOrTask,
      }));

  const auto facts_tree =
      ExtractOneFile(kTestCase.code, file_name, exit_status, parse_ok);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, FunctionAndTaskDeclarationWithArgs) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {{
      "function int ", {kTag, "foo"},  "(int ", {kTag, "arg1"},
      ", input ",      {kTag, "arg2"}, ", ",    {kTag, "arg3"},
      ", bit ",        {kTag, "arg4"}, ");",    ";\nendfunction ",
      "task ",         {kTag, "bar"},  "(int ", {kTag, "arg1"},
      ", input ",      {kTag, "arg2"}, ", ",    {kTag, "arg3"},
      ", bit ",        {kTag, "arg4"}, ");",    ";\nendtask ",
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
      // refers to function foo
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[1], kTestCase.code),
              },
              IndexingFactType::kFunctionOrTask,
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
          }),
          T({
              {
                  Anchor(kTestCase.expected_tokens[7], kTestCase.code),
              },
              IndexingFactType ::kVariableDefinition,
          }),
          T({
              {
                  Anchor(kTestCase.expected_tokens[9], kTestCase.code),
              },
              IndexingFactType ::kVariableDefinition,
          })),
      // refers to task bar
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[13], kTestCase.code),
              },
              IndexingFactType ::kFunctionOrTask,
          },
          T({
              {
                  Anchor(kTestCase.expected_tokens[15], kTestCase.code),
              },
              IndexingFactType ::kVariableDefinition,
          }),
          T({
              {
                  Anchor(kTestCase.expected_tokens[17], kTestCase.code),
              },
              IndexingFactType ::kVariableDefinition,
          }),
          T({
              {
                  Anchor(kTestCase.expected_tokens[19], kTestCase.code),
              },
              IndexingFactType ::kVariableDefinition,
          }),
          T({
              {
                  Anchor(kTestCase.expected_tokens[21], kTestCase.code),
              },
              IndexingFactType ::kVariableDefinition,
          })));

  const auto facts_tree =
      ExtractOneFile(kTestCase.code, file_name, exit_status, parse_ok);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, FunctionAndTaskCallNoArgs) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {{
      "function int ",
      {kTag, "foo"},
      "();",
      ";\nendfunction\n ",
      "task ",
      {kTag, "bar"},
      "();",
      ";\nendtask ",
      "\nmodule ",
      {kTag, "m"},
      "();\ninitial begin\n",
      {kTag, "foo"},
      "();\n",
      {kTag, "bar"},
      "();\nend\nendmodule: ",
      {kTag, "m"},
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
      // refers to function foo
      T({
          {
              Anchor(kTestCase.expected_tokens[1], kTestCase.code),
          },
          IndexingFactType::kFunctionOrTask,
      }),
      // refers to task bar
      T({
          {
              Anchor(kTestCase.expected_tokens[5], kTestCase.code),
          },
          IndexingFactType ::kFunctionOrTask,
      }),
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[9], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[15], kTestCase.code),
              },
              IndexingFactType ::kModule,
          },
          T({
              {
                  Anchor(kTestCase.expected_tokens[11], kTestCase.code),
              },
              IndexingFactType ::kFunctionCall,
          }),
          T({
              {
                  Anchor(kTestCase.expected_tokens[13], kTestCase.code),
              },
              IndexingFactType ::kFunctionCall,
          })));

  const auto facts_tree =
      ExtractOneFile(kTestCase.code, file_name, exit_status, parse_ok);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, FunctionClassCall) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {{
      "class ",
      {kTag, "inner"},
      ";\n static function int ",
      {kTag, "my_fun"},
      "();\nreturn 1;\nendfunction\nfunction int ",
      {kTag, "fun_2"},
      "(int ",
      {kTag, "x"},
      ", int ",
      {kTag, "y"},
      ");\nreturn ",
      {kTag, "x"},
      " + ",
      {kTag, "y"},
      ";\nendfunction\nendclass\n",
      "class ",
      {kTag, "bar"},
      ";\n static ",
      {kTag, "inner"},
      " ",
      {kTag, "in1"},
      " = new();\nendclass: ",
      {kTag, "bar"},
      "\nmodule ",
      {kTag, "foo"},
      "();\n ",
      {kTag, "bar"},
      " ",
      {kTag, "b1"},
      "= new();\n initial $display(",
      {kTag, "bar"},
      "::",
      {kTag, "in"},
      "::",
      {kTag, "my_fun"},
      "());\ninitial $display(",
      {kTag, "bar"},
      "::",
      {kTag, "in"},
      ".",
      {kTag, "my_fun"},
      "());\n",
      {kTag, "inner"},
      " ",
      {kTag, "in1"},
      " = new();\nint ",
      {kTag, "x"},
      ", ",
      {kTag, "y"},
      ";\ninitial $display(",
      {kTag, "in1"},
      ".",
      {kTag, "fun_2"},
      "(",
      {kTag, "x"},
      ", ",
      {kTag, "y"},
      "));\nendmodule",
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
      // refers to class inner.
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[1], kTestCase.code),
              },
              IndexingFactType::kClass,
          },
          // refers to function my_fun.
          T({
              {
                  Anchor(kTestCase.expected_tokens[3], kTestCase.code),
              },
              IndexingFactType::kFunctionOrTask,
          }),
          // refers to function fun_2.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                  },
                  IndexingFactType::kFunctionOrTask,
              },
              // refers to x arg in fun_2.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[7], kTestCase.code),
                  },
                  IndexingFactType ::kVariableDefinition,
              }),
              // refers to y arg in fun_2.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[9], kTestCase.code),
                  },
                  IndexingFactType ::kVariableDefinition,
              }),
              // refers to x.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[11], kTestCase.code),
                  },
                  IndexingFactType ::kVariableReference,
              }),
              // refers to x.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[13], kTestCase.code),
                  },
                  IndexingFactType ::kVariableReference,
              }))),
      // refers to class bar.
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[16], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[22], kTestCase.code),
              },
              IndexingFactType::kClass,
          },
          // refers to inner in1.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[18], kTestCase.code),
                  },
                  IndexingFactType::kDataTypeReference,
              },
              // refers to in1.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[20], kTestCase.code),
                  },
                  IndexingFactType::kClassInstance,
              }))),
      // refers to module foo.
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[24], kTestCase.code),
              },
              IndexingFactType::kModule,
          },
          // refers to bar.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[26], kTestCase.code),
                  },
                  IndexingFactType ::kDataTypeReference,
              },
              // refers to b1.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[28], kTestCase.code),
                  },
                  IndexingFactType ::kClassInstance,
              })),
          // refers to bar::in::my_fun().
          T({
              {
                  Anchor(kTestCase.expected_tokens[30], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[32], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[34], kTestCase.code),
              },
              IndexingFactType ::kFunctionCall,
          }),
          // refers to bar::in.my_fun().
          T({
              {
                  Anchor(kTestCase.expected_tokens[36], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[38], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[40], kTestCase.code),
              },
              IndexingFactType ::kFunctionCall,
          }),
          // refers to inner in1.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[42], kTestCase.code),
                  },
                  IndexingFactType::kDataTypeReference,
              },
              // refers to in1.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[44], kTestCase.code),
                  },
                  IndexingFactType::kClassInstance,
              })),
          // refers to int x.
          T({
              {
                  Anchor(kTestCase.expected_tokens[46], kTestCase.code),
              },
              IndexingFactType ::kVariableDefinition,
          }),
          // refers to int y.
          T({
              {
                  Anchor(kTestCase.expected_tokens[48], kTestCase.code),
              },
              IndexingFactType ::kVariableDefinition,
          }),
          // refers to in1.my_fun().
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[50], kTestCase.code),
                      Anchor(kTestCase.expected_tokens[52], kTestCase.code),
                  },
                  IndexingFactType ::kFunctionCall,
              },
              // refers to x.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[54], kTestCase.code),
                  },
                  IndexingFactType ::kVariableReference,
              }),
              // refers to y.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[56], kTestCase.code),
                  },
                  IndexingFactType ::kVariableReference,
              }))));

  const auto facts_tree =
      ExtractOneFile(kTestCase.code, file_name, exit_status, parse_ok);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, MacroDefinitionTest) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {
          "`define ",
          {kTag, "PRINT_STRING"},
          "(",
          {kTag, "str1"},
          ") $display(\"%s\\n\", str1)\n",
          "`define ",
          {kTag, "PRINT_3_STRING"},
          "(",
          {kTag, "str1"},
          ", ",
          {kTag, "str2"},
          ", ",
          {kTag, "str3"},
          ")",
          R"( \
    `PRINT_STRING(str1); \
    `PRINT_STRING(str2); \
    `PRINT_STRING(str3);)",
          "\n`define ",
          {kTag, "TEN"},
          " 10",
      },
  };

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
      // refers to macro PRINT_STRING.
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[1], kTestCase.code),
              },
              IndexingFactType::kMacro,
          },
          // refers to str1 arg in PRINT_STRING.
          T({
              {
                  Anchor(kTestCase.expected_tokens[3], kTestCase.code),
              },
              IndexingFactType ::kVariableDefinition,
          })),
      // refers to macro PRINT_3_STRING.
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[6], kTestCase.code),
              },
              IndexingFactType::kMacro,
          },
          // refers to str1 arg in PRINT_3_STRING.
          T({
              {
                  Anchor(kTestCase.expected_tokens[8], kTestCase.code),
              },
              IndexingFactType ::kVariableDefinition,
          }),
          // refers to str2 arg in PRINT_3_STRING.
          T({
              {
                  Anchor(kTestCase.expected_tokens[10], kTestCase.code),
              },
              IndexingFactType ::kVariableDefinition,
          }),
          // refers to str3 arg in PRINT_3_STRING.
          T({
              {
                  Anchor(kTestCase.expected_tokens[12], kTestCase.code),
              },
              IndexingFactType ::kVariableDefinition,
          })),
      // refers to macro TEN.
      T({
          {
              Anchor(kTestCase.expected_tokens[16], kTestCase.code),
          },
          IndexingFactType::kMacro,
      }));

  const auto facts_tree =
      ExtractOneFile(kTestCase.code, file_name, exit_status, parse_ok);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, MacroCallTest) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      "`define ",
      {kTag, "PRINT_STRING"},
      "(",
      {kTag, "str1"},
      ") $display(\"%s\\n\", str1)\n",
      "`define ",
      {kTag, "PRINT_3_STRING"},
      "(",
      {kTag, "str1"},
      ", ",
      {kTag, "str2"},
      ", ",
      {kTag, "str3"},
      ")",
      R"( \
    `PRINT_STRING(str1); \
    `PRINT_STRING(str2); \
    `PRINT_STRING(str3);)",
      "\n`define ",
      {kTag, "TEN"},
      " 10\n",
      "\n`define ",
      {kTag, "NUM"},
      "(",
      {kTag, "i"},
      ") i\n",
      "module ",
      {kTag, "macro"},
      ";\ninitial begin\n",
      {kTag, "`PRINT_3_STRINGS"},
      "(\"Grand\", \"Tour\", \"S4\");\n",
      "$display(\"%d\\n\", ",
      {kTag, "`TEN"},
      ");\n",
      "$display(\"%d\\n\", ",
      {kTag, "`NUM"},
      "(",
      {kTag, "`TEN"},
      "));\n",
      "parameter int ",
      {kTag, "x"},
      " = ",
      {kTag, "`TEN"},
      ";\n"
      "end\nendmodule"};

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
      // refers to macro PRINT_STRING.
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[1], kTestCase.code),
              },
              IndexingFactType::kMacro,
          },
          // refers to str1 in PRINT_STRING.
          T({
              {
                  Anchor(kTestCase.expected_tokens[3], kTestCase.code),
              },
              IndexingFactType ::kVariableDefinition,
          })),
      // refers to macro PRINT_3_STRING.
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[6], kTestCase.code),
              },
              IndexingFactType::kMacro,
          },
          // refers to str1 in PRINT_3_STRING.
          T({
              {
                  Anchor(kTestCase.expected_tokens[8], kTestCase.code),
              },
              IndexingFactType ::kVariableDefinition,
          }),
          // refers to str2 in PRINT_3_STRING.
          T({
              {
                  Anchor(kTestCase.expected_tokens[10], kTestCase.code),
              },
              IndexingFactType ::kVariableDefinition,
          }),
          // refers to str3 in PRINT_3_STRING.
          T({
              {
                  Anchor(kTestCase.expected_tokens[12], kTestCase.code),
              },
              IndexingFactType ::kVariableDefinition,
          })),
      // refers to macro TEN.
      T({
          {
              Anchor(kTestCase.expected_tokens[16], kTestCase.code),
          },
          IndexingFactType::kMacro,
      }),
      // refers to macro NUM.
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[19], kTestCase.code),
              },
              IndexingFactType::kMacro,
          },
          // refers to i in macro NUM.
          T({
              {
                  Anchor(kTestCase.expected_tokens[21], kTestCase.code),
              },
              IndexingFactType ::kVariableDefinition,
          })),
      // refers to module macro.
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[24], kTestCase.code),
              },
              IndexingFactType::kModule,
          },
          // refers to macro call PRINT_3_STRINGS.
          T({
              {
                  Anchor(kTestCase.expected_tokens[26], kTestCase.code),
              },
              IndexingFactType::kMacroCall,
          }),
          // refers to macro call TEN.
          T({
              {
                  Anchor(kTestCase.expected_tokens[29], kTestCase.code),
              },
              IndexingFactType::kMacroCall,
          }),
          // refers to macro call NUM.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[32], kTestCase.code),
                  },
                  IndexingFactType::kMacroCall,
              },  // refers to macro call TEN.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[34], kTestCase.code),
                  },
                  IndexingFactType::kMacroCall,
              })),
          // refers to parm x
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[37], kTestCase.code),
                  },
                  IndexingFactType::kParamDeclaration,
              },
              // refers to macro call TEN.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[39], kTestCase.code),
                  },
                  IndexingFactType::kMacroCall,
              }))));

  const auto facts_tree =
      ExtractOneFile(kTestCase.code, file_name, exit_status, parse_ok);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(PackageImportTest, PackageAndImportedItemName) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"package ",
       {kTag, "pkg1"},
       ";\nendpackage\npackage ",
       {kTag, "pkg"},
       ";\nclass ",
       {kTag, "my_class"},
       ";\nendclass\nfunction ",
       {kTag, "my_function"},
       "();\nendfunction\nendpackage\nmodule ",
       {kTag, "m"},
       "();\nimport ",
       {kTag, "pkg1"},
       "::*;\nimport ",
       {kTag, "pkg"},
       "::",
       {kTag, "my_function"},
       ";\nimport ",
       {kTag, "pkg"},
       "::",
       {kTag, "my_class"},
       ";\nendmodule"},
  };

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
      // refers to package pkg1.
      T({
          {
              Anchor(kTestCase.expected_tokens[1], kTestCase.code),
          },
          IndexingFactType::kPackage,
      }),
      // refers to package pkg.
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[3], kTestCase.code),
              },
              IndexingFactType ::kPackage,
          },
          // refers to class my_class.
          T({
              {
                  Anchor(kTestCase.expected_tokens[5], kTestCase.code),
              },
              IndexingFactType ::kClass,
          }),
          // refers to function my_function.
          T({
              {
                  Anchor(kTestCase.expected_tokens[7], kTestCase.code),
              },
              IndexingFactType ::kFunctionOrTask,
          })),
      // refers to module m..
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[9], kTestCase.code),
              },
              IndexingFactType ::kModule,
          },
          // refers to import pkg1::*.
          T({
              {
                  Anchor(kTestCase.expected_tokens[11], kTestCase.code),
              },
              IndexingFactType ::kPackageImport,
          }),
          // refers to import pkg::my_function.
          T({
              {
                  Anchor(kTestCase.expected_tokens[13], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[15], kTestCase.code),
              },
              IndexingFactType ::kPackageImport,
          }),
          // refers to import pkg::my_class.
          T({
              {
                  Anchor(kTestCase.expected_tokens[17], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[19], kTestCase.code),
              },
              IndexingFactType ::kPackageImport,
          })));

  const auto facts_tree =
      ExtractOneFile(kTestCase.code, file_name, exit_status, parse_ok);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(PackageImportTest, PackageDirectMemberReference) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"package ",
       {kTag, "pkg"},
       ";\nclass ",
       {kTag, "my_class"},
       ";\nendclass\nwire ",
       {kTag, "x"},
       ";\nendpackage\nmodule ",
       {kTag, "m"},
       "();\n",
       "initial $display(",
       {kTag, "pkg"},
       "::",
       {kTag, "x"},
       ");\n",
       ";\nendmodule"},
  };

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
      // refers to package pkg.
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[1], kTestCase.code),
              },
              IndexingFactType ::kPackage,
          },
          // refers to class my_class.
          T({
              {
                  Anchor(kTestCase.expected_tokens[3], kTestCase.code),
              },
              IndexingFactType ::kClass,
          }),
          // refers to wire x.
          T({
              {
                  Anchor(kTestCase.expected_tokens[5], kTestCase.code),
              },
              IndexingFactType ::kVariableDefinition,
          })),
      // refers to module m..
      T(
          {
              {
                  Anchor(kTestCase.expected_tokens[7], kTestCase.code),
              },
              IndexingFactType ::kModule,
          },
          // refers to $display(pkg::x).
          T({
              {
                  Anchor(kTestCase.expected_tokens[10], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[12], kTestCase.code),
              },
              IndexingFactType ::kMemberReference,
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
