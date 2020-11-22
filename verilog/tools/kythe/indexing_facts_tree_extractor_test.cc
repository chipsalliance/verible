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
#include "absl/status/status.h"
#include "common/analysis/syntax_tree_search_test_utils.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/util/file_util.h"
#include "verilog/analysis/verilog_analyzer.h"

#undef EXPECT_OK
#define EXPECT_OK(value) EXPECT_TRUE((value).ok())

namespace verilog {
namespace kythe {
namespace {

using verible::file::testing::ScopedTestFile;
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
  std::vector<absl::Status> errors;

  ScopedTestFile test_file(testing::TempDir(), code_text);
  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T({
          {
              Anchor(test_file.filename(), 0, code_text.size()),
              Anchor(code_text, 0, code_text.size()),
          },
          IndexingFactType ::kFile,
      })));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

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

  for (const auto& code_text : code_texts) {
    std::vector<absl::Status> errors;

    ScopedTestFile test_file(testing::TempDir(), code_text);
    const auto facts_tree =
        ExtractFiles({std::string(test_file.filename())}, "",
                     verible::file::Dirname(test_file.filename()), {}, errors);

    EXPECT_FALSE(errors.empty()) << "code\n" << code_text;
  }
}

TEST(FactsTreeExtractor, EmptyModuleTest) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"module ", {kTag, "foo"}, ";\n endmodule: ", {kTag, "foo"}}};

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
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
          }))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, NoIdentifierInsideNet) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {{
      "module ",
      {kTag, "foo"},
      ";\n tri sin;\n endmodule",
  }};

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to module foo.
          T({
              {
                  Anchor(kTestCase.expected_tokens[1], kTestCase.code),
              },
              IndexingFactType::kModule,
          }))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, PropagatedUserDefinedTypeInModulePort) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {{
      "module ",
      {kTag, "foo"},
      "(",
      {kTag, "My_type"},
      " ",
      {kTag, "x"},
      ", ",
      {kTag, "y"},
      ");\n endmodule",
  }};

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to module foo.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  },
                  IndexingFactType::kModule,
              },
              // refers to My_type.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                      },
                      IndexingFactType::kDataTypeReference,
                  },
                  // refers to x.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                      },
                      IndexingFactType::kVariableDefinition,
                  }),
                  // refers to y.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[7], kTestCase.code),
                      },
                      IndexingFactType::kVariableDefinition,
                  }))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, OneLocalNetTest) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase{{
      "module ",
      {kTag, "bar"},
      ";\n"
      "wire ",
      {kTag, "w"},
      ";\n"
      "tri ",
      {kTag, "`x"},
      ";\n"
      "endmodule",
  }};

  std::vector<absl::Status> errors;

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to module bar.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  },
                  IndexingFactType::kModule,
              },
              // refers to w.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                  },
                  IndexingFactType::kVariableDefinition,
              }),
              // refers to `x
              // TODO(minatoma): suppress declarations that use macro
              // identifiers because they evaluate to something unknown.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                  },
                  IndexingFactType::kVariableDefinition,
              })))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

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

  std::vector<absl::Status> errors;

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
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
                  }))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

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

  std::vector<absl::Status> errors;

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
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
                  }))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

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

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
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
                  }))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

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

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
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
              })))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, ModuleDimensionTypePortsTest) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"module ",
       {kTag, "foo"},
       "(input ",
       {kTag, "a"},
       ", output [",
       {kTag, "x"},
       ":",
       {kTag, "y"},
       "] ",
       {kTag, "b"},
       " [",
       {kTag, "x"},
       ":",
       {kTag, "y"},
       "]);\nendmodule"},
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to module foo.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[1], kTestCase.code),
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
                      Anchor(kTestCase.expected_tokens[9], kTestCase.code),
                  },
                  IndexingFactType::kVariableDefinition,
              }),
              // refers to input x.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                  },
                  IndexingFactType::kVariableReference,
              }),
              // refers to input y.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[7], kTestCase.code),
                  },
                  IndexingFactType::kVariableReference,
              }),

              // refers to input x.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[11], kTestCase.code),
                  },
                  IndexingFactType::kVariableReference,
              }),
              // refers to input y.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[13], kTestCase.code),
                  },
                  IndexingFactType::kVariableReference,
              })))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

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

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
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
              })))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, ClassParams) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"class ",
       {kTag, "my_class"},
       " #(parameter ",
       {kTag, "x"},
       " = 4, ",
       {kTag, "y"},
       " = 4); endclass"},
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to class my_class.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  },
                  IndexingFactType::kClass,
              },
              // refers to parameter x
              T({
                  {
                      Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                  },
                  IndexingFactType ::kParamDeclaration,
              }),
              // refers to y
              T({
                  {
                      Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                  },
                  IndexingFactType ::kParamDeclaration,
              })))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, QualifiedVariableType) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"module ",
       {kTag, "m"},
       ";\n ",
       {kTag, "pkg"},
       "::",
       {kTag, "X"},
       " ",
       {kTag, "y"},
       ";\n ",
       {kTag, "pkg"},
       "::",
       {kTag, "H"},
       " ",
       {kTag, "j"},
       " = new();\nendmodule"}};

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to module m.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  },
                  IndexingFactType::kModule,
              },
              // refers to pkg::X
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                          Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                      },
                      IndexingFactType ::kDataTypeReference,
                  },
                  // refers to y
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[7], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  })),
              // refers to pkg::H
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[9], kTestCase.code),
                          Anchor(kTestCase.expected_tokens[11], kTestCase.code),
                      },
                      IndexingFactType ::kDataTypeReference,
                  },
                  // refers to j
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[13], kTestCase.code),
                      },
                      IndexingFactType ::kClassInstance,
                  }))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, ClassTypeParams) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"class ",
       {kTag, "my_class"},
       " #(type ",
       {kTag, "x"},
       " = int, ",
       {kTag, "y"},
       " = int); endclass"},
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to class my_class.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  },
                  IndexingFactType::kClass,
              },
              // refers to parameter x
              T({
                  {
                      Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                  },
                  IndexingFactType ::kParamDeclaration,
              }),
              // refers to y
              T({
                  {
                      Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                  },
                  IndexingFactType ::kParamDeclaration,
              })))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

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

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
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
                              Anchor(kTestCase.expected_tokens[25],
                                     kTestCase.code),
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
                              Anchor(kTestCase.expected_tokens[39],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kModuleNamedPort,
                      })))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

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

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
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
              })))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

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
       " = 5, ",
       {kTag, "y"},
       " = ",
       {kTag, "my_fun"},
       "(",
       {kTag, "o"},
       ", ",
       {kTag, "l"},
       ");\nlogic ",
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

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
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
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[41], kTestCase.code),
                      },
                      IndexingFactType::kVariableDefinition,
                  },
                  // refers to my_fun;
                  T(
                      {
                          {
                              Anchor(kTestCase.expected_tokens[43],
                                     kTestCase.code),
                          },
                          IndexingFactType::kFunctionCall,
                      },
                      // refers to o;
                      T({
                          {
                              Anchor(kTestCase.expected_tokens[45],
                                     kTestCase.code),
                          },
                          IndexingFactType::kVariableReference,
                      }),
                      // refers to l;
                      T({
                          {
                              Anchor(kTestCase.expected_tokens[47],
                                     kTestCase.code),
                          },
                          IndexingFactType::kVariableReference,
                      }))),
              // refers to l1;
              T({
                  {
                      Anchor(kTestCase.expected_tokens[49], kTestCase.code),
                  },
                  IndexingFactType::kVariableDefinition,
              }),
              // refers to l2;
              T({
                  {
                      Anchor(kTestCase.expected_tokens[51], kTestCase.code),
                  },
                  IndexingFactType::kVariableDefinition,
              }),
              // refers to b1;
              T({
                  {
                      Anchor(kTestCase.expected_tokens[53], kTestCase.code),
                  },
                  IndexingFactType::kVariableDefinition,
              }),
              // refers to b2;
              T({
                  {
                      Anchor(kTestCase.expected_tokens[55], kTestCase.code),
                  },
                  IndexingFactType::kVariableDefinition,
              }),
              // refers to s1;
              T({
                  {
                      Anchor(kTestCase.expected_tokens[57], kTestCase.code),
                  },
                  IndexingFactType::kVariableDefinition,
              }),
              // refers to s2;
              T({
                  {
                      Anchor(kTestCase.expected_tokens[59], kTestCase.code),
                  },
                  IndexingFactType::kVariableDefinition,
              }),
              // refers to return x;
              T({
                  {
                      Anchor(kTestCase.expected_tokens[61], kTestCase.code),
                  },
                  IndexingFactType::kVariableReference,
              })))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

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

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
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
              })))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

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

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
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
                              Anchor(kTestCase.expected_tokens[17],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kModuleInstance,
                      },
                      // refers to x
                      T({
                          {
                              Anchor(kTestCase.expected_tokens[19],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kVariableReference,
                      }),
                      // refers to y
                      T({
                          {
                              Anchor(kTestCase.expected_tokens[21],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kVariableReference,
                      })))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

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

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
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
              })))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, ClassTest) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"class ", {kTag, "foo"}, ";\nendclass: ", {kTag, "foo"}}};

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
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
          }))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

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

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
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
              })))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

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

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
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
              })))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, OneClassInstanceTest) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {{"class ",
                                                        {kTag, "bar"},
                                                        ";\n endclass: ",
                                                        {kTag, "bar"},
                                                        "\nmodule ",
                                                        {kTag, "foo"},
                                                        "();\n ",
                                                        {kTag, "bar"},
                                                        " ",
                                                        {kTag, "b1"},
                                                        "= new(), ",
                                                        {kTag, "b2"},
                                                        " = new(",
                                                        {kTag, "x"},
                                                        ", ",
                                                        {kTag, "y"},
                                                        ");\n endmodule: ",
                                                        {kTag, "foo"}}};

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
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
                      Anchor(kTestCase.expected_tokens[17], kTestCase.code),
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
                  }),
                  // refers to b2.
                  T(
                      {
                          {
                              Anchor(kTestCase.expected_tokens[11],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kClassInstance,
                      },
                      // refers to x.
                      T({
                          {
                              Anchor(kTestCase.expected_tokens[13],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kVariableReference,
                      }),
                      // refers to y.
                      T({
                          {
                              Anchor(kTestCase.expected_tokens[15],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kVariableReference,
                      })))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

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

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
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
              // anonymous scope for initial.
              T(
                  {
                      {
                          Anchor("anonymous-scope-0", 0, 0),
                      },
                      IndexingFactType ::kAnonymousScope,
                  },
                  // refers to bar::in::x.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[20], kTestCase.code),
                          Anchor(kTestCase.expected_tokens[22], kTestCase.code),
                          Anchor(kTestCase.expected_tokens[24], kTestCase.code),
                      },
                      IndexingFactType ::kMemberReference,
                  }))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

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

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
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
          }))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

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

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
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
              })))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, ClassMember) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {{
      "module ",
      {kTag, "m"},
      "();\ninitial begin\n$display(",
      {kTag, "my_class"},
      ".",
      {kTag, "x"},
      ");\n$display(",
      {kTag, "my_class"},
      ".",
      {kTag, "instance1"},
      ".",
      {kTag, "y"},
      ");\nend\nendmodule",
  }};

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to module m
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  },
                  IndexingFactType::kModule,
              },
              // anonymous scope for initial.
              T(
                  {
                      {
                          Anchor("anonymous-scope-0", 0, 0),
                      },
                      IndexingFactType ::kAnonymousScope,
                  },
                  // refers to my_class.x
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                          Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                      },
                      IndexingFactType ::kMemberReference,
                  }),
                  // refers to my_class.instance1.x
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[7], kTestCase.code),
                          Anchor(kTestCase.expected_tokens[9], kTestCase.code),
                          Anchor(kTestCase.expected_tokens[11], kTestCase.code),
                      },
                      IndexingFactType ::kMemberReference,
                  }))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

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

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
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
              // anonymous scope for initial.
              T(
                  {
                      {
                          Anchor("anonymous-scope-0", 0, 0),
                      },
                      IndexingFactType ::kAnonymousScope,
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
                  }))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

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

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
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
              // anonymous scope for initial.
              T(
                  {
                      {
                          Anchor("anonymous-scope-0", 0, 0),
                      },
                      IndexingFactType ::kAnonymousScope,
                  },
                  // refers to bar::in::my_fun().
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[30], kTestCase.code),
                          Anchor(kTestCase.expected_tokens[32], kTestCase.code),
                          Anchor(kTestCase.expected_tokens[34], kTestCase.code),
                      },
                      IndexingFactType ::kFunctionCall,
                  })),
              // anonymous scope for initial.
              T(
                  {
                      {
                          Anchor("anonymous-scope-1", 0, 0),
                      },
                      IndexingFactType ::kAnonymousScope,
                  },
                  // refers to bar::in.my_fun().
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[36], kTestCase.code),
                          Anchor(kTestCase.expected_tokens[38], kTestCase.code),
                          Anchor(kTestCase.expected_tokens[40], kTestCase.code),
                      },
                      IndexingFactType ::kFunctionCall,
                  })),
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
              // anonymous scope for initial.
              T(
                  {
                      {
                          Anchor("anonymous-scope-2", 0, 0),
                      },
                      IndexingFactType ::kAnonymousScope,
                  },
                  // refers to in1.my_fun().
                  T(
                      {
                          {
                              Anchor(kTestCase.expected_tokens[50],
                                     kTestCase.code),
                              Anchor(kTestCase.expected_tokens[52],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kFunctionCall,
                      },
                      // refers to x.
                      T({
                          {
                              Anchor(kTestCase.expected_tokens[54],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kVariableReference,
                      }),
                      // refers to y.
                      T({
                          {
                              Anchor(kTestCase.expected_tokens[56],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kVariableReference,
                      })))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, ThisAsFunctionCall) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase{
      {
          {kTag, "r"},
          "=this();",
      },
  };

  std::vector<absl::Status> errors;

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to r.
          T({
              {
                  Anchor(kTestCase.expected_tokens[0], kTestCase.code),
              },
              IndexingFactType::kVariableDefinition,
          }))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

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

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
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
          }))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

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
      ";\ninitial begin\n`",
      {kTag, "PRINT_3_STRINGS"},
      "(\"Grand\", \"Tour\", \"S4\");\n",
      "$display(\"%d\\n\", `",
      {kTag, "TEN"},
      ");\n",
      "$display(\"%d\\n\", `",
      {kTag, "NUM"},
      "(`",
      {kTag, "TEN"},
      "));\n",
      "parameter int ",
      {kTag, "x"},
      " = `",
      {kTag, "TEN"},
      ";\n"
      "end\nendmodule",
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
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
              // anonymous scope for initial.
              T(
                  {
                      {
                          Anchor("anonymous-scope-0", 0, 0),
                      },
                      IndexingFactType ::kAnonymousScope,
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
                              Anchor(kTestCase.expected_tokens[32],
                                     kTestCase.code),
                          },
                          IndexingFactType::kMacroCall,
                      },  // refers to macro call TEN.
                      T({
                          {
                              Anchor(kTestCase.expected_tokens[34],
                                     kTestCase.code),
                          },
                          IndexingFactType::kMacroCall,
                      })),
                  // refers to parm x
                  T(
                      {
                          {
                              Anchor(kTestCase.expected_tokens[37],
                                     kTestCase.code),
                          },
                          IndexingFactType::kParamDeclaration,
                      },
                      // refers to macro call TEN.
                      T({
                          {
                              Anchor(kTestCase.expected_tokens[39],
                                     kTestCase.code),
                          },
                          IndexingFactType::kMacroCall,
                      })))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

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

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
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
              })))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

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

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
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
              // anonymous scope for initial.
              T(
                  {
                      {
                          Anchor("anonymous-scope-0", 0, 0),
                      },
                      IndexingFactType ::kAnonymousScope,
                  },
                  // refers to $display(pkg::x).
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[10], kTestCase.code),
                          Anchor(kTestCase.expected_tokens[12], kTestCase.code),
                      },
                      IndexingFactType ::kMemberReference,
                  }))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(PackageImportTest, ClassInstanceWithMultiParams) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {
          "function ",
          {kTag, "m"},
          "();\n",
          {kTag, "foo"},
          "#(.",
          {kTag, "A"},
          "(",
          {kTag, "var1"},
          "), .",
          {kTag, "B"},
          "(",
          {kTag, "var2"},
          "))::",
          {kTag, "barc"},
          "#(.",
          {kTag, "X"},
          "(",
          {kTag, "var1"},
          "), .",
          {kTag, "W"},
          "(",
          {kTag, "var2"},
          "))::",
          {kTag, "get2"},
          "();\nendfunction",
      },
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to function m.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  },
                  IndexingFactType ::kFunctionOrTask,
              },
              // refers foo(.A, .B).
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                      },
                      IndexingFactType ::kMemberReference,
                  },
                  // refers A.
                  T(
                      {
                          {
                              Anchor(kTestCase.expected_tokens[5],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kNamedParam,
                      },
                      // refers var1.
                      T({
                          {
                              Anchor(kTestCase.expected_tokens[7],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kVariableReference,
                      })),
                  // refers B.
                  T(
                      {
                          {
                              Anchor(kTestCase.expected_tokens[9],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kNamedParam,
                      },
                      // refers var2.
                      T({
                          {
                              Anchor(kTestCase.expected_tokens[11],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kVariableReference,
                      }))),
              // refers barc(.X, .W).
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                          Anchor(kTestCase.expected_tokens[13], kTestCase.code),
                      },
                      IndexingFactType ::kMemberReference,
                  },
                  // refers X.
                  T(
                      {
                          {
                              Anchor(kTestCase.expected_tokens[15],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kNamedParam,
                      },
                      // refers var1.
                      T({
                          {
                              Anchor(kTestCase.expected_tokens[17],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kVariableReference,
                      })),
                  // refers W.
                  T(
                      {
                          {
                              Anchor(kTestCase.expected_tokens[19],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kNamedParam,
                      },
                      // refers var2.
                      T({
                          {
                              Anchor(kTestCase.expected_tokens[21],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kVariableReference,
                      }))),
              // refers var2.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                      Anchor(kTestCase.expected_tokens[13], kTestCase.code),
                      Anchor(kTestCase.expected_tokens[23], kTestCase.code),
                  },
                  IndexingFactType ::kFunctionCall,
              })))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(PackageImportTest, UserDefinedType) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"class ",
       {kTag, "Stack"},
       ";\ntypedef int ",
       {kTag, "some_type"},
       ";\nendclass"},
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to class stack.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  },
                  IndexingFactType ::kClass,
              },
              // refers to some_type.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                  },
                  IndexingFactType ::kTypeDeclaration,
              })))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(PackageImportTest, SelectVariableDimension) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {
          "task ",
          {kTag, "t"},
          "(int ",
          {kTag, "y"},
          ");\n@",
          {kTag, "x"},
          "[",
          {kTag, "y"},
          "];\nendtask",
      },
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to task t.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  },
                  IndexingFactType ::kFunctionOrTask,
              },
              // refers to y.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                  },
                  IndexingFactType ::kVariableDefinition,
              }),
              // refers to x.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                  },
                  IndexingFactType ::kVariableReference,
              }),
              // refers to y.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[7], kTestCase.code),
                  },
                  IndexingFactType ::kVariableReference,
              })))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(PackageImportTest, ClassParameterType) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"class ",
       {kTag, "Stack"},
       " #(parameter type ",
       {kTag, "T"},
       "=int);\nendclass"},
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to class stack.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  },
                  IndexingFactType ::kClass,
              },
              // refers to T.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                  },
                  IndexingFactType ::kParamDeclaration,
              })))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(PackageImportTest, ClassInstanceWithParams) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {
          "module ",
          {kTag, "m"},
          "();\n",
          {kTag, "clt"},
          "#(",
          {kTag, "x"},
          ") ",
          {kTag, "v1"},
          " = new();\nendmodule",
      },
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to module m.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  },
                  IndexingFactType ::kModule,
              },
              // refers to clt#(x) v1.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                      },
                      IndexingFactType ::kDataTypeReference,
                  },
                  // refers to x.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                      },
                      IndexingFactType ::kVariableReference,
                  }),
                  // refers to v1.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[7], kTestCase.code),
                      },
                      IndexingFactType ::kClassInstance,
                  }))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(PackageImportTest, PackageTest) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {
          "package ",
          {kTag, "pkg"},
          ";\nendpackage: ",
          {kTag, "pkg"},
      },
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to package pkg.
          T({
              {
                  Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[3], kTestCase.code),
              },
              IndexingFactType ::kPackage,
          }))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, ForLoopInitializations) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"function int ",
       {kTag, "foo"},
       "();\n for (int ",
       {kTag, "i"},
       " = 0, bit ",
       {kTag, "j"},
       " = 0, bit[",
       {kTag, "l"},
       ":",
       {kTag, "r"},
       "] ",
       {kTag, "tm"},
       " = 0; ",
       {kTag, "i"},
       " < 50; ",
       {kTag, "i"},
       "++) begin\n",
       {kTag, "x"},
       "+=",
       {kTag, "i"},
       ";\nend\nreturn ",
       {kTag, "x"},
       ";\n\nendfunction "},
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
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
              // anonymous scope for initial.
              T(
                  {
                      {
                          Anchor("anonymous-scope-0", 0, 0),
                      },
                      IndexingFactType ::kAnonymousScope,
                  },
                  // refers to i
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  }),
                  // refers to j
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  }),
                  // refers to tm
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[11], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  }),
                  // refers to l
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[7], kTestCase.code),
                      },
                      IndexingFactType ::kVariableReference,
                  }),
                  // refers to r
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[9], kTestCase.code),
                      },
                      IndexingFactType ::kVariableReference,
                  }),
                  // refers to i
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[13], kTestCase.code),
                      },
                      IndexingFactType ::kVariableReference,
                  }),
                  // refers to i
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[15], kTestCase.code),
                      },
                      IndexingFactType ::kVariableReference,
                  }),
                  // refers to x
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[17], kTestCase.code),
                      },
                      IndexingFactType ::kVariableReference,
                  }),
                  // refers to i
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[19], kTestCase.code),
                      },
                      IndexingFactType ::kVariableReference,
                  })),
              // refers to x
              T({
                  {
                      Anchor(kTestCase.expected_tokens[21], kTestCase.code),
                  },
                  IndexingFactType ::kVariableReference,
              })))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, ClassExtends) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"class ",
       {kTag, "X"},
       " extends ",
       {kTag, "Y"},
       ";\nendclass\nclass ",
       {kTag, "H"},
       " extends ",
       {kTag, "G"},
       "::",
       {kTag, "K"},
       ";\nendclass"},
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to class X.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  },
                  IndexingFactType ::kClass,
              },
              // refers to Y.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                  },
                  IndexingFactType ::kExtends,
              })),
          // refers to H.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                  },
                  IndexingFactType ::kClass,
              },
              // refers to G::K.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[7], kTestCase.code),
                      Anchor(kTestCase.expected_tokens[9], kTestCase.code),
                  },
                  IndexingFactType ::kExtends,
              })))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, ParameterExtraction) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"module ",
       {kTag, "m"},
       "#(parameter ",
       {kTag, "x"},
       " = 1, parameter ",
       {kTag, "y"},
       " = 2) (input ",
       {kTag, "z"},
       ");\n ",
       {kTag, "bar"},
       " #(.",
       {kTag, "p1"},
       "(",
       {kTag, "x"},
       "), .",
       {kTag, "p2"},
       "(",
       {kTag, "y"},
       ")) ",
       {kTag, "b1"},
       "(",
       {kTag, "z"},
       ");\nendmodule"},
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to module m.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  },
                  IndexingFactType ::kModule,
              },
              // refers to module parameter x.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                  },
                  IndexingFactType ::kParamDeclaration,
              }),
              // refers to class parameter y.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                  },
                  IndexingFactType ::kParamDeclaration,
              }),
              // refers to class input x.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[7], kTestCase.code),
                  },
                  IndexingFactType ::kVariableDefinition,
              }),
              // refers to bar.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[9], kTestCase.code),
                      },
                      IndexingFactType::kDataTypeReference,
                  },
                  // refers to .p1(x).
                  T(
                      {
                          {
                              Anchor(kTestCase.expected_tokens[11],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kNamedParam,
                      },
                      // refers to x.
                      T({
                          {
                              Anchor(kTestCase.expected_tokens[13],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kVariableReference,
                      })),
                  // refers to .p2(y).
                  T(
                      {
                          {
                              Anchor(kTestCase.expected_tokens[15],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kNamedParam,
                      },
                      // refers to y.
                      T({
                          {
                              Anchor(kTestCase.expected_tokens[17],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kVariableReference,
                      })),
                  // refers to b1.
                  T(
                      {
                          {
                              Anchor(kTestCase.expected_tokens[19],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kModuleInstance,
                      },
                      // refers to z.
                      T({
                          {
                              Anchor(kTestCase.expected_tokens[21],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kVariableReference,
                      })))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, InterfaceParameterExtraction) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"interface ",
       {kTag, "m"},
       "#(parameter ",
       {kTag, "x"},
       " = 1, parameter ",
       {kTag, "y"},
       " = 2) (input ",
       {kTag, "z"},
       ");\n ",
       {kTag, "bar"},
       " #(.",
       {kTag, "p1"},
       "(",
       {kTag, "x"},
       "), .",
       {kTag, "p2"},
       "(",
       {kTag, "y"},
       ")) ",
       {kTag, "b1"},
       "(",
       {kTag, "z"},
       ");\nendinterface"},
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to interface m.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  },
                  IndexingFactType ::kInterface,
              },
              // refers to module parameter x.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                  },
                  IndexingFactType ::kParamDeclaration,
              }),
              // refers to class parameter y.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                  },
                  IndexingFactType ::kParamDeclaration,
              }),
              // refers to class input x.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[7], kTestCase.code),
                  },
                  IndexingFactType ::kVariableDefinition,
              }),
              // refers to bar.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[9], kTestCase.code),
                      },
                      IndexingFactType::kDataTypeReference,
                  },
                  // refers to .p1(x).
                  T(
                      {
                          {
                              Anchor(kTestCase.expected_tokens[11],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kNamedParam,
                      },
                      // refers to x.
                      T({
                          {
                              Anchor(kTestCase.expected_tokens[13],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kVariableReference,
                      })),
                  // refers to .p2(y).
                  T(
                      {
                          {
                              Anchor(kTestCase.expected_tokens[15],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kNamedParam,
                      },
                      // refers to y.
                      T({
                          {
                              Anchor(kTestCase.expected_tokens[17],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kVariableReference,
                      })),
                  // refers to b1.
                  T(
                      {
                          {
                              Anchor(kTestCase.expected_tokens[19],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kModuleInstance,
                      },
                      // refers to z.
                      T({
                          {
                              Anchor(kTestCase.expected_tokens[21],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kVariableReference,
                      })))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, ClassAsPort) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"module ",
       {kTag, "m"},
       "(",
       {kTag, "class_type"},
       " ",
       {kTag, "x"},
       ");\nendmodule"},
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to module m.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  },
                  IndexingFactType ::kModule,
              },
              // refers to class_type.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                      },
                      IndexingFactType ::kDataTypeReference,
                  },
                  // refers to x.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  }))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, ProgramParameterExtraction) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"program ",
       {kTag, "m"},
       "#(parameter ",
       {kTag, "x"},
       " = 1, parameter ",
       {kTag, "y"},
       " = 2) (input ",
       {kTag, "z"},
       ");\n ",
       {kTag, "bar"},
       " #(.",
       {kTag, "p1"},
       "(",
       {kTag, "x"},
       "), .",
       {kTag, "p2"},
       "(",
       {kTag, "y"},
       ")) ",
       {kTag, "b1"},
       "(",
       {kTag, "z"},
       ");\nendprogram"},
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to program m.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  },
                  IndexingFactType ::kProgram,
              },
              // refers to module parameter x.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                  },
                  IndexingFactType ::kParamDeclaration,
              }),
              // refers to class parameter y.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                  },
                  IndexingFactType ::kParamDeclaration,
              }),
              // refers to class input x.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[7], kTestCase.code),
                  },
                  IndexingFactType ::kVariableDefinition,
              }),
              // refers to bar.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[9], kTestCase.code),
                      },
                      IndexingFactType::kDataTypeReference,
                  },
                  // refers to .p1(x).
                  T(
                      {
                          {
                              Anchor(kTestCase.expected_tokens[11],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kNamedParam,
                      },
                      // refers to x.
                      T({
                          {
                              Anchor(kTestCase.expected_tokens[13],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kVariableReference,
                      })),
                  // refers to .p2(y).
                  T(
                      {
                          {
                              Anchor(kTestCase.expected_tokens[15],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kNamedParam,
                      },
                      // refers to y.
                      T({
                          {
                              Anchor(kTestCase.expected_tokens[17],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kVariableReference,
                      })),
                  // refers to b1.
                  T(
                      {
                          {
                              Anchor(kTestCase.expected_tokens[19],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kModuleInstance,
                      },
                      // refers to z.
                      T({
                          {
                              Anchor(kTestCase.expected_tokens[21],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kVariableReference,
                      })))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, PackedAndUnpackedDimension) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"package ",
       {kTag, "pkg"},
       ";\nint [",
       {kTag, "k"},
       ":",
       {kTag, "y"},
       "] ",
       {kTag, "x"},
       " [",
       {kTag, "l"},
       ":",
       {kTag, "r"},
       "];\nendpackage\nmodule ",
       {kTag, "m"},
       ";\nint [",
       {kTag, "j"},
       ":",
       {kTag, "o"},
       "] ",
       {kTag, "v"},
       " [",
       {kTag, "e"},
       ":",
       {kTag, "t"},
       "];\nendmodule"},
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
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
              // refers to k.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                  },
                  IndexingFactType ::kVariableReference,
              }),
              // refers to y.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                  },
                  IndexingFactType ::kVariableReference,
              }),
              // refers to x.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[7], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  },
                  // refers to l.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[9], kTestCase.code),
                      },
                      IndexingFactType ::kVariableReference,
                  }),
                  // refers to r.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[11], kTestCase.code),
                      },
                      IndexingFactType ::kVariableReference,
                  }))),
          // refers to module m.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[13], kTestCase.code),
                  },
                  IndexingFactType::kModule,
              },
              // refers to j.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[15], kTestCase.code),
                  },
                  IndexingFactType ::kVariableReference,
              }),
              // refers to o.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[17], kTestCase.code),
                  },
                  IndexingFactType ::kVariableReference,
              }),
              // refers to v.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[19], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  },
                  // refers to e.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[21], kTestCase.code),
                      },
                      IndexingFactType ::kVariableReference,
                  }),
                  // refers to t.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[23], kTestCase.code),
                      },
                      IndexingFactType ::kVariableReference,
                  }))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, FileIncludes) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase0 = {
      {"class ",
       {kTag, "my_class"},
       ";\n static int ",
       {kTag, "var5"},
       ";\n endclass"},
  };

  ScopedTestFile included_test_file(testing::TempDir(), kTestCase0.code);

  std::string filename_included =
      std::string(verible::file::Basename(included_test_file.filename()));
  filename_included = "\"" + filename_included + "\"";

  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"`include ",
       {kTag, filename_included},
       "\n module ",
       {kTag, "my_module"},
       "();\n initial begin\n$display(",
       {kTag, "my_class"},
       "::",
       {kTag, "var5"},
       ");\n "
       "end\nendmodule"},
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(included_test_file.filename(), 0,
                         kTestCase0.code.size()),
                  Anchor(kTestCase0.code, 0, kTestCase0.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to class my_class.
          T(
              {
                  {
                      Anchor(kTestCase0.expected_tokens[1], kTestCase0.code),
                  },
                  IndexingFactType ::kClass,
              },
              // refers to int var5.
              T({
                  {
                      Anchor(kTestCase0.expected_tokens[3], kTestCase0.code),
                  },
                  IndexingFactType ::kVariableDefinition,
              }))),
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to include.
          T({
              {
                  Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  Anchor(included_test_file.filename(), 0, 0),
              },
              IndexingFactType ::kInclude,
          }),
          // refers to module my_module.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                  },
                  IndexingFactType ::kModule,
              },
              // anonymous scope for initial.
              T(
                  {
                      {
                          Anchor("anonymous-scope-0", 0, 0),
                      },
                      IndexingFactType ::kAnonymousScope,
                  },
                  // refers to $display(my_class::var5).
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                          Anchor(kTestCase.expected_tokens[7], kTestCase.code),
                      },
                      IndexingFactType ::kMemberReference,
                  }))))));

  const auto facts_tree = ExtractFiles(
      {std::string(verible::file::Basename(test_file.filename()))},
      testing::TempDir(), verible::file::Dirname(test_file.filename()),
      {std::string(verible::file::Dirname(included_test_file.filename()))},
      errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, FileIncludeSameFileTwice) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase0 = {
      {"class ",
       {kTag, "my_class"},
       ";\n static int ",
       {kTag, "var5"},
       ";\n endclass"},
  };

  ScopedTestFile included_test_file(testing::TempDir(), kTestCase0.code);

  std::string filename_included =
      std::string(verible::file::Basename(included_test_file.filename()));
  filename_included = "\"" + filename_included + "\"";

  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"`include ",
       {kTag, filename_included},
       "\n `include ",
       {kTag, filename_included},
       "\n module ",
       {kTag, "my_module"},
       "();\n initial begin\n$display(",
       {kTag, "my_class"},
       "::",
       {kTag, "var5"},
       ");\n "
       "end\nendmodule"},
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(included_test_file.filename(), 0,
                         kTestCase0.code.size()),
                  Anchor(kTestCase0.code, 0, kTestCase0.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to class my_class.
          T(
              {
                  {
                      Anchor(kTestCase0.expected_tokens[1], kTestCase0.code),
                  },
                  IndexingFactType ::kClass,
              },
              // refers to int var5.
              T({
                  {
                      Anchor(kTestCase0.expected_tokens[3], kTestCase0.code),
                  },
                  IndexingFactType ::kVariableDefinition,
              }))),
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to include.
          T({
              {
                  Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  Anchor(included_test_file.filename(), 0, 0),
              },
              IndexingFactType ::kInclude,
          }),
          // refers to include.
          T({
              {
                  Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                  Anchor(included_test_file.filename(), 0, 0),
              },
              IndexingFactType ::kInclude,
          }),
          // refers to module my_module.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                  },
                  IndexingFactType ::kModule,
              },
              // anonymous scope for initial.
              T(
                  {
                      {
                          Anchor("anonymous-scope-0", 0, 0),
                      },
                      IndexingFactType ::kAnonymousScope,
                  },
                  // refers to $display(my_class::var5).
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[7], kTestCase.code),
                          Anchor(kTestCase.expected_tokens[9], kTestCase.code),
                      },
                      IndexingFactType ::kMemberReference,
                  }))))));

  const auto facts_tree = ExtractFiles(
      {std::string(verible::file::Basename(test_file.filename()))},
      testing::TempDir(), verible::file::Dirname(test_file.filename()),
      {std::string(verible::file::Dirname(included_test_file.filename()))},
      errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, EnumTest) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"package ",
       {kTag, "pkg"},
       ";\nenum {",
       {kTag, "AA"},
       "} ",
       {kTag, "m_var"},
       ";\ntypedef enum {",
       {kTag, "BB"},
       "} ",
       {kTag, "var2"},
       ";\nendpackage\nmodule ",
       {kTag, "m"},
       "();\nenum {",
       {kTag, "CC"},
       "} ",
       {kTag, "m_var"},
       ";\ntypedef enum {",
       {kTag, "DD"},
       "} ",
       {kTag, "var3"},
       ";\nenum {",
       {kTag, "GG"},
       "=",
       {kTag, "y"},
       "[",
       {kTag, "idx"},
       "]} ",
       {kTag, "var6"},
       ";\ntypedef enum {",
       {kTag, "HH"},
       "=",
       {kTag, "yh"},
       "[",
       {kTag, "idx2"},
       "]} ",
       {kTag, "var5"},
       ";\n"
       "endmodule"},
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
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
              // refers to AA.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                  },
                  IndexingFactType ::kConstant,
              }),
              // refers to enum m_var.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                  },
                  IndexingFactType ::kVariableDefinition,
              }),
              // refers to enum var.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[9], kTestCase.code),
                  },
                  IndexingFactType ::kVariableDefinition,
              }),
              // refers to BB.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[7], kTestCase.code),
                  },
                  IndexingFactType ::kConstant,
              })),
          // refers to module m.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[11], kTestCase.code),
                  },
                  IndexingFactType ::kModule,
              },
              // refers to CC.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[13], kTestCase.code),
                  },
                  IndexingFactType ::kConstant,
              }),
              // refers to enum m_var.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[15], kTestCase.code),
                  },
                  IndexingFactType ::kVariableDefinition,
              }),
              // refers to enum var2.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[19], kTestCase.code),
                  },
                  IndexingFactType ::kVariableDefinition,
              }),
              // refers to DD.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[17], kTestCase.code),
                  },
                  IndexingFactType ::kConstant,
              }),
              // refers to GG.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[21], kTestCase.code),
                      },
                      IndexingFactType ::kConstant,
                  },
                  // refers to y.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[23], kTestCase.code),
                      },
                      IndexingFactType ::kVariableReference,
                  }),
                  // refers to idx.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[25], kTestCase.code),
                      },
                      IndexingFactType ::kVariableReference,
                  })),
              // refers to enum var3.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[27], kTestCase.code),
                  },
                  IndexingFactType ::kVariableDefinition,
              }),
              // refers to enum var5.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[35], kTestCase.code),
                  },
                  IndexingFactType ::kVariableDefinition,
              }),
              // refers to HH.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[29], kTestCase.code),
                      },
                      IndexingFactType ::kConstant,
                  },
                  // refers to yh.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[31], kTestCase.code),
                      },
                      IndexingFactType ::kVariableReference,
                  }),
                  // refers to idx2.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[33], kTestCase.code),
                      },
                      IndexingFactType ::kVariableReference,
                  }))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, NonLiteralIncludeSafeFail) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      // no facts should be extracted for this include.
      {"`include `c\nmodule ",
       {kTag, "pkg"},
       ";\n struct {int ",
       {kTag, "xx"},
       ";} ",
       {kTag, "var1"},
       ";\nendmodule"},
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to module pkg.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  },
                  IndexingFactType::kModule,
              },
              // refers to var1.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  },
                  // refers to x.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  }))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, StructInModule) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"module ",
       {kTag, "pkg"},
       ";\n struct {int ",
       {kTag, "xx"},
       ";} ",
       {kTag, "var1"},
       ";\nendmodule"},
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to module pkg.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  },
                  IndexingFactType::kModule,
              },
              // refers to var1.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  },
                  // refers to x.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  }))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, ClassConstructor) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"class ",
       {kTag, "my_class"},
       ";\nfunction ",
       {kTag, "new"},
       "(int ",
       {kTag, "x"},
       ");\n",
       {kTag, "x"},
       " = 1;\nendfunction\nendclass"},
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to class my_class.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  },
                  IndexingFactType::kClass,
              },
              // refers to new.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                      },
                      IndexingFactType ::kConstructor,
                  },
                  // refers to x.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  }),
                  // refers to x.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[7], kTestCase.code),
                      },
                      IndexingFactType ::kVariableReference,
                  }))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, StructInPackage) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"package ",
       {kTag, "pkg"},
       ";\n struct {int ",
       {kTag, "xx"},
       ";} ",
       {kTag, "var1"},
       ";\nendpackage"},
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
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
              // refers to var1.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  },
                  // refers to x.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  }))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, UnionInModule) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"module ",
       {kTag, "pkg"},
       ";\n union {int ",
       {kTag, "xx"},
       ";} ",
       {kTag, "var1"},
       ";\nendmodule"},
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to module pkg.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  },
                  IndexingFactType::kModule,
              },
              // refers to var1.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  },
                  // refers to x.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  }))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, UnionInPackage) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"package ",
       {kTag, "pkg"},
       ";\n union {int ",
       {kTag, "xx"},
       ";} ",
       {kTag, "var1"},
       ";\nendpackage"},
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
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
              // refers to var1.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  },
                  // refers to x.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  }))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, UnionTypeInPackage) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"package ",
       {kTag, "pkg"},
       ";\n typedef union {int ",
       {kTag, "xx"},
       ";} ",
       {kTag, "var1"},
       ";\nendpackage"},
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
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
              // refers to var1.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                      },
                      IndexingFactType ::kStructOrUnion,
                  },
                  // refers to x.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  }))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, UnionTypenModule) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"module ",
       {kTag, "pkg"},
       ";\n typedef union {int ",
       {kTag, "xx"},
       ";} ",
       {kTag, "var1"},
       ";\nendmodule"},
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to module pkg.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  },
                  IndexingFactType::kModule,
              },
              // refers to var1.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                      },
                      IndexingFactType ::kStructOrUnion,
                  },
                  // refers to x.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  }))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, StructTypeInPackage) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"package ",
       {kTag, "pkg"},
       ";\n typedef struct {int ",
       {kTag, "xx"},
       ";} ",
       {kTag, "var1"},
       ";\nendpackage"},
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
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
              // refers to var1.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                      },
                      IndexingFactType ::kStructOrUnion,
                  },
                  // refers to x.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  }))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, StructTypedefModule) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"module ",
       {kTag, "pkg"},
       ";\n typedef struct {int ",
       {kTag, "xx"},
       ";} ",
       {kTag, "var1"},
       ";\nendmodule"},
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to module pkg.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  },
                  IndexingFactType::kModule,
              },
              // refers to var1.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                      },
                      IndexingFactType ::kStructOrUnion,
                  },
                  // refers to x.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  }))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, DataTypeReferenceInUnionType) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"typedef struct {\n ",
       {kTag, "some_type"},
       " ",
       {kTag, "var1"},
       ";}",
       {kTag, "var2"},
       ";"},
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to struct var1.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                  },
                  IndexingFactType::kStructOrUnion,
              },
              // refers to some_type var1.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                      },
                      IndexingFactType ::kDataTypeReference,
                  },
                  // refers to var1.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  }))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, StructInUnionType) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"typedef union {\n struct {int ",
       {kTag, "xx"},
       ";} ",
       {kTag, "var1"},
       ";}",
       {kTag, "var2"},
       ";"},
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to union var1.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                  },
                  IndexingFactType::kStructOrUnion,
              },
              // refers to struct var2.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  },
                  // refers to x.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  }))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, StructInUnion) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"union {\n struct {int ",
       {kTag, "xx"},
       ";} ",
       {kTag, "var1"},
       ";}",
       {kTag, "var2"},
       ";"},
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to union var1.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                  },
                  IndexingFactType::kVariableDefinition,
              },
              // refers to struct var2.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  },
                  // refers to x.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  }))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, UnionInStructType) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"typedef struct {\n union {int ",
       {kTag, "xx"},
       ";} ",
       {kTag, "var1"},
       ";}",
       {kTag, "var2"},
       ";"},
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to union var1.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                  },
                  IndexingFactType::kStructOrUnion,
              },
              // refers to struct var2.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  },
                  // refers to x.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  }))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, UnionInStruct) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"struct {\n union {int ",
       {kTag, "xx"},
       ";} ",
       {kTag, "var1"},
       ";}",
       {kTag, "var2"},
       ";"},
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to union var1.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                  },
                  IndexingFactType::kVariableDefinition,
              },
              // refers to struct var2.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  },
                  // refers to x.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  }))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, TypedVariable) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {
          "package ",
          {kTag, "m"},
          ";\n",
          {kTag, "some_type"},
          " ",
          {kTag, "var1"},
          ";\nendpackage\nmodule ",
          {kTag, "m"},
          "();\n",
          {kTag, "some_type1"},
          " ",
          {kTag, "var2"},
          ";\nendmodule",
      },
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to package m.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  },
                  IndexingFactType ::kPackage,
              },
              // refers to some_type.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                      },
                      IndexingFactType::kDataTypeReference,
                  },
                  // refers to var1.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  }))),
          // refers to module m.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[7], kTestCase.code),
                  },
                  IndexingFactType ::kModule,
              },
              // refers to some_type1.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[9], kTestCase.code),
                      },
                      IndexingFactType::kDataTypeReference,
                  },
                  // refers to var2.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[11], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  }))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, FunctionNameAsQualifiedId) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {
          "function ",
          {kTag, "pkg"},
          "::",
          {kTag, "f"},
          ";\nendfunction\ntask ",
          {kTag, "pkg"},
          "::",
          {kTag, "t"},
          ";\nendtask",
      },
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to function pkg::f.
          T({
              {
                  Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[3], kTestCase.code),
              },
              IndexingFactType ::kFunctionOrTask,
          }),
          // refers to task pkg::t.
          T({
              {
                  Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                  Anchor(kTestCase.expected_tokens[7], kTestCase.code),
              },
              IndexingFactType ::kFunctionOrTask,
          }))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, BuiltInFunction) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"localparam ",
       {kTag, "x"},
       " = sin(1);\ntask ",
       {kTag, "t1"},
       "();\n",
       {kTag, "b1"},
       "(sin());\nendtask\ntask ",
       {kTag, "t"},
       "(input ",
       {kTag, "foo"},
       " ",
       {kTag, "bar"},
       "[$]);\n",
       {kTag, "bar"},
       ".sort();\nendtask"},
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to param x.
          T({
              {
                  Anchor(kTestCase.expected_tokens[1], kTestCase.code),
              },
              IndexingFactType ::kParamDeclaration,
          }),
          // refers to t1.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                  },
                  IndexingFactType ::kFunctionOrTask,
              },
              // refers to b1.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                  },
                  IndexingFactType ::kFunctionCall,
              })),
          // refers to t.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[7], kTestCase.code),
                  },
                  IndexingFactType ::kFunctionOrTask,
              },
              // refers to foo.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[9], kTestCase.code),
                      },
                      IndexingFactType ::kDataTypeReference,
                  },
                  // refers to bar.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[11], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  })),
              // refers to bar.sort().
              T({
                  {
                      Anchor(kTestCase.expected_tokens[13], kTestCase.code),
                  },
                  IndexingFactType ::kVariableReference,
              })))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, PureVirtualFunction) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"class ",
       {kTag, "env"},
       ";\npure virtual function int ",
       {kTag, "mod_if"},
       "( ",
       {kTag, "x"},
       ");\nendclass"},
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to class env.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  },
                  IndexingFactType ::kClass,
              },
              // refers to function mod_if.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                      },
                      IndexingFactType ::kFunctionOrTaskForwardDeclaration,
                  },
                  // refers to x.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  }))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, ExternFunction) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"class ",
       {kTag, "env"},
       ";\nextern function int ",
       {kTag, "mod_if"},
       "( ",
       {kTag, "x"},
       ");\nendclass"},
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to class env.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  },
                  IndexingFactType ::kClass,
              },
              // refers to function mod_if.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                      },
                      IndexingFactType ::kFunctionOrTaskForwardDeclaration,
                  },
                  // refers to x.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  }))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, ExternTask) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"class ",
       {kTag, "env"},
       ";\nextern task ",
       {kTag, "mod_if"},
       "( ",
       {kTag, "x"},
       ");\nendclass"},
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to class env.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  },
                  IndexingFactType ::kClass,
              },
              // refers to function mod_if.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                      },
                      IndexingFactType ::kFunctionOrTaskForwardDeclaration,
                  },
                  // refers to x.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  }))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, PureVirtualTask) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"class ",
       {kTag, "env"},
       ";\npure virtual task ",
       {kTag, "mod_if"},
       "( ",
       {kTag, "x"},
       ");\nendclass"},
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to class env.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  },
                  IndexingFactType ::kClass,
              },
              // refers to task mod_if.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                      },
                      IndexingFactType ::kFunctionOrTaskForwardDeclaration,
                  },
                  // refers to x.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  }))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, VirtualDataDeclaration) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"class ",
       {kTag, "env"},
       " extends ",
       {kTag, "uvm_env"},
       ";\nvirtual ",
       {kTag, "mod_if"},
       " ",
       {kTag, "m_if"},
       ";\nendclass"},
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to class env.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  },
                  IndexingFactType ::kClass,
              },
              // refers to extends uvm_env.
              T({
                  {
                      Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                  },
                  IndexingFactType ::kExtends,
              }),
              // refers to mod_if.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                      },
                      IndexingFactType ::kDataTypeReference,
                  },
                  // refers to m_if.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[7], kTestCase.code),
                      },
                      IndexingFactType ::kVariableDefinition,
                  }))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, FunctionNamedArgument) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {
          "function void ",
          {kTag, "f1"},
          "();\n",
          {kTag, "f2"},
          "(.",
          {kTag, "a"},
          "(",
          {kTag, "x"},
          "), .",
          {kTag, "b"},
          "(",
          {kTag, "y"},
          "));\nendfunction",
      },
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to function f1.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  },
                  IndexingFactType ::kFunctionOrTask,
              },
              // refers to f2.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                      },
                      IndexingFactType::kFunctionCall,
                  },
                  // refers to a.
                  T(
                      {
                          {
                              Anchor(kTestCase.expected_tokens[5],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kNamedParam,
                      },
                      // refers to x.
                      T({
                          {
                              Anchor(kTestCase.expected_tokens[7],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kVariableReference,
                      })),
                  // refers to b.
                  T(
                      {
                          {
                              Anchor(kTestCase.expected_tokens[9],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kNamedParam,
                      },
                      // refers to y.
                      T({
                          {
                              Anchor(kTestCase.expected_tokens[11],
                                     kTestCase.code),
                          },
                          IndexingFactType ::kVariableReference,
                      })))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, FunctionPortPackedAndUnpackedDimsensions) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {
          "function void ",
          {kTag, "f1"},
          "(int [",
          {kTag, "x"},
          ":",
          {kTag, "y"},
          "] ",
          {kTag, "t"},
          " [",
          {kTag, "l"},
          ":",
          {kTag, "r"},
          "]);\nendfunction",
      },
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to function f1.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  },
                  IndexingFactType ::kFunctionOrTask,
              },
              // refers to t.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[7], kTestCase.code),
                      },
                      IndexingFactType::kVariableDefinition,
                  },
                  // refers to x.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                      },
                      IndexingFactType::kVariableReference,
                  }),
                  // refers to y.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                      },
                      IndexingFactType::kVariableReference,
                  }),
                  // refers to l.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[9], kTestCase.code),
                      },
                      IndexingFactType::kVariableReference,
                  }),
                  // refers to r.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[11], kTestCase.code),
                      },
                      IndexingFactType::kVariableReference,
                  }))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, UserDefinedTypeFunctionPort) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {
          "function void ",
          {kTag, "f1"},
          "(",
          {kTag, "foo"},
          " ",
          {kTag, "bar"},
          ");\nendfunction",
      },
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to function f1.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  },
                  IndexingFactType ::kFunctionOrTask,
              },
              // refers to foo.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                      },
                      IndexingFactType::kDataTypeReference,
                  },
                  // refers to bar.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                      },
                      IndexingFactType::kVariableDefinition,
                  }))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, StructFunctionPort) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {
          "function void ",
          {kTag, "f1"},
          "(struct {int ",
          {kTag, "foo"},
          ";} ",
          {kTag, "bar"},
          ");\nendfunction",
      },
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to function f1.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  },
                  IndexingFactType ::kFunctionOrTask,
              },
              // refers to bar.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[5], kTestCase.code),
                      },
                      IndexingFactType::kVariableDefinition,
                  },
                  // refers to foo.
                  T({
                      {
                          Anchor(kTestCase.expected_tokens[3], kTestCase.code),
                      },
                      IndexingFactType::kVariableDefinition,
                  }))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, NestedStructFunctionPort) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {
          "function void ",
          {kTag, "f1"},
          "(struct {struct {int ",
          {kTag, "y"},
          ";} ",
          {kTag, "foo"},
          ";} ",
          {kTag, "bar"},
          ");\nendfunction",
      },
  };

  ScopedTestFile test_file(testing::TempDir(), kTestCase.code);
  std::vector<absl::Status> errors;

  const IndexingFactNode expected(T(
      {
          {
              Anchor(testing::TempDir(), 0, 0),
              Anchor(verible::file::Dirname(test_file.filename()), 0, 0),
          },
          IndexingFactType::kFileList,
      },
      T(
          {
              {
                  Anchor(test_file.filename(), 0, kTestCase.code.size()),
                  Anchor(kTestCase.code, 0, kTestCase.code.size()),
              },
              IndexingFactType ::kFile,
          },
          // refers to function f1.
          T(
              {
                  {
                      Anchor(kTestCase.expected_tokens[1], kTestCase.code),
                  },
                  IndexingFactType ::kFunctionOrTask,
              },
              // refers to bar.
              T(
                  {
                      {
                          Anchor(kTestCase.expected_tokens[7], kTestCase.code),
                      },
                      IndexingFactType::kVariableDefinition,
                  },
                  // refers to foo.
                  T(
                      {
                          {
                              Anchor(kTestCase.expected_tokens[5],
                                     kTestCase.code),
                          },
                          IndexingFactType::kVariableDefinition,
                      },
                      // refers to y.
                      T({
                          {
                              Anchor(kTestCase.expected_tokens[3],
                                     kTestCase.code),
                          },
                          IndexingFactType::kVariableDefinition,
                      })))))));

  const auto facts_tree =
      ExtractFiles({std::string(verible::file::Basename(test_file.filename()))},
                   testing::TempDir(),
                   verible::file::Dirname(test_file.filename()), {}, errors);

  const auto result_pair = DeepEqual(facts_tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

}  // namespace
}  // namespace kythe
}  // namespace verilog
