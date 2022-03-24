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

#include <functional>

#include "absl/status/status.h"
#include "common/analysis/syntax_tree_search_test_utils.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/util/file_util.h"
#include "common/util/range.h"
#include "common/util/tree_operations.h"
#include "gtest/gtest.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/analysis/verilog_project.h"
#include "verilog/tools/kythe/indexing_facts_tree.h"
#include "verilog/tools/kythe/verilog_extractor_indexing_fact_type.h"

#undef EXPECT_OK
#define EXPECT_OK(value) EXPECT_TRUE((value).ok())

namespace verilog {
namespace kythe {
namespace {

using verible::file::testing::ScopedTestFile;

typedef IndexingFactNode T;
typedef IndexingNodeData D;

// This class exists, solely to store the temp dir name in a variable that
// outlives its uses.  Do not pass TempDir() directly anywhere that expects a
// string_view to be owned elsewhere, because it returns a temporary string,
// which would leave a dangling reference.
class TempDir {
 protected:
  TempDir() : temp_dir_(::testing::TempDir()) {}

 protected:
  const std::string temp_dir_;
};

// Groups information for a single temporary source file together.
struct TestFileEntry {
  // This is a view of the original source code of the translation as it was
  // passed in through the constructor.  VerilogProject::ExtractFiles() works
  // with an internal copy of this code as SourceText().
  // This copy is useful for rebasing the expected finding tokens into that of
  // the internal buffer.
  const absl::string_view origin_text;

  // One file in project, will be given randomly generated name.
  const ScopedTestFile temp_file;

  // Points to a successfully opened source file that corresponds to test_file_
  // opened as a translation unit.
  const VerilogSourceFile* const source_file = nullptr;

  TestFileEntry(
      absl::string_view code_text, absl::string_view temp_dir,
      const std::function<const VerilogSourceFile*(absl::string_view)>&
          file_opener,
      absl::string_view override_basename = "")
      : origin_text(code_text),
        temp_file(temp_dir, origin_text, override_basename),
        // Open this file from inside 'project', save pointer here.
        source_file(file_opener(temp_file.filename())) {}

  // Returns the string_view of text owned by this->source_file.
  absl::string_view SourceText() const {
    return source_file->GetTextStructure()->Contents();
  }

  T::value_type ExpectedFileData() const {
    return {
        IndexingFactType::kFile,
        Anchor(source_file->ResolvedPath()),
        Anchor(SourceText()),
    };
  }

  // Transform a tree so that the string_views point to the copy of the source
  // code that lives inside 'source_file', and not the 'origin_text' it came
  // from.
  T RebaseFileTree(T original_tree) const {
    VLOG(3) << __FUNCTION__;
    // This is the pointer difference between two copies of the same text.
    const std::ptrdiff_t delta =
        std::distance(origin_text.begin(), SourceText().begin());
    // mutate in-place, pre vs. post order doesn't matter here
    ApplyPreOrder(original_tree, [delta](T::value_type& data) {
      data.RebaseStringViewsForTesting(delta);
    });
    VLOG(3) << "end of " << __FUNCTION__;
    return original_tree;  // move
  }
};

// SimpleTestProject create a single-file project for testing.
// No include files supported in this base class.
class SimpleTestProject : public TempDir, public VerilogProject {
 public:
  // 'code_text' is the contents of the single translation unit in this project
  explicit SimpleTestProject(absl::string_view code_text,
                             const std::vector<std::string>& include_paths = {})
      : TempDir(),
        VerilogProject(temp_dir_, include_paths),
        translation_unit_(code_text, temp_dir_,
                          [this](absl::string_view full_file_name)
                              -> const VerilogSourceFile* {
                            /* VerilogProject base class is already fully
                             * initialized */
                            return *OpenTranslationUnit(
                                verible::file::Basename(full_file_name));
                          }) {}

  const TestFileEntry& XUnit() const { return translation_unit_; }

  absl::string_view OnlyFileName() const {
    return translation_unit_.temp_file.filename();
  }

  T Extract() {
    return ExtractFiles(
        /* file_list_path= */ temp_dir_,  // == Dirname(OnlyFileName()),
        /* project= */ this,
        /* file_names= */
        {std::string(verible::file::Basename(OnlyFileName()))});
  }

  T::value_type ExpectedFileListData() const {
    return {
        IndexingFactType::kFileList,  //
        Anchor(absl::string_view(temp_dir_)),
        // file_list is co-located with the files it references
        Anchor(TranslationUnitRoot()),
        // TranslationUnitRoot() == verible::file::Dirname(OnlyFileName())
        // but in a different string_view range.
    };
  }

  absl::string_view TranslationUnitSourceText() const {
    return translation_unit_.SourceText();
  }

  // This printer only works well for tests cases with one translation unit.
  PrintableIndexingFactNode TreePrinter(const IndexingFactNode& n) const {
    return PrintableIndexingFactNode(n, TranslationUnitSourceText());
  }

  using VerilogProject::GetErrorStatuses;

 private:
  // The one and only translation unit under test.
  TestFileEntry translation_unit_;
};

// Test project with one source translation unit, one included file.
class IncludeTestProject : protected SimpleTestProject {
 public:
  // 'code_text' is the contents of the single translation unit in this project.
  // 'include_text' is the contents of the single included file in this project.
  IncludeTestProject(absl::string_view code_text,
                     absl::string_view include_file_basename,
                     absl::string_view include_text,
                     const std::vector<std::string>& include_paths = {})
      : SimpleTestProject(code_text, include_paths),
        include_file_(
            include_text, temp_dir_,
            [this](
                absl::string_view full_file_name) -> const VerilogSourceFile* {
              /* VerilogProject base class is already fully initialized */
              return *OpenIncludedFile(verible::file::Basename(full_file_name));
            },
            include_file_basename) {}

  using SimpleTestProject::ExpectedFileListData;
  using SimpleTestProject::Extract;
  using SimpleTestProject::GetErrorStatuses;
  using SimpleTestProject::XUnit;

  const TestFileEntry& IncludeFile() const { return include_file_; }

 private:
  // The one and only one included file in the test.
  TestFileEntry include_file_;
};

TEST(FactsTreeExtractor, EqualOperatorTest) {
  constexpr absl::string_view code_text = "some other code";
  constexpr absl::string_view file_name = "verilog.v";
  constexpr absl::string_view file_name_other = "other.v";

  const IndexingFactNode expected(D{
      IndexingFactType::kFile,
      Anchor(file_name),
  });

  // same as expected
  const IndexingFactNode tree(D{
      IndexingFactType::kFile,
      Anchor(file_name),
  });

  // different filename
  const IndexingFactNode tree2(D{
      IndexingFactType::kFile,
      Anchor(file_name_other),
  });

  // different number of anchors
  const IndexingFactNode tree3(D{
      IndexingFactType::kFile,
      Anchor(file_name),
      Anchor(file_name),
  });

  // completely different string_view ranges
  const IndexingFactNode tree4(
      D{
          IndexingFactType::kFile,
          Anchor(file_name),
      },
      T(D{
          IndexingFactType::kModule,
          Anchor(absl::string_view("foo")),
          Anchor(absl::string_view("foo")),
      }));

  const auto P = [&code_text](const IndexingFactNode& n) {
    return PrintableIndexingFactNode(n, code_text);
  };
  {
    const auto result_pair = DeepEqual(tree, expected);
    EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
    EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
  }
  {
    const auto result_pair2 = DeepEqual(tree2, expected);
    EXPECT_NE(result_pair2.left, nullptr) << P(*result_pair2.left);
    EXPECT_NE(result_pair2.right, nullptr) << P(*result_pair2.right);
  }
  {
    const auto result_pair3 = DeepEqual(tree3, expected);
    EXPECT_NE(result_pair3.left, nullptr) << P(*result_pair3.left);
    EXPECT_NE(result_pair3.right, nullptr) << P(*result_pair3.right);
  }
  {
    const auto result_pair4 = DeepEqual(tree4, expected);
    EXPECT_NE(result_pair4.left, nullptr) << P(*result_pair4.left);
    EXPECT_NE(result_pair4.right, nullptr) << P(*result_pair4.right);
  }
}

TEST(FactsTreeExtractor, EmptyCSTTest) {
  constexpr absl::string_view code_text = "";

  SimpleTestProject project(code_text);

  const IndexingFactNode expected(     //
      project.ExpectedFileListData(),  //
      T(project.XUnit().ExpectedFileData()));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
}

TEST(FactsTreeExtractor, ParseErrorTest) {
  // These inputs are lexically or syntactically invalid.
  constexpr absl::string_view code_texts[] = {
      "9badid foo;\n",      // lexical error
      "final v;\n",         // syntax error
      "module unfinished",  // syntax error
  };

  for (const auto& code_text : code_texts) {
    SimpleTestProject project(code_text);
    const auto facts_tree = project.Extract();
    const std::vector<absl::Status> errors(project.GetErrorStatuses());
    EXPECT_FALSE(errors.empty()) << "code\n" << code_text;
  }
}

TEST(FactsTreeExtractor, EmptyModuleTest) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"module ", {kTag, "foo"}, ";\n endmodule: ", {kTag, "foo"}}};

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to module foo.
            T(D{
                IndexingFactType::kModule,
                Anchor(kTestCase.expected_tokens[1]),
                Anchor(kTestCase.expected_tokens[3]),
            }))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to module foo.
            T(
                D{
                    IndexingFactType::kModule,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to My_type.
                T(
                    D{
                        IndexingFactType::kDataTypeReference,
                        Anchor(kTestCase.expected_tokens[3]),
                    },
                    // refers to x.
                    T(D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[5]),
                    }),
                    // refers to y.
                    T(D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[7]),
                    }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to module bar.
            T(
                D{
                    IndexingFactType::kModule,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to w.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[3]),
                }),
                // refers to `x
                // TODO(minatoma): suppress declarations that use macro
                // identifiers because they evaluate to something unknown.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[5]),
                })))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to module bar.
            T(D{
                IndexingFactType::kModule,
                Anchor(kTestCase.expected_tokens[1]),
                Anchor(kTestCase.expected_tokens[3]),
            }),
            // refers to module foo.
            T(
                D{
                    IndexingFactType::kModule,
                    Anchor(kTestCase.expected_tokens[5]),
                    Anchor(kTestCase.expected_tokens[11]),
                },
                // refers to bar.
                T(
                    D{
                        IndexingFactType::kDataTypeReference,
                        Anchor(kTestCase.expected_tokens[7]),
                    },
                    // refers to bar b1().
                    T(D{
                        IndexingFactType::kModuleInstance,
                        Anchor(kTestCase.expected_tokens[9]),
                    }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to module bar.
            T(D{
                IndexingFactType::kModule,
                Anchor(kTestCase.expected_tokens[1]),
                Anchor(kTestCase.expected_tokens[3]),
            }),
            // refers to module foo.
            T(
                D{
                    IndexingFactType::kModule,
                    Anchor(kTestCase.expected_tokens[5]),
                    Anchor(kTestCase.expected_tokens[15]),
                },
                // refers to bar.
                T(
                    D{
                        IndexingFactType::kDataTypeReference,
                        Anchor(kTestCase.expected_tokens[7]),
                    },
                    // refers to bar b1().
                    T(D{
                        IndexingFactType::kModuleInstance,
                        Anchor(kTestCase.expected_tokens[9]),
                    })),
                // refers to bar.
                T(
                    D{
                        IndexingFactType::kDataTypeReference,
                        Anchor(kTestCase.expected_tokens[11]),
                    },
                    // refers to bar b2().
                    T(D{
                        IndexingFactType::kModuleInstance,
                        Anchor(kTestCase.expected_tokens[13]),
                    }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to module bar.
            T(D{
                IndexingFactType::kModule,
                Anchor(kTestCase.expected_tokens[1]),
                Anchor(kTestCase.expected_tokens[3]),
            }),
            // refers to module foo.
            T(
                D{
                    IndexingFactType::kModule,
                    Anchor(kTestCase.expected_tokens[5]),
                    Anchor(kTestCase.expected_tokens[13]),
                },
                // refers to bar.
                T(
                    D{
                        IndexingFactType::kDataTypeReference,
                        Anchor(kTestCase.expected_tokens[7]),
                    },
                    // refers to b1().
                    T(D{
                        IndexingFactType::kModuleInstance,
                        Anchor(kTestCase.expected_tokens[9]),
                    }),
                    // refers to bar b2().
                    T(D{
                        IndexingFactType::kModuleInstance,
                        Anchor(kTestCase.expected_tokens[11]),
                    }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to module foo.
            T(
                D{
                    IndexingFactType::kModule,
                    Anchor(kTestCase.expected_tokens[1]),
                    Anchor(kTestCase.expected_tokens[7]),
                },
                // refers to input a.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[3]),
                }),
                // refers to output b.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[5]),
                })))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to module foo.
            T(
                D{
                    IndexingFactType::kModule,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to input a.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[3]),
                }),
                // refers to output b.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[9]),
                }),
                // refers to input x.
                T(D{
                    IndexingFactType::kVariableReference,
                    Anchor(kTestCase.expected_tokens[5]),
                }),
                // refers to input y.
                T(D{
                    IndexingFactType::kVariableReference,
                    Anchor(kTestCase.expected_tokens[7]),
                }),

                // refers to input x.
                T(D{
                    IndexingFactType::kVariableReference,
                    Anchor(kTestCase.expected_tokens[11]),
                }),
                // refers to input y.
                T(D{
                    IndexingFactType::kVariableReference,
                    Anchor(kTestCase.expected_tokens[13]),
                })))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to module foo.
            T(
                D{
                    IndexingFactType::kModule,
                    Anchor(kTestCase.expected_tokens[1]),
                    Anchor(kTestCase.expected_tokens[11]),
                },
                // refers to  a.
                T(D{
                    IndexingFactType::kVariableReference,
                    Anchor(kTestCase.expected_tokens[3]),
                }),
                // refers to  b.
                T(D{
                    IndexingFactType::kVariableReference,
                    Anchor(kTestCase.expected_tokens[5]),
                }),
                // refers to input z.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[7]),
                }),
                // refers to h.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[9]),
                })))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to class my_class.
            T(
                D{
                    IndexingFactType::kClass,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to parameter x
                T(D{
                    IndexingFactType::kParamDeclaration,
                    Anchor(kTestCase.expected_tokens[3]),
                }),
                // refers to y
                T(D{
                    IndexingFactType::kParamDeclaration,
                    Anchor(kTestCase.expected_tokens[5]),
                })))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to module m.
            T(
                D{
                    IndexingFactType::kModule,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to pkg::X
                T(
                    D{
                        IndexingFactType::kDataTypeReference,
                        Anchor(kTestCase.expected_tokens[3]),
                        Anchor(kTestCase.expected_tokens[5]),
                    },
                    // refers to y
                    T(D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[7]),
                    })),
                // refers to pkg::H
                T(
                    D{
                        IndexingFactType::kDataTypeReference,
                        Anchor(kTestCase.expected_tokens[9]),
                        Anchor(kTestCase.expected_tokens[11]),
                    },
                    // refers to j
                    T(D{
                        IndexingFactType::kClassInstance,
                        Anchor(kTestCase.expected_tokens[13]),
                    }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to class my_class.
            T(
                D{
                    IndexingFactType::kClass,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to parameter x
                T(D{
                    IndexingFactType::kParamDeclaration,
                    Anchor(kTestCase.expected_tokens[3]),
                }),
                // refers to y
                T(D{
                    IndexingFactType::kParamDeclaration,
                    Anchor(kTestCase.expected_tokens[5]),
                })))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
}

TEST(FactsTreeExtractor, ModuleInstanceWithActualNamedPorts) {
  constexpr int kTag = 1;  // value doesn't matter

  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"module ",          // 0
       {kTag, "foo"},      // 1
       "(input ",          // 2
       {kTag, "a"},        // 3
       ", input ",         // 4
       {kTag, "b"},        // 5
       ", input wire ",    // 6
       {kTag, "z"},        // 7
       ", output ",        // 8
       {kTag, "h"},        // 9
       ");\nendmodule: ",  // 10
       {kTag, "foo"},      // 11
       "\nmodule ",        // 12
       {kTag, "bar"},      // 13
       "(input ",          // 14
       {kTag, "a"},        // 15
       ", ",               // 16
       {kTag, "b"},        // 17
       ", ",               // 18
       {kTag, "c"},        // 19
       ", ",               // 20
       {kTag, "h"},        // 21
       ");\n",             // 22
       {kTag, "foo"},      // 23
       " ",                // 24
       {kTag, "f1"},       // 25
       "(.",               // 26
       {kTag, "a"},        // 27
       "(",                // 28
       {kTag, "a"},        // 29
       "), .",             // 30
       {kTag, "b"},        // 31
       "(",                // 32
       {kTag, "b"},        // 33
       "), .",             // 34
       {kTag, "z"},        // 35
       "(",                // 36
       {kTag, "c"},        // 37
       "), .",             // 38
       {kTag, "h"},        // 39
       ");\nendmodule"}};

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(
          T(project.XUnit().ExpectedFileData(),
            // refers to module foo.
            T(
                D{
                    IndexingFactType::kModule,
                    Anchor(kTestCase.expected_tokens[1]),
                    Anchor(kTestCase.expected_tokens[11]),
                },
                // refers to  a.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[3]),
                }),
                // refers to  b.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[5]),
                }),
                // refers to input z.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[7]),
                }),
                // refers to h.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[9]),
                })),
            // refers to module bar.
            T(
                D{
                    IndexingFactType::kModule,
                    Anchor(kTestCase.expected_tokens[13]),
                },
                // refers to input a.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[15]),
                }),
                // refers to b.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[17]),
                }),
                // refers to c.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[19]),
                }),
                // refers to h.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[21]),
                }),
                // refers to foo.
                T(
                    D{
                        IndexingFactType::kDataTypeReference,
                        Anchor(kTestCase.expected_tokens[23]),
                    },
                    // refers to f1(.a(a), .b(b), .z(c), .h).
                    T(
                        D{
                            IndexingFactType::kModuleInstance,
                            Anchor(kTestCase.expected_tokens[25]),
                        },
                        // refers to .a
                        T(
                            D{
                                IndexingFactType::kModuleNamedPort,
                                Anchor(kTestCase.expected_tokens[27]),
                            },
                            // refers to a
                            T(D{
                                IndexingFactType::kVariableReference,
                                Anchor(kTestCase.expected_tokens[29]),
                            })),
                        // refers to .b
                        T(
                            D{
                                IndexingFactType::kModuleNamedPort,
                                Anchor(kTestCase.expected_tokens[31]),
                            },
                            // refers to b
                            T(D{
                                IndexingFactType::kVariableReference,
                                Anchor(kTestCase.expected_tokens[33]),
                            })),
                        // refers to .z
                        T(
                            D{
                                IndexingFactType::kModuleNamedPort,
                                Anchor(kTestCase.expected_tokens[35]),
                            },
                            // refers to c
                            T(D{
                                IndexingFactType::kVariableReference,
                                Anchor(kTestCase.expected_tokens[37]),
                            })),
                        // refers to .h
                        T(D{
                            IndexingFactType::kModuleNamedPort,
                            Anchor(kTestCase.expected_tokens[39]),
                        })))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
}

TEST(FactsTreeExtractor, ModuleInstanceWitStarExpandedPorts) {
  constexpr int kTag = 1;  // value doesn't matter

  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"module ",          // 00
       {kTag, "foo"},      // 01
       "(input ",          // 02
       {kTag, "a"},        // 03
       ");\nendmodule\n",  // 04
       "\nmodule ",        // 05
       {kTag, "bar"},      // 06
       "(input ",          // 07
       {kTag, "a"},        // 08
       ");\n",             // 09
       {kTag, "foo"},      // 10
       " ",                // 11
       {kTag, "f1"},       // 12
       "(.",               // 13
       {kTag, "*"},        // 14
       ");\nendmodule"}};

  SimpleTestProject project(kTestCase.code);
  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(
          T(project.XUnit().ExpectedFileData(),
            // refers to module foo.
            T(
                D{
                    IndexingFactType::kModule,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to  a.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[3]),
                })),
            // refers to module bar.
            T(
                D{
                    IndexingFactType::kModule,
                    Anchor(kTestCase.expected_tokens[6]),
                },
                // refers to input a.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[8]),
                }),
                // refers to foo.
                T(
                    D{
                        IndexingFactType::kDataTypeReference,
                        Anchor(kTestCase.expected_tokens[10]),
                    },
                    // refers to f1(.*)
                    T(D{
                        IndexingFactType::kModuleInstance,
                        Anchor(kTestCase.expected_tokens[12]),
                    }  // The .* is not recorded as named module port.
                      ))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to module foo.
            T(
                D{
                    IndexingFactType::kModule,
                    Anchor(kTestCase.expected_tokens[1]),
                    Anchor(kTestCase.expected_tokens[11]),
                },
                // refers to  a.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[3]),
                }),
                // refers to  b.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[5]),
                }),
                // refers to input z.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[7]),
                }),
                // refers to h.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[9]),
                })))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(
          T(
              project.XUnit().ExpectedFileData(),
              // refers to package pkg.
              T(
                  D{
                      IndexingFactType::kPackage,
                      Anchor(kTestCase.expected_tokens[1]),
                  },
                  // refers to x;
                  T(D{
                      IndexingFactType::kVariableDefinition,
                      Anchor(kTestCase.expected_tokens[3]),
                  }),
                  // refers to y;
                  T(D{
                      IndexingFactType::kVariableDefinition,
                      Anchor(kTestCase.expected_tokens[5]),
                  }),
                  // refers to l1;
                  T(D{
                      IndexingFactType::kVariableDefinition,
                      Anchor(kTestCase.expected_tokens[7]),
                  }),
                  // refers to l2;
                  T(D{
                      IndexingFactType::kVariableDefinition,
                      Anchor(kTestCase.expected_tokens[9]),
                  }),
                  // refers to b1;
                  T(D{
                      IndexingFactType::kVariableDefinition,
                      Anchor(kTestCase.expected_tokens[11]),
                  }),
                  // refers to b2;
                  T(D{
                      IndexingFactType::kVariableDefinition,
                      Anchor(kTestCase.expected_tokens[13]),
                  }),
                  // refers to s1;
                  T(D{
                      IndexingFactType::kVariableDefinition,
                      Anchor(kTestCase.expected_tokens[15]),
                  }),
                  // refers to s2;
                  T(D{
                      IndexingFactType::kVariableDefinition,
                      Anchor(kTestCase.expected_tokens[17]),
                  })),
              // refers to class cla.
              T(
                  D{
                      IndexingFactType::kClass,
                      Anchor(kTestCase.expected_tokens[19]),
                  },
                  // refers to x;
                  T(D{
                      IndexingFactType::kVariableDefinition,
                      Anchor(kTestCase.expected_tokens[21]),
                  }),
                  // refers to y;
                  T(D{
                      IndexingFactType::kVariableDefinition,
                      Anchor(kTestCase.expected_tokens[23]),
                  }),
                  // refers to l1;
                  T(D{
                      IndexingFactType::kVariableDefinition,
                      Anchor(kTestCase.expected_tokens[25]),
                  }),
                  // refers to l2;
                  T(D{
                      IndexingFactType::kVariableDefinition,
                      Anchor(kTestCase.expected_tokens[27]),
                  }),
                  // refers to b1;
                  T(D{
                      IndexingFactType::kVariableDefinition,
                      Anchor(kTestCase.expected_tokens[29]),
                  }),
                  // refers to b2;
                  T(D{
                      IndexingFactType::kVariableDefinition,
                      Anchor(kTestCase.expected_tokens[31]),
                  }),
                  // refers to s1;
                  T(D{
                      IndexingFactType::kVariableDefinition,
                      Anchor(kTestCase.expected_tokens[33]),
                  }),
                  // refers to s2;
                  T(D{
                      IndexingFactType::kVariableDefinition,
                      Anchor(kTestCase.expected_tokens[35]),
                  })),
              // refers to function fun.
              T(
                  D{
                      IndexingFactType::kFunctionOrTask,
                      Anchor(kTestCase.expected_tokens[37]),
                  },
                  // refers to x;
                  T(D{
                      IndexingFactType::kVariableDefinition,
                      Anchor(kTestCase.expected_tokens[39]),
                  }),
                  // refers to y;
                  T(
                      D{
                          IndexingFactType::kVariableDefinition,
                          Anchor(kTestCase.expected_tokens[41]),
                      },
                      // refers to my_fun;
                      T(
                          D{
                              IndexingFactType::kFunctionCall,
                              Anchor(kTestCase.expected_tokens[43]),
                          },
                          // refers to o;
                          T(D{
                              IndexingFactType::kVariableReference,
                              Anchor(kTestCase.expected_tokens[45]),
                          }),
                          // refers to l;
                          T(D{
                              IndexingFactType::kVariableReference,
                              Anchor(kTestCase.expected_tokens[47]),
                          }))),
                  // refers to l1;
                  T(D{
                      IndexingFactType::kVariableDefinition,
                      Anchor(kTestCase.expected_tokens[49]),
                  }),
                  // refers to l2;
                  T(D{
                      IndexingFactType::kVariableDefinition,
                      Anchor(kTestCase.expected_tokens[51]),
                  }),
                  // refers to b1;
                  T(D{
                      IndexingFactType::kVariableDefinition,
                      Anchor(kTestCase.expected_tokens[53]),
                  }),
                  // refers to b2;
                  T(D{
                      IndexingFactType::kVariableDefinition,
                      Anchor(kTestCase.expected_tokens[55]),
                  }),
                  // refers to s1;
                  T(D{
                      IndexingFactType::kVariableDefinition,
                      Anchor(kTestCase.expected_tokens[57]),
                  }),
                  // refers to s2;
                  T(D{
                      IndexingFactType::kVariableDefinition,
                      Anchor(kTestCase.expected_tokens[59]),
                  }),
                  // refers to return x;
                  T(D{
                      IndexingFactType::kVariableReference,
                      Anchor(kTestCase.expected_tokens[61]),
                  })))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to module foo.
            T(
                D{
                    IndexingFactType::kModule,
                    Anchor(kTestCase.expected_tokens[1]),
                    Anchor(kTestCase.expected_tokens[13]),
                },
                // refers to input in.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[4]),
                }),
                // refers to output x.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[7]),
                }),
                // refers to output y
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[9]),
                }),
                // refers to output z.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[11]),
                })))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to module bar.
            T(
                D{
                    IndexingFactType::kModule,
                    Anchor(kTestCase.expected_tokens[1]),
                    Anchor(kTestCase.expected_tokens[7]),
                },
                // refers to input x.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[3]),
                }),
                // refers to output y.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[5]),
                })),
            // refers to module foo.
            T(
                D{
                    IndexingFactType::kModule,
                    Anchor(kTestCase.expected_tokens[9]),
                    Anchor(kTestCase.expected_tokens[23]),
                },
                // refers to input x.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[11]),
                }),
                // refers to output y.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[13]),
                }),
                // refers to bar.
                T(
                    D{
                        IndexingFactType::kDataTypeReference,
                        Anchor(kTestCase.expected_tokens[15]),
                    },
                    // refers to b1(x, y).
                    T(
                        D{
                            IndexingFactType::kModuleInstance,
                            Anchor(kTestCase.expected_tokens[17]),
                        },
                        // refers to x
                        T(D{
                            IndexingFactType::kVariableReference,
                            Anchor(kTestCase.expected_tokens[19]),
                        }),
                        // refers to y
                        T(D{
                            IndexingFactType::kVariableReference,
                            Anchor(kTestCase.expected_tokens[21]),
                        })))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
}  // namespace

TEST(FactsTreeExtractor, WireTest) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {{"module ",
                                                        {kTag, "foo"},
                                                        "();\nwire ",
                                                        {kTag, "a"},
                                                        ";\nendmodule: ",
                                                        {kTag, "foo"}}};

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to module foo
            T(
                D{
                    IndexingFactType::kModule,
                    Anchor(kTestCase.expected_tokens[1]),
                    Anchor(kTestCase.expected_tokens[5]),
                },
                // refers to "wire a"
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[3]),
                })))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
}

TEST(FactsTreeExtractor, ClassTest) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"class ", {kTag, "foo"}, ";\nendclass: ", {kTag, "foo"}}};

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to class foo
            T(D{
                IndexingFactType::kClass,
                Anchor(kTestCase.expected_tokens[1]),
                Anchor(kTestCase.expected_tokens[3]),
            }))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to module foo
            T(
                D{
                    IndexingFactType::kModule,
                    Anchor(kTestCase.expected_tokens[1]),
                    Anchor(kTestCase.expected_tokens[7]),
                },
                // refers to "class foo"
                T(D{
                    IndexingFactType::kClass,
                    Anchor(kTestCase.expected_tokens[3]),
                    Anchor(kTestCase.expected_tokens[5]),
                })))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to class foo
            T(
                D{
                    IndexingFactType::kClass,
                    Anchor(kTestCase.expected_tokens[1]),
                    Anchor(kTestCase.expected_tokens[7]),
                },
                // refers to class bar
                T(D{
                    IndexingFactType::kClass,
                    Anchor(kTestCase.expected_tokens[3]),
                    Anchor(kTestCase.expected_tokens[5]),
                })))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to class bar.
            T(D{
                IndexingFactType::kClass,
                Anchor(kTestCase.expected_tokens[1]),
                Anchor(kTestCase.expected_tokens[3]),
            }),
            // refers to module foo.
            T(
                D{
                    IndexingFactType::kModule,
                    Anchor(kTestCase.expected_tokens[5]),
                    Anchor(kTestCase.expected_tokens[17]),
                },
                // refers to bar.
                T(
                    D{
                        IndexingFactType::kDataTypeReference,
                        Anchor(kTestCase.expected_tokens[7]),
                    },
                    // refers to b1.
                    T(D{
                        IndexingFactType::kClassInstance,
                        Anchor(kTestCase.expected_tokens[9]),
                    }),
                    // refers to b2.
                    T(
                        D{
                            IndexingFactType::kClassInstance,
                            Anchor(kTestCase.expected_tokens[11]),
                        },
                        // refers to x.
                        T(D{
                            IndexingFactType::kVariableReference,
                            Anchor(kTestCase.expected_tokens[13]),
                        }),
                        // refers to y.
                        T(D{
                            IndexingFactType::kVariableReference,
                            Anchor(kTestCase.expected_tokens[15]),
                        })))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to class inner.
            T(
                D{
                    IndexingFactType::kClass,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to int x.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[3]),
                })),
            // refers to class bar.
            T(
                D{
                    IndexingFactType::kClass,
                    Anchor(kTestCase.expected_tokens[6]),
                    Anchor(kTestCase.expected_tokens[12]),
                },
                // refers to inner in1.
                T(
                    D{
                        IndexingFactType::kDataTypeReference,
                        Anchor(kTestCase.expected_tokens[8]),
                    },
                    // refers to in1.
                    T(D{
                        IndexingFactType::kClassInstance,
                        Anchor(kTestCase.expected_tokens[10]),
                    }))),
            // refers to module foo.
            T(
                D{
                    IndexingFactType::kModule,
                    Anchor(kTestCase.expected_tokens[14]),
                    Anchor(kTestCase.expected_tokens[26]),
                },
                // refers to bar.
                T(
                    D{
                        IndexingFactType::kDataTypeReference,
                        Anchor(kTestCase.expected_tokens[16]),
                    },
                    // refers to b1.
                    T(D{
                        IndexingFactType::kClassInstance,
                        Anchor(kTestCase.expected_tokens[18]),
                    })),
                // anonymous scope for initial.
                T(
                    D{
                        IndexingFactType::kAnonymousScope,
                        Anchor(absl::make_unique<std::string>(
                            "anonymous-scope-0")),
                    },
                    // refers to bar::in::x.
                    T(D{
                        IndexingFactType::kMemberReference,
                        Anchor(kTestCase.expected_tokens[20]),
                        Anchor(kTestCase.expected_tokens[22]),
                        Anchor(kTestCase.expected_tokens[24]),
                    }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to function foo
            T(D{
                IndexingFactType::kFunctionOrTask,
                Anchor(kTestCase.expected_tokens[1]),
            }),
            // refers to task bar
            T(D{
                IndexingFactType::kFunctionOrTask,
                Anchor(kTestCase.expected_tokens[5]),
            }))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to function foo
            T(
                D{
                    IndexingFactType::kFunctionOrTask,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[3]),
                }),
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[5]),
                }),
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[7]),
                }),
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[9]),
                })),
            // refers to task bar
            T(
                D{
                    IndexingFactType::kFunctionOrTask,
                    Anchor(kTestCase.expected_tokens[13]),
                },
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[15]),
                }),
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[17]),
                }),
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[19]),
                }),
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[21]),
                })))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to module m
            T(
                D{
                    IndexingFactType::kModule,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // anonymous scope for initial.
                T(
                    D{
                        IndexingFactType::kAnonymousScope,
                        Anchor(absl::make_unique<std::string>(
                            "anonymous-scope-0")),
                    },
                    // refers to my_class.x
                    T(D{
                        IndexingFactType::kMemberReference,
                        Anchor(kTestCase.expected_tokens[3]),
                        Anchor(kTestCase.expected_tokens[5]),
                    }),
                    // refers to my_class.instance1.x
                    T(D{
                        IndexingFactType::kMemberReference,
                        Anchor(kTestCase.expected_tokens[7]),
                        Anchor(kTestCase.expected_tokens[9]),
                        Anchor(kTestCase.expected_tokens[11]),
                    }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to function foo
            T(D{
                IndexingFactType::kFunctionOrTask,
                Anchor(kTestCase.expected_tokens[1]),
            }),
            // refers to task bar
            T(D{
                IndexingFactType::kFunctionOrTask,
                Anchor(kTestCase.expected_tokens[5]),
            }),
            T(
                D{
                    IndexingFactType::kModule,
                    Anchor(kTestCase.expected_tokens[9]),
                    Anchor(kTestCase.expected_tokens[15]),
                },
                // anonymous scope for initial.
                T(
                    D{
                        IndexingFactType::kAnonymousScope,
                        Anchor(absl::make_unique<std::string>(
                            "anonymous-scope-0")),
                    },
                    T(D{
                        IndexingFactType::kFunctionCall,
                        Anchor(kTestCase.expected_tokens[11]),
                    }),
                    T(D{
                        IndexingFactType::kFunctionCall,
                        Anchor(kTestCase.expected_tokens[13]),
                    }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(
          T(
              project.XUnit().ExpectedFileData(),
              // refers to class inner.
              T(
                  D{
                      IndexingFactType::kClass,
                      Anchor(kTestCase.expected_tokens[1]),
                  },
                  // refers to function my_fun.
                  T(D{
                      IndexingFactType::kFunctionOrTask,
                      Anchor(kTestCase.expected_tokens[3]),
                  }),
                  // refers to function fun_2.
                  T(
                      D{
                          IndexingFactType::kFunctionOrTask,
                          Anchor(kTestCase.expected_tokens[5]),
                      },
                      // refers to x arg in fun_2.
                      T(D{
                          IndexingFactType::kVariableDefinition,
                          Anchor(kTestCase.expected_tokens[7]),
                      }),
                      // refers to y arg in fun_2.
                      T(D{
                          IndexingFactType::kVariableDefinition,
                          Anchor(kTestCase.expected_tokens[9]),
                      }),
                      // refers to x.
                      T(D{
                          IndexingFactType::kVariableReference,
                          Anchor(kTestCase.expected_tokens[11]),
                      }),
                      // refers to x.
                      T(D{
                          IndexingFactType::kVariableReference,
                          Anchor(kTestCase.expected_tokens[13]),
                      }))),
              // refers to class bar.
              T(
                  D{
                      IndexingFactType::kClass,
                      Anchor(kTestCase.expected_tokens[16]),
                      Anchor(kTestCase.expected_tokens[22]),
                  },
                  // refers to inner in1.
                  T(
                      D{
                          IndexingFactType::kDataTypeReference,
                          Anchor(kTestCase.expected_tokens[18]),
                      },
                      // refers to in1.
                      T(D{
                          IndexingFactType::kClassInstance,
                          Anchor(kTestCase.expected_tokens[20]),
                      }))),
              // refers to module foo.
              T(
                  D{
                      IndexingFactType::kModule,
                      Anchor(kTestCase.expected_tokens[24]),
                  },
                  // refers to bar.
                  T(
                      D{
                          IndexingFactType::kDataTypeReference,
                          Anchor(kTestCase.expected_tokens[26]),
                      },
                      // refers to b1.
                      T(D{
                          IndexingFactType::kClassInstance,
                          Anchor(kTestCase.expected_tokens[28]),
                      })),
                  // anonymous scope for initial.
                  T(
                      D{
                          IndexingFactType::kAnonymousScope,
                          Anchor(absl::make_unique<std::string>(
                              "anonymous-scope-0")),
                      },
                      // refers to bar::in::my_fun().
                      T(D{
                          IndexingFactType::kFunctionCall,
                          Anchor(kTestCase.expected_tokens[30]),
                          Anchor(kTestCase.expected_tokens[32]),
                          Anchor(kTestCase.expected_tokens[34]),
                      })),
                  // anonymous scope for initial.
                  T(
                      D{
                          IndexingFactType::kAnonymousScope,
                          Anchor(absl::make_unique<std::string>(
                              "anonymous-scope-1")),
                      },
                      // refers to bar::in.my_fun().
                      T(D{
                          IndexingFactType::kFunctionCall,
                          Anchor(kTestCase.expected_tokens[36]),
                          Anchor(kTestCase.expected_tokens[38]),
                          Anchor(kTestCase.expected_tokens[40]),
                      })),
                  // refers to inner in1.
                  T(
                      D{
                          IndexingFactType::kDataTypeReference,
                          Anchor(kTestCase.expected_tokens[42]),
                      },
                      // refers to in1.
                      T(D{
                          IndexingFactType::kClassInstance,
                          Anchor(kTestCase.expected_tokens[44]),
                      })),
                  // refers to int x.
                  T(D{
                      IndexingFactType::kVariableDefinition,
                      Anchor(kTestCase.expected_tokens[46]),
                  }),
                  // refers to int y.
                  T(D{
                      IndexingFactType::kVariableDefinition,
                      Anchor(kTestCase.expected_tokens[48]),
                  }),
                  // anonymous scope for initial.
                  T(
                      D{
                          IndexingFactType::kAnonymousScope,
                          Anchor(absl::make_unique<std::string>(
                              "anonymous-scope-2")),
                      },
                      // refers to in1.my_fun().
                      T(
                          D{
                              IndexingFactType::kFunctionCall,
                              Anchor(kTestCase.expected_tokens[50]),
                              Anchor(kTestCase.expected_tokens[52]),
                          },
                          // refers to x.
                          T(D{
                              IndexingFactType::kVariableReference,
                              Anchor(kTestCase.expected_tokens[54]),
                          }),
                          // refers to y.
                          T(D{
                              IndexingFactType::kVariableReference,
                              Anchor(kTestCase.expected_tokens[56]),
                          })))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
}

TEST(FactsTreeExtractor, ThisAsFunctionCall) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase{
      // Note: this test case is also significant in that it exercises
      // short-string-optimization, where by sufficiently short strings live
      // directly in the object, rather than in heap memory.
      {
          {kTag, "r"},
          "=this();",
      },
  };

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to r.
            T(D{
                IndexingFactType::kVariableDefinition,
                Anchor(kTestCase.expected_tokens[0]),
            }))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to macro PRINT_STRING.
            T(
                D{
                    IndexingFactType::kMacro,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to str1 arg in PRINT_STRING.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[3]),
                })),
            // refers to macro PRINT_3_STRING.
            T(
                D{
                    IndexingFactType::kMacro,
                    Anchor(kTestCase.expected_tokens[6]),
                },
                // refers to str1 arg in PRINT_3_STRING.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[8]),
                }),
                // refers to str2 arg in PRINT_3_STRING.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[10]),
                }),
                // refers to str3 arg in PRINT_3_STRING.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[12]),
                })),
            // refers to macro TEN.
            T(D{
                IndexingFactType::kMacro,
                Anchor(kTestCase.expected_tokens[16]),
            }))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(T(
          project.XUnit().ExpectedFileData(),
          // refers to macro PRINT_STRING.
          T(
              D{
                  IndexingFactType::kMacro,
                  Anchor(kTestCase.expected_tokens[1]),
              },
              // refers to str1 in PRINT_STRING.
              T(D{
                  IndexingFactType::kVariableDefinition,
                  Anchor(kTestCase.expected_tokens[3]),
              })),
          // refers to macro PRINT_3_STRING.
          T(
              D{
                  IndexingFactType::kMacro,
                  Anchor(kTestCase.expected_tokens[6]),
              },
              // refers to str1 in PRINT_3_STRING.
              T(D{
                  IndexingFactType::kVariableDefinition,
                  Anchor(kTestCase.expected_tokens[8]),
              }),
              // refers to str2 in PRINT_3_STRING.
              T(D{
                  IndexingFactType::kVariableDefinition,
                  Anchor(kTestCase.expected_tokens[10]),
              }),
              // refers to str3 in PRINT_3_STRING.
              T(D{
                  IndexingFactType::kVariableDefinition,
                  Anchor(kTestCase.expected_tokens[12]),
              })),
          // refers to macro TEN.
          T(D{
              IndexingFactType::kMacro,
              Anchor(kTestCase.expected_tokens[16]),
          }),
          // refers to macro NUM.
          T(
              D{
                  IndexingFactType::kMacro,
                  Anchor(kTestCase.expected_tokens[19]),
              },
              // refers to i in macro NUM.
              T(D{
                  IndexingFactType::kVariableDefinition,
                  Anchor(kTestCase.expected_tokens[21]),
              })),
          // refers to module macro.
          T(
              D{
                  IndexingFactType::kModule,
                  Anchor(kTestCase.expected_tokens[24]),
              },
              // anonymous scope for initial.
              T(
                  D{
                      IndexingFactType::kAnonymousScope,
                      Anchor(
                          absl::make_unique<std::string>("anonymous-scope-0")),
                  },
                  // refers to macro call PRINT_3_STRINGS.
                  T(D{
                      IndexingFactType::kMacroCall,
                      Anchor(kTestCase.expected_tokens[26]),
                  }),
                  // refers to macro call TEN.
                  T(D{
                      IndexingFactType::kMacroCall,
                      Anchor(kTestCase.expected_tokens[29]),
                  }),
                  // refers to macro call NUM.
                  T(
                      D{
                          IndexingFactType::kMacroCall,
                          Anchor(kTestCase.expected_tokens[32]),
                      },  // refers to macro call TEN.
                      T(D{
                          IndexingFactType::kMacroCall,
                          Anchor(kTestCase.expected_tokens[34]),
                      })),
                  // refers to parm x
                  T(
                      D{
                          IndexingFactType::kParamDeclaration,
                          Anchor(kTestCase.expected_tokens[37]),
                      },
                      // refers to macro call TEN.
                      T(D{
                          IndexingFactType::kMacroCall,
                          Anchor(kTestCase.expected_tokens[39]),
                      })))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to package pkg1.
            T(D{
                IndexingFactType::kPackage,
                Anchor(kTestCase.expected_tokens[1]),
            }),
            // refers to package pkg.
            T(
                D{
                    IndexingFactType::kPackage,
                    Anchor(kTestCase.expected_tokens[3]),
                },
                // refers to class my_class.
                T(D{
                    IndexingFactType::kClass,
                    Anchor(kTestCase.expected_tokens[5]),
                }),
                // refers to function my_function.
                T(D{
                    IndexingFactType::kFunctionOrTask,
                    Anchor(kTestCase.expected_tokens[7]),
                })),
            // refers to module m..
            T(
                D{
                    IndexingFactType::kModule,
                    Anchor(kTestCase.expected_tokens[9]),
                },
                // refers to import pkg1::*.
                T(D{
                    IndexingFactType::kPackageImport,
                    Anchor(kTestCase.expected_tokens[11]),
                }),
                // refers to import pkg::my_function.
                T(D{
                    IndexingFactType::kPackageImport,
                    Anchor(kTestCase.expected_tokens[13]),
                    Anchor(kTestCase.expected_tokens[15]),
                }),
                // refers to import pkg::my_class.
                T(D{
                    IndexingFactType::kPackageImport,
                    Anchor(kTestCase.expected_tokens[17]),
                    Anchor(kTestCase.expected_tokens[19]),
                })))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to package pkg.
            T(
                D{
                    IndexingFactType::kPackage,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to class my_class.
                T(D{
                    IndexingFactType::kClass,
                    Anchor(kTestCase.expected_tokens[3]),
                }),
                // refers to wire x.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[5]),
                })),
            // refers to module m..
            T(
                D{
                    IndexingFactType::kModule,
                    Anchor(kTestCase.expected_tokens[7]),
                },
                // anonymous scope for initial.
                T(
                    D{
                        IndexingFactType::kAnonymousScope,
                        Anchor(absl::make_unique<std::string>(
                            "anonymous-scope-0")),
                    },
                    // refers to $display(pkg::x).
                    T(D{
                        IndexingFactType::kMemberReference,
                        Anchor(kTestCase.expected_tokens[10]),
                        Anchor(kTestCase.expected_tokens[12]),
                    }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to function m.
            T(
                D{
                    IndexingFactType::kFunctionOrTask,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers foo(.A, .B).
                T(
                    D{
                        IndexingFactType::kMemberReference,
                        Anchor(kTestCase.expected_tokens[3]),
                    },
                    // refers A.
                    T(
                        D{
                            IndexingFactType::kNamedParam,
                            Anchor(kTestCase.expected_tokens[5]),
                        },
                        // refers var1.
                        T(D{
                            IndexingFactType::kVariableReference,
                            Anchor(kTestCase.expected_tokens[7]),
                        })),
                    // refers B.
                    T(
                        D{
                            IndexingFactType::kNamedParam,
                            Anchor(kTestCase.expected_tokens[9]),
                        },
                        // refers var2.
                        T(D{
                            IndexingFactType::kVariableReference,
                            Anchor(kTestCase.expected_tokens[11]),
                        }))),
                // refers barc(.X, .W).
                T(
                    D{
                        IndexingFactType::kMemberReference,
                        Anchor(kTestCase.expected_tokens[3]),
                        Anchor(kTestCase.expected_tokens[13]),
                    },
                    // refers X.
                    T(
                        D{
                            IndexingFactType::kNamedParam,
                            Anchor(kTestCase.expected_tokens[15]),
                        },
                        // refers var1.
                        T(D{
                            IndexingFactType::kVariableReference,
                            Anchor(kTestCase.expected_tokens[17]),
                        })),
                    // refers W.
                    T(
                        D{
                            IndexingFactType::kNamedParam,
                            Anchor(kTestCase.expected_tokens[19]),
                        },
                        // refers var2.
                        T(D{
                            IndexingFactType::kVariableReference,
                            Anchor(kTestCase.expected_tokens[21]),
                        }))),
                // refers var2.
                T(D{
                    IndexingFactType::kFunctionCall,
                    Anchor(kTestCase.expected_tokens[3]),
                    Anchor(kTestCase.expected_tokens[13]),
                    Anchor(kTestCase.expected_tokens[23]),
                })))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to class stack.
            T(
                D{
                    IndexingFactType::kClass,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to some_type.
                T(D{
                    IndexingFactType::kTypeDeclaration,
                    Anchor(kTestCase.expected_tokens[3]),
                })))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to task t.
            T(
                D{
                    IndexingFactType::kFunctionOrTask,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to y.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[3]),
                }),
                // refers to x.
                T(D{
                    IndexingFactType::kVariableReference,
                    Anchor(kTestCase.expected_tokens[5]),
                }),
                // refers to y.
                T(D{
                    IndexingFactType::kVariableReference,
                    Anchor(kTestCase.expected_tokens[7]),
                })))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to class stack.
            T(
                D{
                    IndexingFactType::kClass,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to T.
                T(D{
                    IndexingFactType::kParamDeclaration,
                    Anchor(kTestCase.expected_tokens[3]),
                })))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to module m.
            T(
                D{
                    IndexingFactType::kModule,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to clt#(x) v1.
                T(
                    D{
                        IndexingFactType::kDataTypeReference,
                        Anchor(kTestCase.expected_tokens[3]),
                    },
                    // refers to x.
                    T(D{
                        IndexingFactType::kVariableReference,
                        Anchor(kTestCase.expected_tokens[5]),
                    }),
                    // refers to v1.
                    T(D{
                        IndexingFactType::kClassInstance,
                        Anchor(kTestCase.expected_tokens[7]),
                    }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to package pkg.
            T(D{
                IndexingFactType::kPackage,
                Anchor(kTestCase.expected_tokens[1]),
                Anchor(kTestCase.expected_tokens[3]),
            }))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to function foo
            T(
                D{
                    IndexingFactType::kFunctionOrTask,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // anonymous scope for initial.
                T(
                    D{
                        IndexingFactType::kAnonymousScope,
                        Anchor(absl::make_unique<std::string>(
                            "anonymous-scope-0")),
                    },
                    // refers to i
                    T(D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[3]),
                    }),
                    // refers to j
                    T(D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[5]),
                    }),
                    // refers to tm
                    T(D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[11]),
                    }),
                    // refers to l
                    T(D{
                        IndexingFactType::kVariableReference,
                        Anchor(kTestCase.expected_tokens[7]),
                    }),
                    // refers to r
                    T(D{
                        IndexingFactType::kVariableReference,
                        Anchor(kTestCase.expected_tokens[9]),
                    }),
                    // refers to i
                    T(D{
                        IndexingFactType::kVariableReference,
                        Anchor(kTestCase.expected_tokens[13]),
                    }),
                    // refers to i
                    T(D{
                        IndexingFactType::kVariableReference,
                        Anchor(kTestCase.expected_tokens[15]),
                    }),
                    // refers to x
                    T(D{
                        IndexingFactType::kVariableReference,
                        Anchor(kTestCase.expected_tokens[17]),
                    }),
                    // refers to i
                    T(D{
                        IndexingFactType::kVariableReference,
                        Anchor(kTestCase.expected_tokens[19]),
                    })),
                // refers to x
                T(D{
                    IndexingFactType::kVariableReference,
                    Anchor(kTestCase.expected_tokens[21]),
                })))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to class X.
            T(
                D{
                    IndexingFactType::kClass,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to Y.
                T(D{
                    IndexingFactType::kExtends,
                    Anchor(kTestCase.expected_tokens[3]),
                })),
            // refers to H.
            T(
                D{
                    IndexingFactType::kClass,
                    Anchor(kTestCase.expected_tokens[5]),
                },
                // refers to G::K.
                T(D{
                    IndexingFactType::kExtends,
                    Anchor(kTestCase.expected_tokens[7]),
                    Anchor(kTestCase.expected_tokens[9]),
                })))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to module m.
            T(
                D{
                    IndexingFactType::kModule,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to module parameter x.
                T(D{
                    IndexingFactType::kParamDeclaration,
                    Anchor(kTestCase.expected_tokens[3]),
                }),
                // refers to class parameter y.
                T(D{
                    IndexingFactType::kParamDeclaration,
                    Anchor(kTestCase.expected_tokens[5]),
                }),
                // refers to class input x.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[7]),
                }),
                // refers to bar.
                T(
                    D{
                        IndexingFactType::kDataTypeReference,
                        Anchor(kTestCase.expected_tokens[9]),
                    },
                    // refers to .p1(x).
                    T(
                        D{
                            IndexingFactType::kNamedParam,
                            Anchor(kTestCase.expected_tokens[11]),
                        },
                        // refers to x.
                        T(D{
                            IndexingFactType::kVariableReference,
                            Anchor(kTestCase.expected_tokens[13]),
                        })),
                    // refers to .p2(y).
                    T(
                        D{
                            IndexingFactType::kNamedParam,
                            Anchor(kTestCase.expected_tokens[15]),
                        },
                        // refers to y.
                        T(D{
                            IndexingFactType::kVariableReference,
                            Anchor(kTestCase.expected_tokens[17]),
                        })),
                    // refers to b1.
                    T(
                        D{
                            IndexingFactType::kModuleInstance,
                            Anchor(kTestCase.expected_tokens[19]),
                        },
                        // refers to z.
                        T(D{
                            IndexingFactType::kVariableReference,
                            Anchor(kTestCase.expected_tokens[21]),
                        })))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to interface m.
            T(
                D{
                    IndexingFactType::kInterface,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to module parameter x.
                T(D{
                    IndexingFactType::kParamDeclaration,
                    Anchor(kTestCase.expected_tokens[3]),
                }),
                // refers to class parameter y.
                T(D{
                    IndexingFactType::kParamDeclaration,
                    Anchor(kTestCase.expected_tokens[5]),
                }),
                // refers to class input x.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[7]),
                }),
                // refers to bar.
                T(
                    D{
                        IndexingFactType::kDataTypeReference,
                        Anchor(kTestCase.expected_tokens[9]),
                    },
                    // refers to .p1(x).
                    T(
                        D{
                            IndexingFactType::kNamedParam,
                            Anchor(kTestCase.expected_tokens[11]),
                        },
                        // refers to x.
                        T(D{
                            IndexingFactType::kVariableReference,
                            Anchor(kTestCase.expected_tokens[13]),
                        })),
                    // refers to .p2(y).
                    T(
                        D{
                            IndexingFactType::kNamedParam,
                            Anchor(kTestCase.expected_tokens[15]),
                        },
                        // refers to y.
                        T(D{
                            IndexingFactType::kVariableReference,
                            Anchor(kTestCase.expected_tokens[17]),
                        })),
                    // refers to b1.
                    T(
                        D{
                            IndexingFactType::kModuleInstance,
                            Anchor(kTestCase.expected_tokens[19]),
                        },
                        // refers to z.
                        T(D{
                            IndexingFactType::kVariableReference,
                            Anchor(kTestCase.expected_tokens[21]),
                        })))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to module m.
            T(
                D{
                    IndexingFactType::kModule,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to class_type.
                T(
                    D{
                        IndexingFactType::kDataTypeReference,
                        Anchor(kTestCase.expected_tokens[3]),
                    },
                    // refers to x.
                    T(D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[5]),
                    }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to program m.
            T(
                D{
                    IndexingFactType::kProgram,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to module parameter x.
                T(D{
                    IndexingFactType::kParamDeclaration,
                    Anchor(kTestCase.expected_tokens[3]),
                }),
                // refers to class parameter y.
                T(D{
                    IndexingFactType::kParamDeclaration,
                    Anchor(kTestCase.expected_tokens[5]),
                }),
                // refers to class input x.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[7]),
                }),
                // refers to bar.
                T(
                    D{
                        IndexingFactType::kDataTypeReference,
                        Anchor(kTestCase.expected_tokens[9]),
                    },
                    // refers to .p1(x).
                    T(
                        D{
                            IndexingFactType::kNamedParam,
                            Anchor(kTestCase.expected_tokens[11]),
                        },
                        // refers to x.
                        T(D{
                            IndexingFactType::kVariableReference,
                            Anchor(kTestCase.expected_tokens[13]),
                        })),
                    // refers to .p2(y).
                    T(
                        D{
                            IndexingFactType::kNamedParam,
                            Anchor(kTestCase.expected_tokens[15]),
                        },
                        // refers to y.
                        T(D{
                            IndexingFactType::kVariableReference,
                            Anchor(kTestCase.expected_tokens[17]),
                        })),
                    // refers to b1.
                    T(
                        D{
                            IndexingFactType::kModuleInstance,
                            Anchor(kTestCase.expected_tokens[19]),
                        },
                        // refers to z.
                        T(D{
                            IndexingFactType::kVariableReference,
                            Anchor(kTestCase.expected_tokens[21]),
                        })))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to package pkg.
            T(
                D{
                    IndexingFactType::kPackage,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to k.
                T(D{
                    IndexingFactType::kVariableReference,
                    Anchor(kTestCase.expected_tokens[3]),
                }),
                // refers to y.
                T(D{
                    IndexingFactType::kVariableReference,
                    Anchor(kTestCase.expected_tokens[5]),
                }),
                // refers to x.
                T(
                    D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[7]),
                    },
                    // refers to l.
                    T(D{
                        IndexingFactType::kVariableReference,
                        Anchor(kTestCase.expected_tokens[9]),
                    }),
                    // refers to r.
                    T(D{
                        IndexingFactType::kVariableReference,
                        Anchor(kTestCase.expected_tokens[11]),
                    }))),
            // refers to module m.
            T(
                D{
                    IndexingFactType::kModule,
                    Anchor(kTestCase.expected_tokens[13]),
                },
                // refers to j.
                T(D{
                    IndexingFactType::kVariableReference,
                    Anchor(kTestCase.expected_tokens[15]),
                }),
                // refers to o.
                T(D{
                    IndexingFactType::kVariableReference,
                    Anchor(kTestCase.expected_tokens[17]),
                }),
                // refers to v.
                T(
                    D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[19]),
                    },
                    // refers to e.
                    T(D{
                        IndexingFactType::kVariableReference,
                        Anchor(kTestCase.expected_tokens[21]),
                    }),
                    // refers to t.
                    T(D{
                        IndexingFactType::kVariableReference,
                        Anchor(kTestCase.expected_tokens[23]),
                    }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  const std::string included_file_basename(
      verible::file::testing::RandomFileBasename("include-this"));
  const std::string quoted_included_file =
      absl::StrCat("\"", included_file_basename, "\"");

  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"`include ",
       {kTag, quoted_included_file},
       "\n module ",
       {kTag, "my_module"},
       "();\n initial begin\n$display(",
       {kTag, "my_class"},
       "::",
       {kTag, "var5"},
       ");\n "
       "end\nendmodule"},
  };

  IncludeTestProject project(kTestCase.code, included_file_basename,
                             kTestCase0.code, {::testing::TempDir()});

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.IncludeFile().RebaseFileTree(
          T(project.IncludeFile().ExpectedFileData(),
            // refers to class my_class.
            T(
                D{
                    IndexingFactType::kClass,
                    Anchor(kTestCase0.expected_tokens[1]),
                },
                // refers to int var5.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase0.expected_tokens[3]),
                })))),
      project.XUnit().RebaseFileTree(T(
          project.XUnit().ExpectedFileData(),
          // refers to include.
          T(D{
              IndexingFactType::kInclude,
              Anchor(kTestCase.expected_tokens[1]),
              Anchor(absl::make_unique<std::string>(
                  project.IncludeFile().source_file->ResolvedPath())),
          }),
          // refers to module my_module.
          T(
              D{
                  IndexingFactType::kModule,
                  Anchor(kTestCase.expected_tokens[3]),
              },
              // anonymous scope for initial.
              T(
                  D{
                      IndexingFactType::kAnonymousScope,
                      Anchor(
                          absl::make_unique<std::string>("anonymous-scope-0")),
                  },
                  // refers to $display(my_class::var5).
                  T(D{
                      IndexingFactType::kMemberReference,
                      Anchor(kTestCase.expected_tokens[5]),
                      Anchor(kTestCase.expected_tokens[7]),
                  }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  // Print showing raw addresses instead of in-file byte-offsets.
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, IncludedFileContainsParseError) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase0{
      {"parameter 7;"},  // parse error
  };

  const std::string included_file_basename(
      verible::file::testing::RandomFileBasename("include-this"));
  const std::string quoted_included_file =
      absl::StrCat("\"", included_file_basename, "\"");

  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"`include ",
       {kTag, quoted_included_file},
       "\n module ",
       {kTag, "my_module"},
       ";\nendmodule"},
  };

  IncludeTestProject project(kTestCase.code, included_file_basename,
                             kTestCase0.code, {::testing::TempDir()});

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      // No subtree from project.IncludeFile(), due to parse error.
      project.XUnit().RebaseFileTree(
          T(project.XUnit().ExpectedFileData(),
            // refers to include.
            T(D{
                IndexingFactType::kInclude,
                Anchor(kTestCase.expected_tokens[1]),
                Anchor(absl::make_unique<std::string>(
                    project.IncludeFile().source_file->ResolvedPath())),
            }),
            // refers to module my_module.
            T(D{
                IndexingFactType::kModule,
                Anchor(kTestCase.expected_tokens[3]),
            }))));

  const auto facts_tree = project.Extract();

  // Check that a parse error was recorded.
  const auto statuses(project.GetErrorStatuses());
  ASSERT_EQ(statuses.size(), 1);
  EXPECT_EQ(statuses.front().code(), absl::StatusCode::kInvalidArgument);

  const auto result_pair = DeepEqual(facts_tree, expected);
  // Print showing raw addresses instead of in-file byte-offsets.
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(FactsTreeExtractor, IncludedFileNotFound) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCase0{
      {"parameter int foo = 7;"},  // parse error
  };

  const std::string included_file_basename(
      verible::file::testing::RandomFileBasename("include-this"));

  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"`include ",
       {kTag, "\"wrong-wrong.wrong-file\""},
       "\n module ",
       {kTag, "my_module"},
       ";\nendmodule"},
  };

  IncludeTestProject project(kTestCase.code, included_file_basename,
                             kTestCase0.code, {::testing::TempDir()});

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      // included file was not found, and ignored
      project.XUnit().RebaseFileTree(
          T(project.XUnit().ExpectedFileData(),
            // No include node created.
            T(
                // refers to module my_module.
                D{
                    IndexingFactType::kModule,
                    Anchor(kTestCase.expected_tokens[3]),
                }))));

  const auto facts_tree = project.Extract();

  // Confirm that we have an error status.
  const auto statuses(project.GetErrorStatuses());
  ASSERT_EQ(statuses.size(), 1);
  EXPECT_EQ(statuses.front().code(), absl::StatusCode::kNotFound);

  const auto result_pair = DeepEqual(facts_tree, expected);
  // Print showing raw addresses instead of in-file byte-offsets.
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

  const std::string included_file_basename(
      verible::file::testing::RandomFileBasename("include-this"));
  const std::string quoted_included_file =
      absl::StrCat("\"", included_file_basename, "\"");

  const verible::SyntaxTreeSearchTestCase kTestCase = {
      {"`include ",
       {kTag, quoted_included_file},
       "\n `include ",
       {kTag, quoted_included_file},  // repeated
       "\n module ",
       {kTag, "my_module"},
       "();\n initial begin\n$display(",
       {kTag, "my_class"},
       "::",
       {kTag, "var5"},
       ");\n "
       "end\nendmodule"},
  };

  IncludeTestProject project(kTestCase.code, included_file_basename,
                             kTestCase0.code, {::testing::TempDir()});

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.IncludeFile().RebaseFileTree(T(  //
          project.IncludeFile().ExpectedFileData(),
          // refers to class my_class.
          T(
              D{
                  IndexingFactType::kClass,
                  Anchor(kTestCase0.expected_tokens[1]),
              },
              // refers to int var5.
              T(D{
                  IndexingFactType::kVariableDefinition,
                  Anchor(kTestCase0.expected_tokens[3]),
              })))),
      project.XUnit().RebaseFileTree(T(  //
          project.XUnit().ExpectedFileData(),
          // refers to include.
          T(D{
              IndexingFactType::kInclude,
              Anchor(kTestCase.expected_tokens[1]),
              Anchor(absl::make_unique<std::string>(
                  project.IncludeFile().source_file->ResolvedPath())),
          }),
          // refers to include (duplicate).
          T(D{
              IndexingFactType::kInclude,
              Anchor(kTestCase.expected_tokens[3]),
              Anchor(absl::make_unique<std::string>(
                  project.IncludeFile().source_file->ResolvedPath())),
          }),
          // refers to module my_module.
          T(
              D{
                  IndexingFactType::kModule,
                  Anchor(kTestCase.expected_tokens[5]),
              },
              // anonymous scope for initial.
              T(
                  D{
                      IndexingFactType::kAnonymousScope,
                      Anchor(
                          absl::make_unique<std::string>("anonymous-scope-0")),
                  },
                  // refers to $display(my_class::var5).
                  T(D{
                      IndexingFactType::kMemberReference,
                      Anchor(kTestCase.expected_tokens[7]),
                      Anchor(kTestCase.expected_tokens[9]),
                  }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  // Print showing raw addresses instead of in-file byte-offsets.
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(
          T(project.XUnit().ExpectedFileData(),
            // refers to package pkg.
            T(
                D{
                    IndexingFactType::kPackage,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to AA.
                T(D{
                    IndexingFactType::kConstant,
                    Anchor(kTestCase.expected_tokens[3]),
                }),
                // refers to enum m_var.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[5]),
                }),
                // refers to enum var.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[9]),
                }),
                // refers to BB.
                T(D{
                    IndexingFactType::kConstant,
                    Anchor(kTestCase.expected_tokens[7]),
                })),
            // refers to module m.
            T(
                D{
                    IndexingFactType::kModule,
                    Anchor(kTestCase.expected_tokens[11]),
                },
                // refers to CC.
                T(D{
                    IndexingFactType::kConstant,
                    Anchor(kTestCase.expected_tokens[13]),
                }),
                // refers to enum m_var.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[15]),
                }),
                // refers to enum var2.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[19]),
                }),
                // refers to DD.
                T(D{
                    IndexingFactType::kConstant,
                    Anchor(kTestCase.expected_tokens[17]),
                }),
                // refers to GG.
                T(
                    D{
                        IndexingFactType::kConstant,
                        Anchor(kTestCase.expected_tokens[21]),
                    },
                    // refers to y.
                    T(D{
                        IndexingFactType::kVariableReference,
                        Anchor(kTestCase.expected_tokens[23]),
                    }),
                    // refers to idx.
                    T(D{
                        IndexingFactType::kVariableReference,
                        Anchor(kTestCase.expected_tokens[25]),
                    })),
                // refers to enum var3.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[27]),
                }),
                // refers to enum var5.
                T(D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[35]),
                }),
                // refers to HH.
                T(
                    D{
                        IndexingFactType::kConstant,
                        Anchor(kTestCase.expected_tokens[29]),
                    },
                    // refers to yh.
                    T(D{
                        IndexingFactType::kVariableReference,
                        Anchor(kTestCase.expected_tokens[31]),
                    }),
                    // refers to idx2.
                    T(D{
                        IndexingFactType::kVariableReference,
                        Anchor(kTestCase.expected_tokens[33]),
                    }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to module pkg.
            T(
                D{
                    IndexingFactType::kModule,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to var1.
                T(
                    D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[5]),
                    },
                    // refers to x.
                    T(D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[3]),
                    }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to module pkg.
            T(
                D{
                    IndexingFactType::kModule,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to var1.
                T(
                    D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[5]),
                    },
                    // refers to x.
                    T(D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[3]),
                    }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to class my_class.
            T(
                D{
                    IndexingFactType::kClass,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to new.
                T(
                    D{
                        IndexingFactType::kConstructor,
                        Anchor(kTestCase.expected_tokens[3]),
                    },
                    // refers to x.
                    T(D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[5]),
                    }),
                    // refers to x.
                    T(D{
                        IndexingFactType::kVariableReference,
                        Anchor(kTestCase.expected_tokens[7]),
                    }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to package pkg.
            T(
                D{
                    IndexingFactType::kPackage,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to var1.
                T(
                    D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[5]),
                    },
                    // refers to x.
                    T(D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[3]),
                    }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to module pkg.
            T(
                D{
                    IndexingFactType::kModule,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to var1.
                T(
                    D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[5]),
                    },
                    // refers to x.
                    T(D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[3]),
                    }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to package pkg.
            T(
                D{
                    IndexingFactType::kPackage,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to var1.
                T(
                    D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[5]),
                    },
                    // refers to x.
                    T(D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[3]),
                    }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to package pkg.
            T(
                D{
                    IndexingFactType::kPackage,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to var1.
                T(
                    D{
                        IndexingFactType::kStructOrUnion,
                        Anchor(kTestCase.expected_tokens[5]),
                    },
                    // refers to x.
                    T(D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[3]),
                    }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to module pkg.
            T(
                D{
                    IndexingFactType::kModule,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to var1.
                T(
                    D{
                        IndexingFactType::kStructOrUnion,
                        Anchor(kTestCase.expected_tokens[5]),
                    },
                    // refers to x.
                    T(D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[3]),
                    }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to package pkg.
            T(
                D{
                    IndexingFactType::kPackage,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to var1.
                T(
                    D{
                        IndexingFactType::kStructOrUnion,
                        Anchor(kTestCase.expected_tokens[5]),
                    },
                    // refers to x.
                    T(D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[3]),
                    }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to module pkg.
            T(
                D{
                    IndexingFactType::kModule,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to var1.
                T(
                    D{
                        IndexingFactType::kStructOrUnion,
                        Anchor(kTestCase.expected_tokens[5]),
                    },
                    // refers to x.
                    T(D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[3]),
                    }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to struct var1.
            T(
                D{
                    IndexingFactType::kStructOrUnion,
                    Anchor(kTestCase.expected_tokens[5]),
                },
                // refers to some_type var1.
                T(
                    D{
                        IndexingFactType::kDataTypeReference,
                        Anchor(kTestCase.expected_tokens[1]),
                    },
                    // refers to var1.
                    T(D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[3]),
                    }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to union var1.
            T(
                D{
                    IndexingFactType::kStructOrUnion,
                    Anchor(kTestCase.expected_tokens[5]),
                },
                // refers to struct var2.
                T(
                    D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[3]),
                    },
                    // refers to x.
                    T(D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[1]),
                    }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to union var1.
            T(
                D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[5]),
                },
                // refers to struct var2.
                T(
                    D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[3]),
                    },
                    // refers to x.
                    T(D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[1]),
                    }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to union var1.
            T(
                D{
                    IndexingFactType::kStructOrUnion,
                    Anchor(kTestCase.expected_tokens[5]),
                },
                // refers to struct var2.
                T(
                    D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[3]),
                    },
                    // refers to x.
                    T(D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[1]),
                    }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to union var1.
            T(
                D{
                    IndexingFactType::kVariableDefinition,
                    Anchor(kTestCase.expected_tokens[5]),
                },
                // refers to struct var2.
                T(
                    D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[3]),
                    },
                    // refers to x.
                    T(D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[1]),
                    }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to package m.
            T(
                D{
                    IndexingFactType::kPackage,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to some_type.
                T(
                    D{
                        IndexingFactType::kDataTypeReference,
                        Anchor(kTestCase.expected_tokens[3]),
                    },
                    // refers to var1.
                    T(D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[5]),
                    }))),
            // refers to module m.
            T(
                D{
                    IndexingFactType::kModule,
                    Anchor(kTestCase.expected_tokens[7]),
                },
                // refers to some_type1.
                T(
                    D{
                        IndexingFactType::kDataTypeReference,
                        Anchor(kTestCase.expected_tokens[9]),
                    },
                    // refers to var2.
                    T(D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[11]),
                    }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to function pkg::f.
            T(D{
                IndexingFactType::kFunctionOrTask,
                Anchor(kTestCase.expected_tokens[1]),
                Anchor(kTestCase.expected_tokens[3]),
            }),
            // refers to task pkg::t.
            T(D{
                IndexingFactType::kFunctionOrTask,
                Anchor(kTestCase.expected_tokens[5]),
                Anchor(kTestCase.expected_tokens[7]),
            }))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to class env.
            T(
                D{
                    IndexingFactType::kClass,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to function mod_if.
                T(
                    D{
                        IndexingFactType::kFunctionOrTaskForwardDeclaration,
                        Anchor(kTestCase.expected_tokens[3]),
                    },
                    // refers to x.
                    T(D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[5]),
                    }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to class env.
            T(
                D{
                    IndexingFactType::kClass,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to function mod_if.
                T(
                    D{
                        IndexingFactType::kFunctionOrTaskForwardDeclaration,
                        Anchor(kTestCase.expected_tokens[3]),
                    },
                    // refers to x.
                    T(D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[5]),
                    }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to class env.
            T(
                D{
                    IndexingFactType::kClass,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to function mod_if.
                T(
                    D{
                        IndexingFactType::kFunctionOrTaskForwardDeclaration,
                        Anchor(kTestCase.expected_tokens[3]),
                    },
                    // refers to x.
                    T(D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[5]),
                    }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to class env.
            T(
                D{
                    IndexingFactType::kClass,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to task mod_if.
                T(
                    D{
                        IndexingFactType::kFunctionOrTaskForwardDeclaration,
                        Anchor(kTestCase.expected_tokens[3]),
                    },
                    // refers to x.
                    T(D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[5]),
                    }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to class env.
            T(
                D{
                    IndexingFactType::kClass,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to extends uvm_env.
                T(D{
                    IndexingFactType::kExtends,
                    Anchor(kTestCase.expected_tokens[3]),
                }),
                // refers to mod_if.
                T(
                    D{
                        IndexingFactType::kDataTypeReference,
                        Anchor(kTestCase.expected_tokens[5]),
                    },
                    // refers to m_if.
                    T(D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[7]),
                    }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to function f1.
            T(
                D{
                    IndexingFactType::kFunctionOrTask,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to f2.
                T(
                    D{
                        IndexingFactType::kFunctionCall,
                        Anchor(kTestCase.expected_tokens[3]),
                    },
                    // refers to a.
                    T(
                        D{
                            IndexingFactType::kNamedParam,
                            Anchor(kTestCase.expected_tokens[5]),
                        },
                        // refers to x.
                        T(D{
                            IndexingFactType::kVariableReference,
                            Anchor(kTestCase.expected_tokens[7]),
                        })),
                    // refers to b.
                    T(
                        D{
                            IndexingFactType::kNamedParam,
                            Anchor(kTestCase.expected_tokens[9]),
                        },
                        // refers to y.
                        T(D{
                            IndexingFactType::kVariableReference,
                            Anchor(kTestCase.expected_tokens[11]),
                        })))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to function f1.
            T(
                D{
                    IndexingFactType::kFunctionOrTask,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to t.
                T(
                    D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[7]),
                    },
                    // refers to x.
                    T(D{
                        IndexingFactType::kVariableReference,
                        Anchor(kTestCase.expected_tokens[3]),
                    }),
                    // refers to y.
                    T(D{
                        IndexingFactType::kVariableReference,
                        Anchor(kTestCase.expected_tokens[5]),
                    }),
                    // refers to l.
                    T(D{
                        IndexingFactType::kVariableReference,
                        Anchor(kTestCase.expected_tokens[9]),
                    }),
                    // refers to r.
                    T(D{
                        IndexingFactType::kVariableReference,
                        Anchor(kTestCase.expected_tokens[11]),
                    }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to function f1.
            T(
                D{
                    IndexingFactType::kFunctionOrTask,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to foo.
                T(
                    D{
                        IndexingFactType::kDataTypeReference,
                        Anchor(kTestCase.expected_tokens[3]),
                    },
                    // refers to bar.
                    T(D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[5]),
                    }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to function f1.
            T(
                D{
                    IndexingFactType::kFunctionOrTask,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to bar.
                T(
                    D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[5]),
                    },
                    // refers to foo.
                    T(D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[3]),
                    }))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
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

  SimpleTestProject project(kTestCase.code);

  const IndexingFactNode expected(  //
      project.ExpectedFileListData(),
      project.XUnit().RebaseFileTree(  //
          T(project.XUnit().ExpectedFileData(),
            // refers to function f1.
            T(
                D{
                    IndexingFactType::kFunctionOrTask,
                    Anchor(kTestCase.expected_tokens[1]),
                },
                // refers to bar.
                T(
                    D{
                        IndexingFactType::kVariableDefinition,
                        Anchor(kTestCase.expected_tokens[7]),
                    },
                    // refers to foo.
                    T(
                        D{
                            IndexingFactType::kVariableDefinition,
                            Anchor(kTestCase.expected_tokens[5]),
                        },
                        // refers to y.
                        T(D{
                            IndexingFactType::kVariableDefinition,
                            Anchor(kTestCase.expected_tokens[3]),
                        })))))));

  const auto facts_tree = project.Extract();

  const auto result_pair = DeepEqual(facts_tree, expected);
  const auto P = [&project](const T& t) { return project.TreePrinter(t); };
  EXPECT_EQ(result_pair.left, nullptr) << P(*result_pair.left);
  EXPECT_EQ(result_pair.right, nullptr) << P(*result_pair.right);
}

}  // namespace
}  // namespace kythe
}  // namespace verilog
