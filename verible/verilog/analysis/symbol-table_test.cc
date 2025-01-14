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

#include "verible/verilog/analysis/symbol-table.h"

#include <algorithm>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/status/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verible/common/text/tree-utils.h"
#include "verible/common/util/file-util.h"
#include "verible/common/util/logging.h"
#include "verible/common/util/range.h"
#include "verible/common/util/tree-operations.h"
#include "verible/verilog/analysis/verilog-filelist.h"
#include "verible/verilog/analysis/verilog-project.h"

namespace verilog {

// Directly test some SymbolTable internals.
class SymbolTable::Tester : public SymbolTable {
 public:
  explicit Tester(VerilogProject *project) : SymbolTable(project) {}

  using SymbolTable::MutableRoot;
};

namespace {

using testing::ElementsAreArray;
using testing::HasSubstr;
using verible::file::Basename;
using verible::file::CreateDir;
using verible::file::JoinPath;
using verible::file::testing::ScopedTestFile;

// An in-memory source file that doesn't require file-system access,
// nor create temporary files.
using TestVerilogSourceFile = InMemoryVerilogSourceFile;

struct ScopePathPrinter {
  const SymbolTableNode &node;
};

static std::ostream &operator<<(std::ostream &stream,
                                const ScopePathPrinter &p) {
  return SymbolTableNodeFullPath(stream, p.node);
}

// Assert that map/set element exists at 'key', and assigns it to 'dest'
// 'key' must be printable for failure diagnostic message.
// Defined as a macro for meaningful line numbers on failure.
#define ASSIGN_MUST_FIND(dest, map, key)                 \
  const auto found_##dest(map.find(key)); /* iterator */ \
  ASSERT_NE(found_##dest, map.end())                     \
      << "No element at \"" << key << "\" in " #map;     \
  const auto &dest ABSL_ATTRIBUTE_UNUSED(                \
      found_##dest->second); /* mapped_type */

// Assert that container is not empty, and reference its first element.
// Works on any container type with .begin().
// Defined as a macro for meaningful line numbers on failure.
#define ASSIGN_MUST_HAVE_FIRST_ELEMENT(dest, container) \
  ASSERT_FALSE(container.empty());                      \
  const auto &dest(*container.begin());

// Assert that container has exactly one-element, and reference it.
// Works on any container with .size() and .begin().
// Defined as a macro for meaningful line numbers on failure.
#define ASSIGN_MUST_HAVE_UNIQUE(dest, container) \
  ASSERT_EQ(container.size(), 1);                \
  const auto &dest(*container.begin());

// Shorthand for asserting that a symbol table lookup from
// (const SymbolTableNode& scope) using (std::string_view key) must succeed,
// and is captured as (const SymbolTableNode& dest).
// Most of the time, the tester is not interested in the found_* iterator.
// This also defines 'dest##_info' as the SymbolInfo value attached to the
// 'dest' SymbolTableNode. Defined as a macro so that failure gives meaningful
// line numbers, and this allows ASSERT_NE to early exit.
#define MUST_ASSIGN_LOOKUP_SYMBOL(dest, scope, key)                       \
  const auto found_##dest = (scope).Find(key);                            \
  ASSERT_NE(found_##dest, (scope).end())                                  \
      << "No symbol at \"" << key << "\" in " << ScopePathPrinter{scope}; \
  EXPECT_EQ(found_##dest->first, key);                                    \
  const SymbolTableNode &dest(found_##dest->second);                      \
  const SymbolInfo &dest##_info ABSL_ATTRIBUTE_UNUSED(dest.Value())

// For SymbolInfo::references_map_view_type only: Assert that there is exactly
// one element at 'key' in 'map' and assign it to 'dest' (DependentReferences).
// 'map' should come from SymbolInfo::LocalReferencesMapViewForTesting().
#define ASSIGN_MUST_FIND_EXACTLY_ONE_REF(dest, map, key)                 \
  ASSIGN_MUST_FIND(dest##_candidates, map, key); /* set of candidates */ \
  ASSIGN_MUST_HAVE_UNIQUE(dest, dest##_candidates);

// Expect sequence of statuses to be empty, or print first (non-ok) status.
#define EXPECT_EMPTY_STATUSES(diagnostics)                             \
  EXPECT_EQ(diagnostics.size(), 0) << "First unexpected diagnostic:\n" \
                                   << diagnostics.front().message()

TEST(SymbolMetaTypePrintTest, Print) {
  std::ostringstream stream;
  stream << SymbolMetaType::kClass;
  EXPECT_EQ(stream.str(), "class");
}

TEST(SymbolTableNodeFullPathTest, Print) {
  using KV = SymbolTableNode::key_value_type;
  const SymbolTableNode root(
      SymbolInfo{},
      KV("AA", SymbolTableNode(SymbolInfo{}, KV("BB", SymbolTableNode{}))));
  {
    std::ostringstream stream;
    SymbolTableNodeFullPath(stream, root);
    EXPECT_EQ(stream.str(), "$root");
  }
  {
    std::ostringstream stream;
    SymbolTableNodeFullPath(stream, root.begin()->second);
    EXPECT_EQ(stream.str(), "$root::AA");
  }
  {
    std::ostringstream stream;
    SymbolTableNodeFullPath(stream, root.begin()->second.begin()->second);
    EXPECT_EQ(stream.str(), "$root::AA::BB");
  }
}

TEST(ReferenceComponentTest, MatchesMetatypeTest) {
  {  // kUnspecified matches all metatypes
    const ReferenceComponent component{
        .identifier = "",
        .ref_type = ReferenceType::kUnqualified,
        .required_metatype = SymbolMetaType::kUnspecified};
    for (const auto &other :
         {SymbolMetaType::kUnspecified, SymbolMetaType::kParameter,
          SymbolMetaType::kFunction, SymbolMetaType::kTask}) {
      const auto status = component.MatchesMetatype(other);
      EXPECT_TRUE(status.ok()) << status.message();
    }
  }
  {  // kCallable matches only kFunction and kTask
    const ReferenceComponent component{
        .identifier = "",
        .ref_type = ReferenceType::kUnqualified,
        .required_metatype = SymbolMetaType::kCallable};
    for (const auto &other :
         {SymbolMetaType::kFunction, SymbolMetaType::kTask}) {
      const auto status = component.MatchesMetatype(other);
      EXPECT_TRUE(status.ok()) << status.message();
    }
    for (const auto &other : {SymbolMetaType::kModule, SymbolMetaType::kPackage,
                              SymbolMetaType::kClass}) {
      const auto status = component.MatchesMetatype(other);
      EXPECT_FALSE(status.ok())
          << component.required_metatype << " vs. " << other;
    }
  }
  {  // kClass matches only kClass and kTypeAlias
    const ReferenceComponent component{
        .identifier = "",
        .ref_type = ReferenceType::kUnqualified,
        .required_metatype = SymbolMetaType::kClass};
    for (const auto &other :
         {SymbolMetaType::kClass, SymbolMetaType::kTypeAlias}) {
      const auto status = component.MatchesMetatype(other);
      EXPECT_TRUE(status.ok()) << status.message();
    }
    for (const auto &other :
         {SymbolMetaType::kModule, SymbolMetaType::kPackage,
          SymbolMetaType::kFunction, SymbolMetaType::kTask}) {
      const auto status = component.MatchesMetatype(other);
      EXPECT_FALSE(status.ok())
          << component.required_metatype << " vs. " << other;
    }
  }
  {  // all other types must be matched exactly
    const ReferenceComponent component{
        .identifier = "",
        .ref_type = ReferenceType::kUnqualified,
        .required_metatype = SymbolMetaType::kFunction};

    for (const auto &other :
         {SymbolMetaType::kUnspecified, SymbolMetaType::kParameter,
          SymbolMetaType::kModule, SymbolMetaType::kTask,
          SymbolMetaType::kClass}) {
      const auto status = component.MatchesMetatype(other);
      EXPECT_FALSE(status.ok())
          << component.required_metatype << " vs. " << other;
    }
  }
}

TEST(ReferenceNodeFullPathTest, Print) {
  using Node = ReferenceComponentNode;
  using Data = ReferenceComponent;
  const Node root(
      Data{.identifier = "xx",
           .ref_type = ReferenceType::kUnqualified,
           .required_metatype = SymbolMetaType::kClass},
      Node(Data{.identifier = "yy", .ref_type = ReferenceType::kDirectMember},
           Node(Data{.identifier = "zz",
                     .ref_type = ReferenceType::kMemberOfTypeOfParent})));
  {
    std::ostringstream stream;
    ReferenceNodeFullPath(stream, root);
    EXPECT_EQ(stream.str(), "@xx[class]");
  }
  {
    std::ostringstream stream;
    ReferenceNodeFullPath(stream, root.Children().front());
    EXPECT_EQ(stream.str(), "@xx[class]::yy");
  }
  {
    std::ostringstream stream;
    ReferenceNodeFullPath(stream, root.Children().front().Children().front());
    EXPECT_EQ(stream.str(), "@xx[class]::yy.zz");
  }
}

TEST(DependentReferencesTest, PrintEmpty) {
  DependentReferences dep_refs;
  std::ostringstream stream;
  stream << dep_refs;
  EXPECT_EQ(stream.str(), "(empty-ref)");
}

TEST(DependentReferencesTest, PrintOnlyRootNodeUnresolved) {
  const DependentReferences dep_refs{std::make_unique<ReferenceComponentNode>(
      ReferenceComponent{.identifier = "foo",
                         .ref_type = ReferenceType::kUnqualified,
                         .required_metatype = SymbolMetaType::kUnspecified,
                         .resolved_symbol = nullptr})};
  std::ostringstream stream;
  stream << dep_refs;
  EXPECT_EQ(stream.str(), "{ (@foo -> <unresolved>) }");
}

TEST(DependentReferencesTest, PrintNonRootResolved) {
  // Synthesize a symbol table.
  using KV = SymbolTableNode::key_value_type;
  SymbolTableNode root(
      SymbolInfo{SymbolMetaType::kRoot},
      KV{"p_pkg",
         SymbolTableNode(SymbolInfo{SymbolMetaType::kPackage},
                         KV{"c_class", SymbolTableNode(SymbolInfo{
                                           SymbolMetaType::kClass})})});

  // Bookmark symbol table nodes.
  MUST_ASSIGN_LOOKUP_SYMBOL(p_pkg, root, "p_pkg");
  MUST_ASSIGN_LOOKUP_SYMBOL(c_class, p_pkg, "c_class");

  // Construct references already resolved to above nodes.
  const DependentReferences dep_refs{std::make_unique<ReferenceComponentNode>(
      ReferenceComponent{.identifier = "p_pkg",
                         .ref_type = ReferenceType::kUnqualified,
                         .required_metatype = SymbolMetaType::kPackage,
                         .resolved_symbol = &p_pkg},
      ReferenceComponentNode(
          ReferenceComponent{.identifier = "c_class",
                             .ref_type = ReferenceType::kDirectMember,
                             .required_metatype = SymbolMetaType::kClass,
                             .resolved_symbol = &c_class}))};

  // Print and compare.
  std::ostringstream stream;
  stream << dep_refs;
  EXPECT_EQ(stream.str(),
            R"({ (@p_pkg[package] -> $root::p_pkg)
  { (::c_class[class] -> $root::p_pkg::c_class) }
})");
}

TEST(SymbolTablePrintTest, PrintClass) {
  TestVerilogSourceFile src("foobar.sv",
                            "module ss;\n"
                            "endmodule\n"
                            "module tt;\n"
                            "  ss qq();\n"
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok());
  SymbolTable symbol_table(nullptr);
  EXPECT_EQ(symbol_table.Project(), nullptr);

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  {
    std::ostringstream stream;
    symbol_table.PrintSymbolDefinitions(stream);
    EXPECT_EQ(stream.str(), R"({ (
    metatype: <root>
)
  ss: { (
      metatype: module
      file: foobar.sv
  ) }
  tt: { (
      metatype: module
      file: foobar.sv
  )
    qq: { (
        metatype: data/net/var/instance
        file: foobar.sv
        type-info { source: "ss", type ref: { (@ss -> <unresolved>) } }
    ) }
  }
})");
  }
  {
    std::ostringstream stream;
    symbol_table.PrintSymbolReferences(stream);
    EXPECT_EQ(stream.str(), R"({ (refs: )
  ss: { (refs: ) }
  tt: { (refs:
      { (@ss -> <unresolved>) }
      { (@qq[data/net/var/instance] -> $root::tt::qq) }
      )
    qq: { (refs: ) }
  }
})");
  }

  {  // Resolve symbols.
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }

  {  // <unresolved> should now become "$root::ss"
    std::ostringstream stream;
    symbol_table.PrintSymbolDefinitions(stream);
    EXPECT_EQ(stream.str(), R"({ (
    metatype: <root>
)
  ss: { (
      metatype: module
      file: foobar.sv
  ) }
  tt: { (
      metatype: module
      file: foobar.sv
  )
    qq: { (
        metatype: data/net/var/instance
        file: foobar.sv
        type-info { source: "ss", type ref: { (@ss -> $root::ss) } }
    ) }
  }
})");
  }
  {  // <unresolved> should now become "$root::ss"
    std::ostringstream stream;
    symbol_table.PrintSymbolReferences(stream);
    EXPECT_EQ(stream.str(), R"({ (refs: )
  ss: { (refs: ) }
  tt: { (refs:
      { (@ss -> $root::ss) }
      { (@qq[data/net/var/instance] -> $root::tt::qq) }
      )
    qq: { (refs: ) }
  }
})");
  }
}

TEST(BuildSymbolTableTest, IntegrityCheckResolvedSymbol) {
  const auto test_func = []() {
    SymbolTable::Tester symbol_table_1(nullptr), symbol_table_2(nullptr);
    SymbolTableNode &root1(symbol_table_1.MutableRoot());
    SymbolTableNode &root2(symbol_table_2.MutableRoot());
    // Deliberately point from one symbol table to the other.
    // To avoid an use-after-free AddressSanitizer error,
    // mind the destruction ordering here:
    // symbol_table1 will outlive symbol_table_2, so give symbol_table_2 a
    // pointer to symbol_table_1.
    root2.Value().local_references_to_bind.emplace_back(
        std::make_unique<ReferenceComponentNode>(ReferenceComponent{
            .identifier = "foo",
            .ref_type = ReferenceType::kUnqualified,
            .required_metatype = SymbolMetaType::kUnspecified,
            .resolved_symbol = &root1}));
    // CheckIntegrity() will fail on destruction of symbol_table_2.
  };
  EXPECT_DEATH(test_func(),
               "Resolved symbols must point to a node in the same SymbolTable");
}

TEST(BuildSymbolTableTest, IntegrityCheckDeclaredType) {
  const auto test_func = []() {
    SymbolTable::Tester symbol_table_1(nullptr), symbol_table_2(nullptr);
    SymbolTableNode &root1(symbol_table_1.MutableRoot());
    SymbolTableNode &root2(symbol_table_2.MutableRoot());
    // Deliberately point from one symbol table to the other.
    // To avoid an use-after-free AddressSanitizer error,
    // mind the destruction ordering here:
    // symbol_table1 will outlive symbol_table_2, so give symbol_table_2 a
    // pointer to symbol_table_1.
    root1.Value().local_references_to_bind.emplace_back(
        std::make_unique<ReferenceComponentNode>(ReferenceComponent{
            .identifier = "foo",
            .ref_type = ReferenceType::kUnqualified,
            .required_metatype = SymbolMetaType::kUnspecified,
            .resolved_symbol = &root1}));
    root2.Value().declared_type.user_defined_type =
        root1.Value().local_references_to_bind.front().components.get();
    // CheckIntegrity() will fail on destruction of symbol_table_2.
  };
  EXPECT_DEATH(test_func(),
               "Resolved symbols must point to a node in the same SymbolTable");
}

TEST(BuildSymbolTableTest, InvalidSyntax) {
  constexpr std::string_view invalid_codes[] = {
      "module;\nendmodule\n",
  };
  for (const auto &code : invalid_codes) {
    TestVerilogSourceFile src("foobar.sv", code);
    const auto status = src.Parse();
    EXPECT_FALSE(status.ok());
    SymbolTable symbol_table(nullptr);
    EXPECT_EQ(symbol_table.Project(), nullptr);

    {  // Attempt to build symbol table after parse failure.
      const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

      EXPECT_TRUE(symbol_table.Root().Children().empty());
      EXPECT_EMPTY_STATUSES(build_diagnostics);
    }
    {  // Attempt to resolve empty symbol table and references.
      std::vector<absl::Status> resolve_diagnostics;
      symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
      EXPECT_EMPTY_STATUSES(resolve_diagnostics);
    }
  }
}

TEST(BuildSymbolTableTest, AvoidCrashFromFuzzer) {
  // All that matters is that these test cases do not trigger crashes.
  constexpr std::string_view codes[] = {
      // some of these test cases come from fuzz testing
      // and may contain syntax errors
      "`e(C*C);\n",              // expect two distinct reference trees
      "`e(C::D * C.m + 12);\n",  // expect two reference trees
      "n#7;\n",
      "c#1;;=P;\n",
  };
  for (const auto &code : codes) {
    TestVerilogSourceFile src("foobar.sv", code);
    const auto status = src.Parse();  // don't care if code is valid or not
    SymbolTable symbol_table(nullptr);
    EXPECT_EQ(symbol_table.Project(), nullptr);

    {  // Attempt to build symbol table.
      const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
      // don't care about diagnostics
    }
    {  // Attempt to resolve empty symbol table and references.
      std::vector<absl::Status> resolve_diagnostics;
      symbol_table.Resolve(&resolve_diagnostics);
      // don't care about diagnostics
    }
  }
}

TEST(BuildSymbolTableTest, ModuleDeclarationSingleEmpty) {
  TestVerilogSourceFile src("foobar.sv", "module m;\nendmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(module_node, root_symbol, "m");
  EXPECT_EQ(module_node_info.metatype, SymbolMetaType::kModule);
  EXPECT_EQ(module_node_info.file_origin, &src);
  EXPECT_EQ(module_node_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, ModuleDeclarationLocalNetsVariables) {
  TestVerilogSourceFile src("foobar.sv",
                            "module m;\n"
                            "  wire w1, w2;\n"
                            "  logic l1, l2;\n"
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(module_node, root_symbol, "m");
  EXPECT_EQ(module_node_info.metatype, SymbolMetaType::kModule);
  EXPECT_EQ(module_node_info.file_origin, &src);
  EXPECT_EQ(module_node_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  static constexpr std::string_view members[] = {"w1", "w2", "l1", "l2"};
  for (const auto &member : members) {
    MUST_ASSIGN_LOOKUP_SYMBOL(member_node, module_node, member);
    EXPECT_EQ(member_node_info.metatype,
              SymbolMetaType::kDataNetVariableInstance);
    EXPECT_EQ(member_node_info.declared_type.user_defined_type,
              nullptr);  // types are primitive
  }

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, ModuleDeclarationLocalDuplicateNets) {
  TestVerilogSourceFile src("foobar.sv",
                            "module m;\n"
                            "  wire y1;\n"
                            "  logic y1;\n"  // y1 already declared
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(module_node, root_symbol, "m");
  EXPECT_EQ(module_node_info.metatype, SymbolMetaType::kModule);
  EXPECT_EQ(module_node_info.file_origin, &src);
  EXPECT_EQ(module_node_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type

  ASSIGN_MUST_HAVE_UNIQUE(err_status, build_diagnostics);
  EXPECT_EQ(err_status.code(), absl::StatusCode::kAlreadyExists);
  EXPECT_THAT(err_status.message(),
              HasSubstr("\"y1\" is already defined in the $root::m scope"));

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, ModuleDeclarationConditionalGenerateAnonymous) {
  constexpr std::string_view source_variants[] = {
      // with begin/end
      "module m;\n"
      "  if (1) begin\n"
      "    wire x;\n"
      "  end else if (2) begin\n"
      "    wire y;\n"
      "  end else begin\n"
      "    wire z;\n"
      "  end\n"
      "endmodule\n",
      // without begin/end
      "module m;\n"
      "  if (1)\n"
      "    wire x;\n"
      "  else if (2)\n"
      "    wire y;\n"
      "  else\n"
      "    wire z;\n"
      "endmodule\n",
  };
  for (const auto &code : source_variants) {
    TestVerilogSourceFile src("foobar.sv", code);
    const auto status = src.Parse();
    ASSERT_TRUE(status.ok()) << status.message();
    SymbolTable symbol_table(nullptr);
    const SymbolTableNode &root_symbol(symbol_table.Root());

    const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
    EXPECT_EMPTY_STATUSES(build_diagnostics);

    MUST_ASSIGN_LOOKUP_SYMBOL(module_node, root_symbol, "m");
    EXPECT_EQ(module_node_info.metatype, SymbolMetaType::kModule);
    EXPECT_EQ(module_node_info.file_origin, &src);
    EXPECT_EQ(module_node_info.declared_type.syntax_origin,
              nullptr);  // there is no module meta-type

    ASSERT_EQ(module_node.Children().size(), 3);
    auto iter = module_node.Children().begin();
    {
      const SymbolTableNode &gen_block(iter->second);  // anonymous "...-0"
      const SymbolInfo &gen_block_info(gen_block.Value());
      EXPECT_EQ(gen_block_info.metatype, SymbolMetaType::kGenerate);
      MUST_ASSIGN_LOOKUP_SYMBOL(wire_x, gen_block, "x");
      EXPECT_EQ(wire_x_info.metatype, SymbolMetaType::kDataNetVariableInstance);
      ++iter;
    }
    {
      const SymbolTableNode &gen_block(iter->second);  // anonymous "...-1"
      const SymbolInfo &gen_block_info(gen_block.Value());
      EXPECT_EQ(gen_block_info.metatype, SymbolMetaType::kGenerate);
      MUST_ASSIGN_LOOKUP_SYMBOL(wire_y, gen_block, "y");
      EXPECT_EQ(wire_y_info.metatype, SymbolMetaType::kDataNetVariableInstance);
      ++iter;
    }
    {
      const SymbolTableNode &gen_block(iter->second);  // anonymous "...-2"
      const SymbolInfo &gen_block_info(gen_block.Value());
      EXPECT_EQ(gen_block_info.metatype, SymbolMetaType::kGenerate);
      MUST_ASSIGN_LOOKUP_SYMBOL(wire_z, gen_block, "z");
      EXPECT_EQ(wire_z_info.metatype, SymbolMetaType::kDataNetVariableInstance);
      ++iter;
    }

    {
      std::vector<absl::Status> resolve_diagnostics;
      symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
      EXPECT_EMPTY_STATUSES(resolve_diagnostics);
    }
  }
}

TEST(BuildSymbolTableTest, ModuleDeclarationConditionalGenerateLabeled) {
  TestVerilogSourceFile src("foobar.sv",
                            "module m;\n"
                            "  if (1) begin : cc\n"
                            "    wire x;\n"
                            "  end else if (2) begin : bb\n"
                            "    wire y;\n"
                            "  end else begin : aa\n"
                            "    wire z;\n"
                            "  end\n"
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(module_node, root_symbol, "m");
  EXPECT_EQ(module_node_info.metatype, SymbolMetaType::kModule);
  EXPECT_EQ(module_node_info.file_origin, &src);
  EXPECT_EQ(module_node_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type

  ASSERT_EQ(module_node.Children().size(), 3);
  {
    MUST_ASSIGN_LOOKUP_SYMBOL(gen_block, module_node, "aa");
    EXPECT_EQ(gen_block_info.metatype, SymbolMetaType::kGenerate);
    MUST_ASSIGN_LOOKUP_SYMBOL(wire_z, gen_block, "z");
    EXPECT_EQ(wire_z_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  }
  {
    MUST_ASSIGN_LOOKUP_SYMBOL(gen_block, module_node, "bb");
    EXPECT_EQ(gen_block_info.metatype, SymbolMetaType::kGenerate);
    MUST_ASSIGN_LOOKUP_SYMBOL(wire_y, gen_block, "y");
    EXPECT_EQ(wire_y_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  }
  {
    MUST_ASSIGN_LOOKUP_SYMBOL(gen_block, module_node, "cc");
    EXPECT_EQ(gen_block_info.metatype, SymbolMetaType::kGenerate);
    MUST_ASSIGN_LOOKUP_SYMBOL(wire_x, gen_block, "x");
    EXPECT_EQ(wire_x_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  }

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, ModuleDeclarationWithPorts) {
  TestVerilogSourceFile src("foobar.sv",
                            "module m (\n"
                            "  input wire clk,\n"
                            "  output reg q\n"
                            ");\n"
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(module_node, root_symbol, "m");
  EXPECT_EQ(module_node_info.metatype, SymbolMetaType::kModule);
  EXPECT_EQ(module_node_info.file_origin, &src);
  EXPECT_EQ(module_node_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type

  static constexpr std::string_view members[] = {"clk", "q"};
  for (const auto &member : members) {
    MUST_ASSIGN_LOOKUP_SYMBOL(member_node, module_node, member);
    EXPECT_EQ(member_node_info.metatype,
              SymbolMetaType::kDataNetVariableInstance);
    EXPECT_EQ(member_node_info.declared_type.user_defined_type,
              nullptr);  // types are primitive
  }

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, ModuleDeclarationMultiple) {
  TestVerilogSourceFile src("foobar.sv",
                            "module m1;\nendmodule\n"
                            "module m2;\nendmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  const std::string_view expected_modules[] = {"m1", "m2"};
  for (const auto &expected_module : expected_modules) {
    MUST_ASSIGN_LOOKUP_SYMBOL(module_node, root_symbol, expected_module);
    EXPECT_EQ(module_node_info.metatype, SymbolMetaType::kModule);
    EXPECT_EQ(module_node_info.file_origin, &src);
    EXPECT_EQ(module_node_info.declared_type.syntax_origin,
              nullptr);  // there is no module meta-type
  }

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, ModuleDeclarationDuplicate) {
  TestVerilogSourceFile src("foobar.sv",
                            "module mm;\nendmodule\n"
                            "module mm;\nendmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(module_node, root_symbol, "mm");
  EXPECT_EQ(module_node_info.metatype, SymbolMetaType::kModule);
  EXPECT_EQ(module_node_info.file_origin, &src);
  EXPECT_EQ(module_node_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type

  ASSIGN_MUST_HAVE_UNIQUE(err, build_diagnostics);
  EXPECT_EQ(err.code(), absl::StatusCode::kAlreadyExists);
  EXPECT_THAT(err.message(),
              HasSubstr("\"mm\" is already defined in the $root scope"));

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, ModuleDeclarationDuplicateSeparateFiles) {
  TestVerilogSourceFile src("foobar.sv", "module mm;\nendmodule\n");
  TestVerilogSourceFile src2("foobar-2.sv", "module mm;\nendmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  const auto status2 = src2.Parse();
  ASSERT_TRUE(status2.ok()) << status2.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics1 = BuildSymbolTable(src, &symbol_table);
  const auto build_diagnostics = BuildSymbolTable(src2, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(module_node, root_symbol, "mm");
  EXPECT_EQ(module_node_info.metatype, SymbolMetaType::kModule);
  EXPECT_EQ(module_node_info.file_origin, &src);
  EXPECT_EQ(module_node_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type

  ASSIGN_MUST_HAVE_UNIQUE(err, build_diagnostics);
  EXPECT_EQ(err.code(), absl::StatusCode::kAlreadyExists);
  EXPECT_THAT(err.message(),
              HasSubstr("\"mm\" is already defined in the $root scope"));

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, ModuleDeclarationNested) {
  TestVerilogSourceFile src("foobar.sv",
                            "module m_outer;\n"
                            "  module m_inner;\n"
                            "  endmodule\n"
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(outer_module_node, root_symbol, "m_outer");
  {
    EXPECT_EQ(outer_module_node_info.metatype, SymbolMetaType::kModule);
    EXPECT_EQ(outer_module_node_info.file_origin, &src);
    EXPECT_EQ(outer_module_node_info.declared_type.syntax_origin,
              nullptr);  // there is no module meta-type
  }
  {
    MUST_ASSIGN_LOOKUP_SYMBOL(inner_module_node, outer_module_node, "m_inner");
    EXPECT_EQ(inner_module_node_info.metatype, SymbolMetaType::kModule);
    EXPECT_EQ(inner_module_node_info.file_origin, &src);
    EXPECT_EQ(inner_module_node_info.declared_type.syntax_origin,
              nullptr);  // there is no module meta-type
  }
  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, ModuleDeclarationNestedDuplicate) {
  TestVerilogSourceFile src("foobar.sv",
                            "module outer;\n"
                            "  module mm;\nendmodule\n"
                            "  module mm;\nendmodule\n"  // dupe
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(module_node, root_symbol, "outer");
  EXPECT_EQ(module_node_info.metatype, SymbolMetaType::kModule);

  ASSIGN_MUST_HAVE_UNIQUE(err, build_diagnostics);
  EXPECT_EQ(err.code(), absl::StatusCode::kAlreadyExists);
  EXPECT_THAT(err.message(),
              HasSubstr("\"mm\" is already defined in the $root::outer scope"));
  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, ModuleInstance) {
  // The following code variants should yield the same symbol table results:
  static constexpr std::string_view source_variants[] = {
      // pp defined earlier in file
      "module pp;\n"
      "endmodule\n"
      "module qq;\n"
      "  pp rr();\n"  // instance
      "endmodule\n",
      // pp defined later in file
      "module qq;\n"
      "  pp rr();\n"  // instance
      "endmodule\n"
      "module pp;\n"
      "endmodule\n",
  };
  for (const auto &code : source_variants) {
    VLOG(1) << "code:\n" << code;
    TestVerilogSourceFile src("foobar.sv", code);
    const auto status = src.Parse();
    ASSERT_TRUE(status.ok()) << status.message();
    SymbolTable symbol_table(nullptr);
    const SymbolTableNode &root_symbol(symbol_table.Root());

    const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
    EXPECT_EMPTY_STATUSES(build_diagnostics);

    // Goal: resolve the reference of "pp" to this definition node.
    MUST_ASSIGN_LOOKUP_SYMBOL(pp, root_symbol, "pp");

    // Inspect inside the "qq" module definition.
    MUST_ASSIGN_LOOKUP_SYMBOL(qq, root_symbol, "qq");

    // "rr" is an instance of type "pp"
    MUST_ASSIGN_LOOKUP_SYMBOL(rr, qq, "rr");

    {
      EXPECT_EQ(qq_info.file_origin, &src);
      ASSERT_EQ(qq_info.local_references_to_bind.size(), 2);
      const auto ref_map(qq_info.LocalReferencesMapViewForTesting());
      {
        ASSIGN_MUST_FIND_EXACTLY_ONE_REF(pp_type, ref_map, "pp");
        const ReferenceComponentNode *ref_node = pp_type->LastTypeComponent();
        ASSERT_NE(ref_node, nullptr);
        const ReferenceComponent &ref(ref_node->Value());
        EXPECT_EQ(ref.identifier, "pp");
        EXPECT_TRUE(verible::IsSubRange(ref.identifier,
                                        src.GetTextStructure()->Contents()));
        EXPECT_EQ(ref.ref_type, ReferenceType::kUnqualified);
        EXPECT_EQ(ref.required_metatype, SymbolMetaType::kUnspecified);
        EXPECT_EQ(ref.resolved_symbol, nullptr);
      }
      {  // self-reference to "rr" instance
        ASSIGN_MUST_FIND_EXACTLY_ONE_REF(rr_self_ref, ref_map, "rr");
        EXPECT_TRUE(is_leaf(*rr_self_ref->components));  // no named ports
        // self-reference is already bound.
        EXPECT_EQ(rr_self_ref->components->Value().resolved_symbol, &rr);
      }
    }

    EXPECT_TRUE(rr_info.local_references_to_bind.empty());
    EXPECT_NE(rr_info.declared_type.user_defined_type, nullptr);
    {
      const ReferenceComponent &pp_type(
          rr_info.declared_type.user_defined_type->Value());
      EXPECT_EQ(pp_type.identifier, "pp");
      EXPECT_EQ(pp_type.resolved_symbol, nullptr);  // nothing resolved yet
      EXPECT_EQ(pp_type.ref_type, ReferenceType::kUnqualified);
      EXPECT_EQ(pp_type.required_metatype, SymbolMetaType::kUnspecified);
    }
    EXPECT_EQ(rr_info.file_origin, &src);

    // Resolve symbols.
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // Verify that typeof(rr) successfully resolved to module pp.
    EXPECT_EQ(rr_info.declared_type.user_defined_type->Value().resolved_symbol,
              &pp);
  }
}

TEST(BuildSymbolTableTest, ModuleInstanceUndefined) {
  TestVerilogSourceFile src("foobar.sv",
                            "module qq;\n"
                            "  pp rr();\n"  // instance, pp undefined
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  // Inspect inside the "qq" module definition.
  MUST_ASSIGN_LOOKUP_SYMBOL(qq, root_symbol, "qq");
  {
    EXPECT_EQ(qq_info.file_origin, &src);
    // There is only one reference to the "pp" module type.
    ASSERT_EQ(qq_info.local_references_to_bind.size(), 2);
    const auto ref_map(qq_info.LocalReferencesMapViewForTesting());
    ASSIGN_MUST_FIND_EXACTLY_ONE_REF(pp_type, ref_map, "pp");
    {  // verify that a reference to "pp" was established
      const ReferenceComponentNode *ref_node = pp_type->LastTypeComponent();
      ASSERT_NE(ref_node, nullptr);
      const ReferenceComponent &ref(ref_node->Value());
      EXPECT_EQ(ref.identifier, "pp");
      EXPECT_TRUE(verible::IsSubRange(ref.identifier,
                                      src.GetTextStructure()->Contents()));
      EXPECT_EQ(ref.ref_type, ReferenceType::kUnqualified);
      EXPECT_EQ(ref.required_metatype, SymbolMetaType::kUnspecified);
      EXPECT_EQ(ref.resolved_symbol, nullptr);
    }
  }

  // "rr" is an instance of type "pp" (which is undefined)
  MUST_ASSIGN_LOOKUP_SYMBOL(rr, qq, "rr");
  EXPECT_TRUE(rr_info.local_references_to_bind.empty());
  EXPECT_NE(rr_info.declared_type.user_defined_type, nullptr);
  {
    const ReferenceComponent &pp_type(
        rr_info.declared_type.user_defined_type->Value());
    EXPECT_EQ(pp_type.identifier, "pp");
    EXPECT_EQ(pp_type.resolved_symbol, nullptr);  // nothing resolved yet
    EXPECT_EQ(pp_type.ref_type, ReferenceType::kUnqualified);
    EXPECT_EQ(pp_type.required_metatype, SymbolMetaType::kUnspecified);
  }
  EXPECT_EQ(rr_info.file_origin, &src);

  {
    // Resolve symbols.  Expect one unresolved symbol.
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);

    ASSIGN_MUST_HAVE_UNIQUE(err_status, resolve_diagnostics);
    EXPECT_EQ(err_status.code(), absl::StatusCode::kNotFound);
    EXPECT_THAT(err_status.message(),
                HasSubstr("Unable to resolve symbol \"pp\""));
    // Verify that typeof(rr) failed to resolve "pp".
    EXPECT_EQ(rr_info.declared_type.user_defined_type->Value().resolved_symbol,
              nullptr);
  }
}

TEST(BuildSymbolTableTest, ModuleInstanceTwoInSameDecl) {
  static constexpr std::string_view source_variants[] = {
      // The following all yield equivalent symbol tables bindings.
      "module pp;\n"
      "endmodule\n"
      "module qq;\n"
      "  pp r1(), r2();\n"  // instances
      "endmodule\n",
      "module qq;\n"
      "  pp r1(), r2();\n"  // instances
      "endmodule\n"
      "module pp;\n"
      "endmodule\n",
      // swap r1, r2 order
      "module pp;\n"
      "endmodule\n"
      "module qq;\n"
      "  pp r2(), r1();\n"  // instances
      "endmodule\n",
      "module qq;\n"
      "  pp r2(), r1();\n"  // instances
      "endmodule\n"
      "module pp;\n"
      "endmodule\n",
  };
  for (const auto &code : source_variants) {
    TestVerilogSourceFile src("foobar.sv", code);
    const auto status = src.Parse();
    ASSERT_TRUE(status.ok()) << status.message();
    SymbolTable symbol_table(nullptr);
    const SymbolTableNode &root_symbol(symbol_table.Root());

    const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
    EXPECT_EMPTY_STATUSES(build_diagnostics);

    MUST_ASSIGN_LOOKUP_SYMBOL(pp, root_symbol, "pp");

    // Inspect inside the "qq" module definition.
    MUST_ASSIGN_LOOKUP_SYMBOL(qq, root_symbol, "qq");
    {
      EXPECT_EQ(qq_info.file_origin, &src);
      // There is only one type reference of interest, the "pp" module type.
      // The other two are instance self-references.
      ASSERT_EQ(qq_info.local_references_to_bind.size(), 3);
      const auto ref_map(qq_info.LocalReferencesMapViewForTesting());
      ASSIGN_MUST_FIND_EXACTLY_ONE_REF(pp_type, ref_map, "pp");
      const ReferenceComponentNode *ref_node = pp_type->LastTypeComponent();
      ASSERT_NE(ref_node, nullptr);
      const ReferenceComponent &ref(ref_node->Value());
      EXPECT_EQ(ref.identifier, "pp");
      EXPECT_TRUE(verible::IsSubRange(ref.identifier,
                                      src.GetTextStructure()->Contents()));
      EXPECT_EQ(ref.ref_type, ReferenceType::kUnqualified);
      EXPECT_EQ(ref.required_metatype, SymbolMetaType::kUnspecified);
      EXPECT_EQ(ref.resolved_symbol, nullptr);
    }

    // "r1" and "r2" are both instances of type "pp"
    static constexpr std::string_view pp_instances[] = {"r1", "r2"};
    for (const auto &pp_inst : pp_instances) {
      MUST_ASSIGN_LOOKUP_SYMBOL(rr, qq, pp_inst);
      EXPECT_TRUE(rr_info.local_references_to_bind.empty());
      EXPECT_NE(rr_info.declared_type.user_defined_type, nullptr);
      {
        const ReferenceComponent &pp_type(
            rr_info.declared_type.user_defined_type->Value());
        EXPECT_EQ(pp_type.identifier, "pp");
        EXPECT_EQ(pp_type.resolved_symbol, nullptr);  // nothing resolved yet
        EXPECT_EQ(pp_type.ref_type, ReferenceType::kUnqualified);
        EXPECT_EQ(pp_type.required_metatype, SymbolMetaType::kUnspecified);
      }
      EXPECT_EQ(rr_info.file_origin, &src);
    }

    {
      std::vector<absl::Status> resolve_diagnostics;
      symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
      EXPECT_EMPTY_STATUSES(resolve_diagnostics);

      for (const auto &pp_inst : pp_instances) {
        MUST_ASSIGN_LOOKUP_SYMBOL(rr, qq, pp_inst);
        EXPECT_TRUE(rr_info.local_references_to_bind.empty());
        // Verify that typeof(r1,r2) successfully resolved to module pp.
        EXPECT_EQ(
            rr_info.declared_type.user_defined_type->Value().resolved_symbol,
            &pp);
      }
    }
  }
}

TEST(BuildSymbolTableTest, ModuleInstancePositionalPortConnection) {
  TestVerilogSourceFile src("foobar.sv",
                            "module m (\n"
                            "  input wire clk,\n"
                            "  output reg q\n"
                            ");\n"
                            "endmodule\n"
                            "module rr;\n"
                            "  wire c, d;\n"
                            "  m m_inst(c, d);"
                            // one type reference, two net references
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(m_node, root_symbol, "m");
  EXPECT_EQ(m_node_info.metatype, SymbolMetaType::kModule);
  EXPECT_EQ(m_node_info.file_origin, &src);
  EXPECT_EQ(m_node_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type

  MUST_ASSIGN_LOOKUP_SYMBOL(clk_node, m_node, "clk");
  EXPECT_EQ(clk_node_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(clk_node_info.declared_type.user_defined_type,
            nullptr);  // types are primitive

  MUST_ASSIGN_LOOKUP_SYMBOL(q_node, m_node, "q");
  EXPECT_EQ(q_node_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(q_node_info.declared_type.user_defined_type,
            nullptr);  // types are primitive

  MUST_ASSIGN_LOOKUP_SYMBOL(rr_node, root_symbol, "rr");

  // Inspect local references to wires "c" and "d".
  ASSERT_EQ(rr_node_info.local_references_to_bind.size(), 4);
  const auto ref_map(rr_node_info.LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(c_ref, ref_map, "c");
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(d_ref, ref_map, "d");
  EXPECT_EQ(c_ref->LastLeaf()->Value().identifier, "c");
  EXPECT_EQ(c_ref->LastLeaf()->Value().resolved_symbol, nullptr);
  EXPECT_EQ(d_ref->LastLeaf()->Value().identifier, "d");
  EXPECT_EQ(d_ref->LastLeaf()->Value().resolved_symbol, nullptr);

  // Get the local symbol definitions for wires "c" and "d".
  MUST_ASSIGN_LOOKUP_SYMBOL(c_node, rr_node, "c");
  MUST_ASSIGN_LOOKUP_SYMBOL(d_node, rr_node, "d");

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // Expect to resolve local references to wires "c" and "d".
    EXPECT_EQ(c_ref->LastLeaf()->Value().resolved_symbol, &c_node);
    EXPECT_EQ(d_ref->LastLeaf()->Value().resolved_symbol, &d_node);
  }
}

TEST(BuildSymbolTableTest, ModuleInstanceNamedPortConnection) {
  TestVerilogSourceFile src("foobar.sv",
                            "module m (\n"
                            "  input wire clk,\n"
                            "  output reg q\n"
                            ");\n"
                            "endmodule\n"
                            "module rr;\n"
                            "  wire c, d;\n"
                            "  m m_inst(.clk(c), .q(d));"
                            // one type reference, two local net references
                            // two named port references
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(m_node, root_symbol, "m");
  EXPECT_EQ(m_node_info.metatype, SymbolMetaType::kModule);
  EXPECT_EQ(m_node_info.file_origin, &src);
  EXPECT_EQ(m_node_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type

  MUST_ASSIGN_LOOKUP_SYMBOL(clk_node, m_node, "clk");
  EXPECT_EQ(clk_node_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(clk_node_info.declared_type.user_defined_type,
            nullptr);  // types are primitive

  MUST_ASSIGN_LOOKUP_SYMBOL(q_node, m_node, "q");
  EXPECT_EQ(q_node_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(q_node_info.declared_type.user_defined_type,
            nullptr);  // types are primitive

  MUST_ASSIGN_LOOKUP_SYMBOL(rr_node, root_symbol, "rr");

  // Inspect local references to wires "c" and "d".
  ASSERT_EQ(rr_node_info.local_references_to_bind.size(), 4);
  const auto ref_map(rr_node_info.LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(c_ref, ref_map, "c");
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(d_ref, ref_map, "d");
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(m_inst_ref, ref_map, "m_inst");
  EXPECT_EQ(c_ref->LastLeaf()->Value().identifier, "c");
  EXPECT_EQ(c_ref->LastLeaf()->Value().resolved_symbol, nullptr);
  EXPECT_EQ(d_ref->LastLeaf()->Value().identifier, "d");
  EXPECT_EQ(d_ref->LastLeaf()->Value().resolved_symbol, nullptr);

  const ReferenceComponentNode &m_inst_ref_root(*m_inst_ref->components);
  ASSERT_EQ(m_inst_ref_root.Children().size(), 2);
  const ReferenceComponentMap port_refs(
      ReferenceComponentNodeMapView(m_inst_ref_root));

  ASSIGN_MUST_FIND(clk_ref, port_refs, "clk");
  const ReferenceComponent &clk_ref_comp(clk_ref->Value());
  EXPECT_EQ(clk_ref_comp.identifier, "clk");
  EXPECT_EQ(clk_ref_comp.ref_type, ReferenceType::kMemberOfTypeOfParent);
  EXPECT_EQ(clk_ref_comp.required_metatype,
            SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(clk_ref_comp.resolved_symbol, nullptr);  // not yet resolved

  ASSIGN_MUST_FIND(q_ref, port_refs, "q");
  const ReferenceComponent &q_ref_comp(q_ref->Value());
  EXPECT_EQ(q_ref_comp.identifier, "q");
  EXPECT_EQ(q_ref_comp.ref_type, ReferenceType::kMemberOfTypeOfParent);
  EXPECT_EQ(q_ref_comp.required_metatype,
            SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(q_ref_comp.resolved_symbol, nullptr);  // not yet resolved

  // Get the local symbol definitions for wires "c" and "d".
  MUST_ASSIGN_LOOKUP_SYMBOL(c_node, rr_node, "c");
  MUST_ASSIGN_LOOKUP_SYMBOL(d_node, rr_node, "d");

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // Expect to resolve local references to wires c and d
    EXPECT_EQ(c_ref->LastLeaf()->Value().resolved_symbol, &c_node);
    EXPECT_EQ(d_ref->LastLeaf()->Value().resolved_symbol, &d_node);

    // Expect to resolved named port references to "clk" and "q".
    EXPECT_EQ(clk_ref_comp.resolved_symbol, &clk_node);
    EXPECT_EQ(q_ref_comp.resolved_symbol, &q_node);
  }
}

TEST(BuildSymbolTableTest,
     ModuleInstanceNamedPortConnectionResolveLocallyOnly) {
  // Similar to ModuleInstanceNamedPortConnection, but will not resolve
  // non-local references.
  TestVerilogSourceFile src("foobar.sv",
                            "module m (\n"
                            "  input wire clk,\n"
                            "  output reg q\n"
                            ");\n"
                            "endmodule\n"
                            "module rr;\n"
                            "  wire c, d;\n"
                            "  m m_inst(.clk(c), .q(d));"
                            // one type reference, two local net references
                            // two named port references
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(m_node, root_symbol, "m");
  EXPECT_EQ(m_node_info.metatype, SymbolMetaType::kModule);
  EXPECT_EQ(m_node_info.file_origin, &src);
  EXPECT_EQ(m_node_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type

  MUST_ASSIGN_LOOKUP_SYMBOL(clk_node, m_node, "clk");
  EXPECT_EQ(clk_node_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(clk_node_info.declared_type.user_defined_type,
            nullptr);  // types are primitive

  MUST_ASSIGN_LOOKUP_SYMBOL(q_node, m_node, "q");
  EXPECT_EQ(q_node_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(q_node_info.declared_type.user_defined_type,
            nullptr);  // types are primitive

  MUST_ASSIGN_LOOKUP_SYMBOL(rr_node, root_symbol, "rr");

  // Inspect local references to wires "c" and "d".
  ASSERT_EQ(rr_node_info.local_references_to_bind.size(), 4);
  const auto ref_map(rr_node_info.LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(c_ref, ref_map, "c");
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(d_ref, ref_map, "d");
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(m_inst_ref, ref_map, "m_inst");
  // Initially not resolved, but will be resolved below.
  EXPECT_EQ(c_ref->LastLeaf()->Value().identifier, "c");
  EXPECT_EQ(c_ref->LastLeaf()->Value().resolved_symbol, nullptr);
  EXPECT_EQ(d_ref->LastLeaf()->Value().identifier, "d");
  EXPECT_EQ(d_ref->LastLeaf()->Value().resolved_symbol, nullptr);

  const ReferenceComponentNode &m_inst_ref_root(*m_inst_ref->components);
  ASSERT_EQ(m_inst_ref_root.Children().size(), 2);
  const ReferenceComponentMap port_refs(
      ReferenceComponentNodeMapView(m_inst_ref_root));

  ASSIGN_MUST_FIND(clk_ref, port_refs, "clk");
  const ReferenceComponent &clk_ref_comp(clk_ref->Value());
  EXPECT_EQ(clk_ref_comp.identifier, "clk");
  EXPECT_EQ(clk_ref_comp.ref_type, ReferenceType::kMemberOfTypeOfParent);
  EXPECT_EQ(clk_ref_comp.required_metatype,
            SymbolMetaType::kDataNetVariableInstance);
  // "clk" is a non-local reference that will not even be resolved below
  EXPECT_EQ(clk_ref_comp.resolved_symbol, nullptr);

  ASSIGN_MUST_FIND(q_ref, port_refs, "q");
  const ReferenceComponent &q_ref_comp(q_ref->Value());
  EXPECT_EQ(q_ref_comp.identifier, "q");
  EXPECT_EQ(q_ref_comp.ref_type, ReferenceType::kMemberOfTypeOfParent);
  EXPECT_EQ(q_ref_comp.required_metatype,
            SymbolMetaType::kDataNetVariableInstance);
  // "q" is a non-local reference that will not even be resolved below
  EXPECT_EQ(q_ref_comp.resolved_symbol, nullptr);

  // Get the local symbol definitions for wires "c" and "d".
  MUST_ASSIGN_LOOKUP_SYMBOL(c_node, rr_node, "c");
  MUST_ASSIGN_LOOKUP_SYMBOL(d_node, rr_node, "d");

  // Running this twice changes nothing and is safe.
  for (int i = 0; i < 2; ++i) {
    symbol_table.ResolveLocallyOnly();

    // Expect to resolve local references to wires c and d
    EXPECT_EQ(c_ref->LastLeaf()->Value().resolved_symbol, &c_node);
    EXPECT_EQ(d_ref->LastLeaf()->Value().resolved_symbol, &d_node);

    // Expect to named port references to "clk" and "q" to remain unresolved.
    EXPECT_EQ(clk_ref_comp.resolved_symbol, nullptr);
    EXPECT_EQ(q_ref_comp.resolved_symbol, nullptr);
  }
}

TEST(BuildSymbolTableTest, ModuleInstancePositionalParameterAssignment) {
  TestVerilogSourceFile src("foobar.sv",
                            "module m #(\n"
                            "  int N = 1\n"
                            ");\n"
                            "endmodule\n"
                            "module rr;\n"
                            "  m #(3) m_inst();"
                            // one type reference to "m"
                            // one instance self-reference
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(m_node, root_symbol, "m");
  EXPECT_EQ(m_node_info.metatype, SymbolMetaType::kModule);
  EXPECT_EQ(m_node_info.file_origin, &src);
  EXPECT_EQ(m_node_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type

  MUST_ASSIGN_LOOKUP_SYMBOL(n_param, m_node, "N");
  EXPECT_EQ(n_param_info.metatype, SymbolMetaType::kParameter);
  EXPECT_EQ(n_param_info.declared_type.user_defined_type,
            nullptr);  // types are primitive

  MUST_ASSIGN_LOOKUP_SYMBOL(rr_node, root_symbol, "rr");
  MUST_ASSIGN_LOOKUP_SYMBOL(m_inst_node, rr_node, "m_inst");

  // Inspect local references to "m" and "m_inst".
  ASSERT_EQ(rr_node_info.local_references_to_bind.size(), 2);
  const auto ref_map(rr_node_info.LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(m_ref, ref_map, "m");
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(m_inst_ref, ref_map, "m_inst");
  EXPECT_EQ(m_ref->LastLeaf()->Value().identifier, "m");
  EXPECT_EQ(m_ref->LastLeaf()->Value().resolved_symbol, nullptr);
  EXPECT_EQ(m_inst_ref->LastLeaf()->Value().identifier, "m_inst");
  EXPECT_EQ(m_inst_ref->LastLeaf()->Value().resolved_symbol, &m_inst_node);

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // Expect to resolve local references to "m" and "m_inst".
    EXPECT_EQ(m_ref->LastLeaf()->Value().resolved_symbol, &m_node);
    EXPECT_EQ(m_inst_ref->LastLeaf()->Value().resolved_symbol, &m_inst_node);
  }
}

TEST(BuildSymbolTableTest, ModuleInstanceNamedParameterAssignment) {
  TestVerilogSourceFile src("foobar.sv",
                            "module m #(\n"
                            "  int N = 0,\n"
                            "  int P = 1\n"
                            ");\n"
                            "endmodule\n"
                            "module rr;\n"
                            "  m #(.N(2), .P(3)) m_inst();"
                            // one type reference, one instance self-reference
                            // two named param rereference
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(m_node, root_symbol, "m");
  EXPECT_EQ(m_node_info.metatype, SymbolMetaType::kModule);
  EXPECT_EQ(m_node_info.file_origin, &src);
  EXPECT_EQ(m_node_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type

  MUST_ASSIGN_LOOKUP_SYMBOL(n_param, m_node, "N");
  EXPECT_EQ(n_param_info.metatype, SymbolMetaType::kParameter);
  EXPECT_EQ(n_param_info.declared_type.user_defined_type,
            nullptr);  // types are primitive

  MUST_ASSIGN_LOOKUP_SYMBOL(p_param, m_node, "P");
  EXPECT_EQ(p_param_info.metatype, SymbolMetaType::kParameter);
  EXPECT_EQ(p_param_info.declared_type.user_defined_type,
            nullptr);  // types are primitive

  MUST_ASSIGN_LOOKUP_SYMBOL(rr_node, root_symbol, "rr");
  MUST_ASSIGN_LOOKUP_SYMBOL(m_inst_node, rr_node, "m_inst");

  const auto ref_map(rr_node_info.LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(m_type_ref, ref_map, "m");

  const ReferenceComponentNode &m_type_ref_root(*m_type_ref->components);
  ASSERT_EQ(m_type_ref_root.Children().size(), 2);
  const ReferenceComponentMap param_refs(
      ReferenceComponentNodeMapView(m_type_ref_root));

  ASSIGN_MUST_FIND(n_ref, param_refs, "N");
  const ReferenceComponent &n_ref_comp(n_ref->Value());
  EXPECT_EQ(n_ref_comp.identifier, "N");
  EXPECT_EQ(n_ref_comp.ref_type, ReferenceType::kDirectMember);
  EXPECT_EQ(n_ref_comp.required_metatype, SymbolMetaType::kParameter);
  EXPECT_EQ(n_ref_comp.resolved_symbol, nullptr);  // not yet resolved

  ASSIGN_MUST_FIND(p_ref, param_refs, "P");
  const ReferenceComponent &p_ref_comp(p_ref->Value());
  EXPECT_EQ(p_ref_comp.identifier, "P");
  EXPECT_EQ(p_ref_comp.ref_type, ReferenceType::kDirectMember);
  EXPECT_EQ(p_ref_comp.required_metatype, SymbolMetaType::kParameter);
  EXPECT_EQ(p_ref_comp.resolved_symbol, nullptr);  // not yet resolved

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // Expect ".N" and ".P" to resolve to formal parameters of "m".
    EXPECT_EQ(n_ref_comp.resolved_symbol, &n_param);
    EXPECT_EQ(p_ref_comp.resolved_symbol, &p_param);
  }
}

TEST(BuildSymbolTableTest, TimerAsModuleNameRegressionIssue917) {
  TestVerilogSourceFile src("foobar.sv",
                            "module foo;\n"
                            " timer #(.N(1)) t;\n"
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(foo_node, root_symbol, "foo");
  MUST_ASSIGN_LOOKUP_SYMBOL(timer_instance_node, foo_node, "t");
}

TEST(BuildSymbolTableTest, ModuleInstanceNamedPortIsParameter) {
  TestVerilogSourceFile src("foobar.sv",
                            "module m #(\n"
                            "  int N = 0\n"
                            ") (\n"
                            "  input wire clk\n"
                            ");\n"
                            "endmodule\n"
                            "module rr;\n"
                            "  m #(.clk(2)) m_inst();"
                            // error: clk is a net-port, not parameter
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(m_node, root_symbol, "m");
  EXPECT_EQ(m_node_info.metatype, SymbolMetaType::kModule);
  EXPECT_EQ(m_node_info.file_origin, &src);
  EXPECT_EQ(m_node_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type

  MUST_ASSIGN_LOOKUP_SYMBOL(n_param, m_node, "N");
  EXPECT_EQ(n_param_info.metatype, SymbolMetaType::kParameter);
  EXPECT_EQ(n_param_info.declared_type.user_defined_type,
            nullptr);  // types are primitive

  MUST_ASSIGN_LOOKUP_SYMBOL(clk_port, m_node, "clk");
  EXPECT_EQ(clk_port_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(clk_port_info.declared_type.user_defined_type,
            nullptr);  // type is primitive

  MUST_ASSIGN_LOOKUP_SYMBOL(rr_node, root_symbol, "rr");
  MUST_ASSIGN_LOOKUP_SYMBOL(m_inst_node, rr_node, "m_inst");

  const auto ref_map(rr_node_info.LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(m_type_ref, ref_map, "m");

  const ReferenceComponentNode &m_type_ref_root(*m_type_ref->components);
  ASSERT_EQ(m_type_ref_root.Children().size(), 1);
  const ReferenceComponentMap param_refs(
      ReferenceComponentNodeMapView(m_type_ref_root));

  ASSIGN_MUST_FIND(clk_ref, param_refs, "clk");
  const ReferenceComponent &clk_ref_comp(clk_ref->Value());
  EXPECT_EQ(clk_ref_comp.identifier, "clk");
  EXPECT_EQ(clk_ref_comp.ref_type, ReferenceType::kDirectMember);
  EXPECT_EQ(clk_ref_comp.required_metatype, SymbolMetaType::kParameter);
  EXPECT_EQ(clk_ref_comp.resolved_symbol, nullptr);  // not yet resolved

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);

    // Expect ".clk" to fail to resolve.
    ASSIGN_MUST_HAVE_UNIQUE(err, resolve_diagnostics);
    EXPECT_EQ(err.code(), absl::StatusCode::kInvalidArgument);
    EXPECT_THAT(err.message(),
                HasSubstr("Expecting reference \"clk\" to resolve to a "
                          "parameter, but found a data/net/var/instance"));
    EXPECT_EQ(clk_ref_comp.resolved_symbol, nullptr);  // still unresolved
  }
}

TEST(BuildSymbolTableTest, ModuleInstanceNamedParameterIsPort) {
  TestVerilogSourceFile src("foobar.sv",
                            "module m #(\n"
                            "  int N = 0\n"
                            ") (\n"
                            "  input wire clk\n"
                            ");\n"
                            "endmodule\n"
                            "module rr;\n"
                            "  m m_inst(.N(1));"
                            // error: N is a parameter, not net-port
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(m_node, root_symbol, "m");
  EXPECT_EQ(m_node_info.metatype, SymbolMetaType::kModule);
  EXPECT_EQ(m_node_info.file_origin, &src);
  EXPECT_EQ(m_node_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type

  MUST_ASSIGN_LOOKUP_SYMBOL(n_param, m_node, "N");
  EXPECT_EQ(n_param_info.metatype, SymbolMetaType::kParameter);
  EXPECT_EQ(n_param_info.declared_type.user_defined_type,
            nullptr);  // type is primitive

  MUST_ASSIGN_LOOKUP_SYMBOL(clk_port, m_node, "clk");
  EXPECT_EQ(clk_port_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(clk_port_info.declared_type.user_defined_type,
            nullptr);  // type is primitive

  MUST_ASSIGN_LOOKUP_SYMBOL(rr_node, root_symbol, "rr");
  MUST_ASSIGN_LOOKUP_SYMBOL(m_inst_node, rr_node, "m_inst");

  const auto ref_map(rr_node_info.LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(m_inst_ref, ref_map, "m_inst");

  const ReferenceComponentNode &m_inst_ref_root(*m_inst_ref->components);
  ASSERT_EQ(m_inst_ref_root.Children().size(), 1);
  const ReferenceComponentMap port_refs(
      ReferenceComponentNodeMapView(m_inst_ref_root));

  ASSIGN_MUST_FIND(n_ref, port_refs, "N");
  const ReferenceComponent &n_ref_comp(n_ref->Value());
  EXPECT_EQ(n_ref_comp.identifier, "N");
  EXPECT_EQ(n_ref_comp.ref_type, ReferenceType::kMemberOfTypeOfParent);
  EXPECT_EQ(n_ref_comp.required_metatype,
            SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(n_ref_comp.resolved_symbol, nullptr);  // not yet resolved

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);

    // Expect ".N" to fail to resolve.
    ASSIGN_MUST_HAVE_UNIQUE(err, resolve_diagnostics);
    EXPECT_EQ(err.code(), absl::StatusCode::kInvalidArgument);
    EXPECT_THAT(err.message(),
                HasSubstr("Expecting reference \"N\" to resolve to a "
                          "data/net/var/instance, but found a parameter"));
    EXPECT_EQ(n_ref_comp.resolved_symbol, nullptr);  // still unresolved
  }
}

TEST(BuildSymbolTableTest, ModuleInstanceNamedPortConnectionNonexistentPort) {
  TestVerilogSourceFile src("foobar.sv",
                            "module m (\n"
                            "  input wire clk,\n"
                            "  output reg q\n"
                            ");\n"
                            "endmodule\n"
                            "module rr;\n"
                            "  wire c;\n"
                            "  m m_inst(.clk(c), .p(c));"
                            // one type reference, two local net references
                            // two named port references, "p" does not exist
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(m_node, root_symbol, "m");

  MUST_ASSIGN_LOOKUP_SYMBOL(clk_node, m_node, "clk");

  MUST_ASSIGN_LOOKUP_SYMBOL(q_node, m_node, "q");

  MUST_ASSIGN_LOOKUP_SYMBOL(rr_node, root_symbol, "rr");

  const auto ref_map(rr_node_info.LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(m_inst_ref, ref_map, "m_inst");
  ASSERT_NE(m_inst_ref, nullptr);

  const ReferenceComponentNode &m_inst_ref_root(*m_inst_ref->components);
  ASSERT_EQ(m_inst_ref_root.Children().size(), 2);
  const ReferenceComponentMap port_refs(
      ReferenceComponentNodeMapView(m_inst_ref_root));

  ASSIGN_MUST_FIND(clk_ref, port_refs, "clk");
  const ReferenceComponent &clk_ref_comp(clk_ref->Value());
  EXPECT_EQ(clk_ref_comp.identifier, "clk");
  EXPECT_EQ(clk_ref_comp.ref_type, ReferenceType::kMemberOfTypeOfParent);
  EXPECT_EQ(clk_ref_comp.required_metatype,
            SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(clk_ref_comp.resolved_symbol, nullptr);  // not yet resolved

  ASSIGN_MUST_FIND(p_ref, port_refs, "p");
  const ReferenceComponent &p_ref_comp(p_ref->Value());
  EXPECT_EQ(p_ref_comp.identifier, "p");
  EXPECT_EQ(p_ref_comp.ref_type, ReferenceType::kMemberOfTypeOfParent);
  EXPECT_EQ(p_ref_comp.required_metatype,
            SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(p_ref_comp.resolved_symbol, nullptr);  // not yet resolved

  // Get the local symbol definitions for wire "c".
  MUST_ASSIGN_LOOKUP_SYMBOL(c_node, rr_node, "c");

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);

    ASSIGN_MUST_HAVE_UNIQUE(err, resolve_diagnostics);
    EXPECT_EQ(err.code(), absl::StatusCode::kNotFound);
    EXPECT_THAT(
        err.message(),
        HasSubstr("No member symbol \"p\" in parent scope (module) m."));

    // Expect to resolved named port reference to "clk", but not "p".
    EXPECT_EQ(clk_ref_comp.resolved_symbol, &clk_node);
    EXPECT_EQ(p_ref_comp.resolved_symbol, nullptr);  // failed to resolve
  }
}

TEST(BuildSymbolTableTest, ModuleInstanceNamedParameterNonexistentError) {
  TestVerilogSourceFile src("foobar.sv",
                            "module m #(\n"
                            "  int N = 0,\n"
                            "  int P = 1\n"
                            ");\n"
                            "endmodule\n"
                            "module rr;\n"
                            "  m #(.N(2), .Q(3)) m_inst();"
                            // one type reference, one instance self-reference
                            // two named param rereference (one error)
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(m_node, root_symbol, "m");
  EXPECT_EQ(m_node_info.metatype, SymbolMetaType::kModule);
  EXPECT_EQ(m_node_info.file_origin, &src);
  EXPECT_EQ(m_node_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type

  MUST_ASSIGN_LOOKUP_SYMBOL(n_param, m_node, "N");
  EXPECT_EQ(n_param_info.metatype, SymbolMetaType::kParameter);
  EXPECT_EQ(n_param_info.declared_type.user_defined_type,
            nullptr);  // types are primitive

  MUST_ASSIGN_LOOKUP_SYMBOL(p_param, m_node, "P");
  EXPECT_EQ(p_param_info.metatype, SymbolMetaType::kParameter);
  EXPECT_EQ(p_param_info.declared_type.user_defined_type,
            nullptr);  // types are primitive

  MUST_ASSIGN_LOOKUP_SYMBOL(rr_node, root_symbol, "rr");
  MUST_ASSIGN_LOOKUP_SYMBOL(m_inst_node, rr_node, "m_inst");

  const auto ref_map(rr_node_info.LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(m_type_ref, ref_map, "m");

  const ReferenceComponentNode &m_type_ref_root(*m_type_ref->components);
  ASSERT_EQ(m_type_ref_root.Children().size(), 2);
  const ReferenceComponentMap param_refs(
      ReferenceComponentNodeMapView(m_type_ref_root));

  ASSIGN_MUST_FIND(n_ref, param_refs, "N");
  const ReferenceComponent &n_ref_comp(n_ref->Value());
  EXPECT_EQ(n_ref_comp.identifier, "N");
  EXPECT_EQ(n_ref_comp.ref_type, ReferenceType::kDirectMember);
  EXPECT_EQ(n_ref_comp.required_metatype, SymbolMetaType::kParameter);
  EXPECT_EQ(n_ref_comp.resolved_symbol, nullptr);  // not yet resolved

  ASSIGN_MUST_FIND(q_ref, param_refs, "Q");
  const ReferenceComponent &q_ref_comp(q_ref->Value());
  EXPECT_EQ(q_ref_comp.identifier, "Q");
  EXPECT_EQ(q_ref_comp.ref_type, ReferenceType::kDirectMember);
  EXPECT_EQ(q_ref_comp.required_metatype, SymbolMetaType::kParameter);
  EXPECT_EQ(q_ref_comp.resolved_symbol, nullptr);  // not yet resolved

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);

    ASSIGN_MUST_HAVE_UNIQUE(err, resolve_diagnostics);
    EXPECT_EQ(err.code(), absl::StatusCode::kNotFound);

    // Expect only ".N" to resolve to formal parameters of "m".
    EXPECT_EQ(n_ref_comp.resolved_symbol, &n_param);
    EXPECT_EQ(q_ref_comp.resolved_symbol, nullptr);
  }
}

TEST(BuildSymbolTableTest, OneGlobalIntParameter) {
  TestVerilogSourceFile src("foobar.sv", "localparam int mint = 1;\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(mint_param, root_symbol, "mint");
  EXPECT_EQ(mint_param_info.metatype, SymbolMetaType::kParameter);
  EXPECT_EQ(mint_param_info.file_origin, &src);
  ASSERT_NE(mint_param_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(
      verible::StringSpanOfSymbol(*mint_param_info.declared_type.syntax_origin),
      "int");

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, OneGlobalUndefinedTypeParameter) {
  TestVerilogSourceFile src("foobar.sv", "localparam foo_t gun = 1;\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(gun_param, root_symbol, "gun");
  EXPECT_EQ(gun_param_info.metatype, SymbolMetaType::kParameter);
  EXPECT_EQ(gun_param_info.file_origin, &src);
  ASSERT_NE(gun_param_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(
      verible::StringSpanOfSymbol(*gun_param_info.declared_type.syntax_origin),
      "foo_t");

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve

    ASSIGN_MUST_HAVE_UNIQUE(err_status, resolve_diagnostics);
    EXPECT_EQ(err_status.code(), absl::StatusCode::kNotFound);
    EXPECT_THAT(err_status.message(),
                HasSubstr("Unable to resolve symbol \"foo_t\""));
    EXPECT_EQ(
        gun_param_info.declared_type.user_defined_type->Value().resolved_symbol,
        nullptr);  // not resolved
  }
}

TEST(BuildSymbolTableTest, ReferenceOneParameterExpression) {
  TestVerilogSourceFile src("foobar.sv",
                            "localparam int mint = 1;\n"
                            "localparam int tea = mint;\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(tea, root_symbol, "tea");
  EXPECT_EQ(tea_info.metatype, SymbolMetaType::kParameter);

  MUST_ASSIGN_LOOKUP_SYMBOL(mint, root_symbol, "mint");
  EXPECT_EQ(mint_info.metatype, SymbolMetaType::kParameter);
  EXPECT_EQ(mint_info.file_origin, &src);
  ASSERT_NE(mint_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(verible::StringSpanOfSymbol(*mint_info.declared_type.syntax_origin),
            "int");

  // There should be one reference: "mint" (line 2)
  EXPECT_EQ(root_symbol.Value().local_references_to_bind.size(), 1);
  const auto ref_map(root_symbol.Value().LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(ref, ref_map, "mint");
  const ReferenceComponent &ref_comp(ref->components->Value());
  EXPECT_TRUE(is_leaf(*ref->components));
  EXPECT_EQ(ref_comp.identifier, "mint");
  EXPECT_EQ(ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(ref_comp.resolved_symbol,
            nullptr);  // have not tried to resolve yet

  {  // resolve symbols
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    EXPECT_EQ(ref_comp.resolved_symbol, &mint);  // resolved
  }
}

TEST(BuildSymbolTableTest, OneUnresolvedReferenceInExpression) {
  TestVerilogSourceFile src("foobar.sv", "localparam int mint = spice;\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(mint, root_symbol, "mint");
  EXPECT_EQ(mint_info.metatype, SymbolMetaType::kParameter);
  EXPECT_EQ(mint_info.file_origin, &src);
  ASSERT_NE(mint_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(verible::StringSpanOfSymbol(*mint_info.declared_type.syntax_origin),
            "int");

  // There should be one reference: "spice" (line 2)
  EXPECT_EQ(root_symbol.Value().local_references_to_bind.size(), 1);
  const auto ref_map(root_symbol.Value().LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(ref, ref_map, "spice");
  const ReferenceComponent &ref_comp(ref->components->Value());
  EXPECT_TRUE(is_leaf(*ref->components));
  EXPECT_EQ(ref_comp.identifier, "spice");
  EXPECT_EQ(ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(ref_comp.resolved_symbol,
            nullptr);  // have not tried to resolve yet

  {  // resolve symbols
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);

    ASSIGN_MUST_HAVE_UNIQUE(err_status, resolve_diagnostics);
    EXPECT_EQ(err_status.code(), absl::StatusCode::kNotFound);
    EXPECT_THAT(err_status.message(),
                HasSubstr("Unable to resolve symbol \"spice\""));
    EXPECT_EQ(ref_comp.resolved_symbol, nullptr);  // still resolved
  }
}

TEST(BuildSymbolTableTest, PackageDeclarationSingle) {
  TestVerilogSourceFile src("foobar.sv", "package my_pkg;\nendpackage\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(my_pkg, root_symbol, "my_pkg");
  EXPECT_EQ(my_pkg_info.metatype, SymbolMetaType::kPackage);
  EXPECT_EQ(my_pkg_info.file_origin, &src);
  EXPECT_EQ(my_pkg_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, ReferenceOneParameterFromPackageToRoot) {
  TestVerilogSourceFile src("foobar.sv",
                            "localparam int mint = 1;\n"
                            "package p;\n"
                            "localparam int tea = mint;\n"  // reference
                            "endpackage\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(p_pkg, root_symbol, "p");
  EXPECT_EQ(p_pkg_info.metatype, SymbolMetaType::kPackage);

  ASSERT_EQ(p_pkg_info.local_references_to_bind.size(), 1);
  const auto ref_map(p_pkg_info.LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(ref, ref_map, "mint");
  const ReferenceComponent &mint_ref(ref->components->Value());
  EXPECT_EQ(mint_ref.identifier, "mint");
  EXPECT_EQ(mint_ref.resolved_symbol, nullptr);  // not yet resolved

  MUST_ASSIGN_LOOKUP_SYMBOL(tea, p_pkg, "tea");  // p::tea
  EXPECT_EQ(tea_info.metatype, SymbolMetaType::kParameter);

  MUST_ASSIGN_LOOKUP_SYMBOL(mint, root_symbol, "mint");
  EXPECT_EQ(mint_info.metatype, SymbolMetaType::kParameter);
  EXPECT_EQ(mint_info.file_origin, &src);
  ASSERT_NE(mint_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(verible::StringSpanOfSymbol(*mint_info.declared_type.syntax_origin),
            "int");

  {  // resolve symbols
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    EXPECT_EQ(mint_ref.resolved_symbol, &mint);  // resolved "mint"
  }
}

TEST(BuildSymbolTableTest, ReferenceOneParameterFromRootToPackage) {
  TestVerilogSourceFile src(
      "foobar.sv",
      "package p;\n"
      "localparam int mint = 1;\n"
      "endpackage\n"
      "localparam int tea = p::mint;\n"  // qualified reference
  );
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(p_pkg, root_symbol, "p");
  EXPECT_EQ(p_pkg_info.metatype, SymbolMetaType::kPackage);

  ASSERT_EQ(root_symbol.Value().local_references_to_bind.size(), 1);
  // p_mint_ref is the reference chain for "p::mint".
  const auto ref_map(root_symbol.Value().LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(p_mint_ref, ref_map, "p");
  const ReferenceComponent &p_ref(p_mint_ref->components->Value());
  EXPECT_EQ(p_ref.identifier, "p");
  EXPECT_EQ(p_ref.resolved_symbol, nullptr);  // not yet resolved
  const ReferenceComponent &mint_ref(p_mint_ref->LastLeaf()->Value());
  EXPECT_EQ(mint_ref.identifier, "mint");
  EXPECT_EQ(mint_ref.resolved_symbol, nullptr);  // not yet resolved

  MUST_ASSIGN_LOOKUP_SYMBOL(tea, root_symbol, "tea");
  EXPECT_EQ(tea_info.metatype, SymbolMetaType::kParameter);

  MUST_ASSIGN_LOOKUP_SYMBOL(mint, p_pkg, "mint");  // p::mint
  EXPECT_EQ(mint_info.metatype, SymbolMetaType::kParameter);
  EXPECT_EQ(mint_info.file_origin, &src);
  ASSERT_NE(mint_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(verible::StringSpanOfSymbol(*mint_info.declared_type.syntax_origin),
            "int");

  {  // resolve symbols
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    EXPECT_EQ(p_ref.resolved_symbol, &p_pkg);    // resolved "p"
    EXPECT_EQ(mint_ref.resolved_symbol, &mint);  // resolved "p::mint"
  }
}

TEST(BuildSymbolTableTest, ReferenceOneParameterFromRootToPackageNoSuchMember) {
  TestVerilogSourceFile src("foobar.sv",
                            "package p;\n"
                            "localparam int mint = 1;\n"
                            "endpackage\n"
                            "localparam int tea = p::zzz;\n"  // expect fail
  );
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(p_pkg, root_symbol, "p");
  EXPECT_EQ(p_pkg_info.metatype, SymbolMetaType::kPackage);

  ASSERT_EQ(root_symbol.Value().local_references_to_bind.size(), 1);
  // p_mint_ref is the reference chain for "p::mint".
  const auto ref_map(root_symbol.Value().LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(p_mint_ref, ref_map, "p");
  const ReferenceComponent &p_ref(p_mint_ref->components->Value());
  EXPECT_EQ(p_ref.identifier, "p");
  EXPECT_EQ(p_ref.resolved_symbol, nullptr);  // not yet resolved
  const ReferenceComponent &zzz_ref(p_mint_ref->LastLeaf()->Value());
  EXPECT_EQ(zzz_ref.identifier, "zzz");
  EXPECT_EQ(zzz_ref.resolved_symbol, nullptr);  // not yet resolved

  MUST_ASSIGN_LOOKUP_SYMBOL(tea, root_symbol, "tea");
  EXPECT_EQ(tea_info.metatype, SymbolMetaType::kParameter);

  MUST_ASSIGN_LOOKUP_SYMBOL(mint, p_pkg, "mint");
  EXPECT_EQ(mint_info.metatype, SymbolMetaType::kParameter);
  EXPECT_EQ(mint_info.file_origin, &src);
  ASSERT_NE(mint_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(verible::StringSpanOfSymbol(*mint_info.declared_type.syntax_origin),
            "int");

  // resolving twice should not change results
  for (int i = 0; i < 2; ++i) {  // resolve symbols
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);

    ASSIGN_MUST_HAVE_UNIQUE(err_status, resolve_diagnostics);
    EXPECT_EQ(err_status.code(), absl::StatusCode::kNotFound);
    EXPECT_EQ(p_ref.resolved_symbol, &p_pkg);     // resolved "p"
    EXPECT_EQ(zzz_ref.resolved_symbol, nullptr);  // unresolved "p::zzz"
  }
}

TEST(BuildSymbolTableTest, ModuleDeclarationWithParameters) {
  TestVerilogSourceFile src("foobar.sv",
                            "module m #(\n"
                            "  int W = 2,\n"
                            "  bar B = W\n"
                            ");\n"
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(module_node, root_symbol, "m");
  EXPECT_EQ(module_node_info.metatype, SymbolMetaType::kModule);
  EXPECT_EQ(module_node_info.file_origin, &src);
  EXPECT_EQ(module_node_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type

  MUST_ASSIGN_LOOKUP_SYMBOL(w_param, module_node, "W");
  EXPECT_EQ(w_param_info.metatype, SymbolMetaType::kParameter);
  const ReferenceComponentNode *w_type_ref =
      w_param_info.declared_type.user_defined_type;
  EXPECT_EQ(w_type_ref, nullptr);  // int is primitive type

  MUST_ASSIGN_LOOKUP_SYMBOL(b_param, module_node, "B");
  EXPECT_EQ(b_param_info.metatype, SymbolMetaType::kParameter);
  const ReferenceComponentNode *b_type_ref =
      b_param_info.declared_type.user_defined_type;
  ASSERT_NE(b_type_ref, nullptr);
  const ReferenceComponent &b_type_ref_comp(b_type_ref->Value());
  EXPECT_EQ(b_type_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(b_type_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(b_type_ref_comp.identifier, "bar");

  ASSERT_EQ(module_node_info.local_references_to_bind.size(), 2);
  const auto ref_map(module_node_info.LocalReferencesMapViewForTesting());

  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(w_ref, ref_map, "W");
  const ReferenceComponent &w_ref_comp(w_ref->components->Value());
  EXPECT_EQ(w_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(w_ref_comp.identifier, "W");
  EXPECT_EQ(w_ref_comp.resolved_symbol, nullptr);  // not yet resolved

  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(bar_ref, ref_map, "bar");
  const ReferenceComponent &bar_ref_comp(bar_ref->components->Value());
  EXPECT_EQ(bar_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(bar_ref_comp.identifier, "bar");
  EXPECT_EQ(bar_ref_comp.resolved_symbol, nullptr);  // not yet resolved

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);

    ASSIGN_MUST_HAVE_UNIQUE(error, resolve_diagnostics);
    // type reference 'bar' is unresolved
    EXPECT_EQ(error.code(), absl::StatusCode::kNotFound);
    EXPECT_THAT(error.message(), HasSubstr("Unable to resolve symbol \"bar\""));

    EXPECT_EQ(w_ref_comp.resolved_symbol, &w_param);   // resolved successfully
    EXPECT_EQ(bar_ref_comp.resolved_symbol, nullptr);  // failed to resolve
  }
}

TEST(BuildSymbolTableTest, ModuleDeclarationLocalsDependOnParameter) {
  TestVerilogSourceFile src("foobar.sv",
                            "module m #(\n"
                            "  parameter int N = 2\n"
                            ") (\n"
                            "  input logic [N-1:0] ins,\n"  // ref
                            "  output reg [0:N-1] outs\n"   // ref
                            ");\n"
                            "  wire [N][N] arr[N][N];\n"  // 4 refs
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(module_m, root_symbol, "m");
  EXPECT_EQ(module_m_info.metatype, SymbolMetaType::kModule);
  EXPECT_EQ(module_m_info.file_origin, &src);
  EXPECT_EQ(module_m_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type

  MUST_ASSIGN_LOOKUP_SYMBOL(n_param, module_m, "N");
  EXPECT_EQ(n_param_info.metatype, SymbolMetaType::kParameter);
  const ReferenceComponentNode *n_type_ref =
      n_param_info.declared_type.user_defined_type;
  EXPECT_EQ(n_type_ref, nullptr);  // int is primitive type

  EXPECT_EQ(module_m_info.local_references_to_bind.size(), 6);
  const auto ref_map(module_m_info.LocalReferencesMapViewForTesting());

  ASSIGN_MUST_FIND(n_refs, ref_map, "N");
  ASSERT_EQ(n_refs.size(), 6);  // all references to "N" parameter
  for (const auto &n_ref : n_refs) {
    const ReferenceComponent &n_ref_comp(n_ref->components->Value());
    EXPECT_EQ(n_ref_comp.ref_type, ReferenceType::kUnqualified);
    EXPECT_EQ(n_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
    EXPECT_EQ(n_ref_comp.identifier, "N");
    EXPECT_EQ(n_ref_comp.resolved_symbol, nullptr);  // not yet resolved
  }

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // All references to "N" resolved.
    for (const auto &n_ref : n_refs) {
      const ReferenceComponent &n_ref_comp(n_ref->components->Value());
      EXPECT_EQ(n_ref_comp.resolved_symbol, &n_param);  // resolved successfully
    }
  }
}

TEST(BuildSymbolTableTest, ModuleSingleImplicitDeclaration) {
  TestVerilogSourceFile src("foo.sv",
                            "module m;"
                            "assign a = 1'b0;"
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(module_m, root_symbol, "m");
  EXPECT_EQ(module_m_info.metatype, SymbolMetaType::kModule);
  EXPECT_EQ(module_m_info.file_origin, &src);
  EXPECT_EQ(module_m_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type

  MUST_ASSIGN_LOOKUP_SYMBOL(a_variable, module_m, "a");
  EXPECT_EQ(a_variable_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  const ReferenceComponentNode *a_type_ref =
      a_variable_info.declared_type.user_defined_type;
  EXPECT_EQ(a_type_ref, nullptr);  // implicit type is primitive type
  EXPECT_TRUE(a_variable_info.declared_type.implicit);

  EXPECT_EQ(module_m_info.local_references_to_bind.size(), 1);
  const auto ref_map(module_m_info.LocalReferencesMapViewForTesting());

  ASSIGN_MUST_FIND(a_refs, ref_map, "a");
  ASSERT_EQ(a_refs.size(), 1);  // all references to "a" parameter
  for (const auto &a_ref : a_refs) {
    const ReferenceComponent &a_ref_comp(a_ref->components->Value());
    EXPECT_EQ(a_ref_comp.ref_type, ReferenceType::kUnqualified);
    EXPECT_EQ(a_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
    EXPECT_EQ(a_ref_comp.identifier, "a");
    EXPECT_EQ(a_ref_comp.resolved_symbol, &a_variable);  // pre-resolved
  }

  {
    std::vector<absl::Status> resolve_diagnostics;
    // Resolve mustn't break anything
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // All references to "a" resolved.
    for (const auto &a_ref : a_refs) {
      const ReferenceComponent &a_ref_comp(a_ref->components->Value());
      EXPECT_EQ(a_ref_comp.resolved_symbol,
                &a_variable);  // resolved successfully
    }
  }
}

TEST(BuildSymbolTableTest, ModuleReferenceToImplicitDeclaration) {
  TestVerilogSourceFile src("foo.sv",
                            "module m;"
                            "assign a = 1'b0;"
                            "assign a = 1'b1;"
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(module_m, root_symbol, "m");
  EXPECT_EQ(module_m_info.metatype, SymbolMetaType::kModule);
  EXPECT_EQ(module_m_info.file_origin, &src);
  EXPECT_EQ(module_m_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type

  MUST_ASSIGN_LOOKUP_SYMBOL(a_variable, module_m, "a");
  EXPECT_EQ(a_variable_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  const ReferenceComponentNode *a_type_ref =
      a_variable_info.declared_type.user_defined_type;
  EXPECT_EQ(a_type_ref, nullptr);  // implicit type is primitive type
  EXPECT_TRUE(a_variable_info.declared_type.implicit);

  EXPECT_EQ(module_m_info.local_references_to_bind.size(), 2);
  const auto ref_map(module_m_info.LocalReferencesMapViewForTesting());

  ASSIGN_MUST_FIND(a_refs, ref_map, "a");
  ASSERT_EQ(a_refs.size(), 2);  // all references to "a" parameter
  {
    const auto &a_ref = *a_refs.begin();
    const ReferenceComponent &a_ref_comp(a_ref->components->Value());
    EXPECT_EQ(a_ref_comp.ref_type, ReferenceType::kUnqualified);
    EXPECT_EQ(a_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
    EXPECT_EQ(a_ref_comp.identifier, "a");
    EXPECT_EQ(a_ref_comp.resolved_symbol, &a_variable);  // pre-resolved
  }
  {
    const auto &a_ref = *std::next(a_refs.begin());
    const ReferenceComponent &a_ref_comp(a_ref->components->Value());
    EXPECT_EQ(a_ref_comp.ref_type, ReferenceType::kUnqualified);
    EXPECT_EQ(a_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
    EXPECT_EQ(a_ref_comp.identifier, "a");
    EXPECT_EQ(a_ref_comp.resolved_symbol, nullptr);  // pre-resolved
  }

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // All references to "a" resolved.
    for (const auto &a_ref : a_refs) {
      const ReferenceComponent &a_ref_comp(a_ref->components->Value());
      EXPECT_EQ(a_ref_comp.resolved_symbol,
                &a_variable);  // resolved successfully
    }
  }
}

TEST(BuildSymbolTableTest, ModuleReferenceToImplicitDeclarationInSubScope) {
  TestVerilogSourceFile src("foo.sv",
                            "module m;"
                            " assign a = 1'b0;"
                            " module n;"
                            "  assign a = 1'b1;"
                            " endmodule;"
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(module_m, root_symbol, "m");
  EXPECT_EQ(module_m_info.metatype, SymbolMetaType::kModule);
  EXPECT_EQ(module_m_info.file_origin, &src);
  EXPECT_EQ(module_m_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type

  MUST_ASSIGN_LOOKUP_SYMBOL(a_variable, module_m, "a");
  EXPECT_EQ(a_variable_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  const ReferenceComponentNode *a_type_ref =
      a_variable_info.declared_type.user_defined_type;
  EXPECT_EQ(a_type_ref, nullptr);  // implicit type is primitive type
  EXPECT_TRUE(a_variable_info.declared_type.implicit);

  EXPECT_EQ(module_m_info.local_references_to_bind.size(), 1);
  const auto ref_map(module_m_info.LocalReferencesMapViewForTesting());

  ASSIGN_MUST_FIND(a_refs, ref_map, "a");
  ASSERT_EQ(a_refs.size(), 1);  // all references to "a" parameter
  for (const auto &a_ref : a_refs) {
    const ReferenceComponent &a_ref_comp(a_ref->components->Value());
    EXPECT_EQ(a_ref_comp.ref_type, ReferenceType::kUnqualified);
    EXPECT_EQ(a_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
    EXPECT_EQ(a_ref_comp.identifier, "a");
    EXPECT_EQ(a_ref_comp.resolved_symbol, &a_variable);  // pre-resolved
  }

  // Submodule "n"
  MUST_ASSIGN_LOOKUP_SYMBOL(module_n, module_m, "n");
  EXPECT_EQ(module_n_info.metatype, SymbolMetaType::kModule);
  EXPECT_EQ(module_n_info.file_origin, &src);
  EXPECT_EQ(module_n_info.declared_type.syntax_origin, nullptr);

  EXPECT_EQ(module_n_info.local_references_to_bind.size(), 1);
  const auto n_ref_map(module_n_info.LocalReferencesMapViewForTesting());

  ASSIGN_MUST_FIND(n_a_refs, n_ref_map, "a");
  ASSERT_EQ(n_a_refs.size(), 1);  // references to "a" net in "n" module
  for (const auto &n_a_ref : n_a_refs) {
    const ReferenceComponent &n_a_ref_comp(n_a_ref->components->Value());
    EXPECT_EQ(n_a_ref_comp.ref_type, ReferenceType::kUnqualified);
    EXPECT_EQ(n_a_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
    EXPECT_EQ(n_a_ref_comp.identifier, "a");
    EXPECT_EQ(n_a_ref_comp.resolved_symbol,
              nullptr);  // resolving only in same scope
  }

  {
    std::vector<absl::Status> resolve_diagnostics;
    // Resolve mustn't break anything
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // All references to "a" resolved.
    for (const auto &a_ref : a_refs) {
      const ReferenceComponent &a_ref_comp(a_ref->components->Value());
      EXPECT_EQ(a_ref_comp.resolved_symbol,
                &a_variable);  // resolved successfully
    }

    for (const auto &n_a_ref : n_a_refs) {
      const ReferenceComponent &n_a_ref_comp(n_a_ref->components->Value());
      EXPECT_EQ(n_a_ref_comp.resolved_symbol,
                &a_variable);  // resolved successfully
    }
  }
}

TEST(BuildSymbolTableTest, ModuleExplicitDeclarationInSubScope) {
  TestVerilogSourceFile src("foo.sv",
                            "module m;"
                            " assign a = 1'b0;"
                            " module n;"
                            "  wire a;"
                            "  assign a = 1'b1;"
                            " endmodule;"
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(module_m, root_symbol, "m");
  EXPECT_EQ(module_m_info.metatype, SymbolMetaType::kModule);
  EXPECT_EQ(module_m_info.file_origin, &src);
  EXPECT_EQ(module_m_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type

  MUST_ASSIGN_LOOKUP_SYMBOL(a_variable, module_m, "a");
  EXPECT_EQ(a_variable_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  const ReferenceComponentNode *a_type_ref =
      a_variable_info.declared_type.user_defined_type;
  EXPECT_EQ(a_type_ref, nullptr);  // implicit type is primitive type
  EXPECT_TRUE(a_variable_info.declared_type.implicit);

  EXPECT_EQ(module_m_info.local_references_to_bind.size(), 1);
  const auto ref_map(module_m_info.LocalReferencesMapViewForTesting());

  ASSIGN_MUST_FIND(a_refs, ref_map, "a");
  ASSERT_EQ(a_refs.size(), 1);  // all references to "a" parameter
  for (const auto &a_ref : a_refs) {
    const ReferenceComponent &a_ref_comp(a_ref->components->Value());
    EXPECT_EQ(a_ref_comp.ref_type, ReferenceType::kUnqualified);
    EXPECT_EQ(a_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
    EXPECT_EQ(a_ref_comp.identifier, "a");
    EXPECT_EQ(a_ref_comp.resolved_symbol, &a_variable);  // pre-resolved
  }

  // Submodule "n"
  MUST_ASSIGN_LOOKUP_SYMBOL(module_n, module_m, "n");
  EXPECT_EQ(module_n_info.metatype, SymbolMetaType::kModule);
  EXPECT_EQ(module_n_info.file_origin, &src);
  EXPECT_EQ(module_n_info.declared_type.syntax_origin, nullptr);

  MUST_ASSIGN_LOOKUP_SYMBOL(n_a_variable, module_n, "a");
  EXPECT_EQ(n_a_variable_info.metatype,
            SymbolMetaType::kDataNetVariableInstance);
  const ReferenceComponentNode *n_a_type_ref =
      n_a_variable_info.declared_type.user_defined_type;
  EXPECT_EQ(n_a_type_ref, nullptr);
  EXPECT_FALSE(n_a_variable_info.declared_type.implicit);

  EXPECT_EQ(module_n_info.local_references_to_bind.size(), 1);
  const auto n_ref_map(module_n_info.LocalReferencesMapViewForTesting());

  ASSIGN_MUST_FIND(n_a_refs, n_ref_map, "a");
  ASSERT_EQ(n_a_refs.size(), 1);  // references to "a" net in "n" module
  for (const auto &n_a_ref : n_a_refs) {
    const ReferenceComponent &n_a_ref_comp(n_a_ref->components->Value());
    EXPECT_EQ(n_a_ref_comp.ref_type, ReferenceType::kUnqualified);
    EXPECT_EQ(n_a_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
    EXPECT_EQ(n_a_ref_comp.identifier, "a");
    EXPECT_EQ(n_a_ref_comp.resolved_symbol,
              nullptr);  // resolving only in same scope
  }

  {
    std::vector<absl::Status> resolve_diagnostics;
    // Resolve mustn't break anything
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // All references to "a" resolved.
    for (const auto &a_ref : a_refs) {
      const ReferenceComponent &a_ref_comp(a_ref->components->Value());
      EXPECT_EQ(a_ref_comp.resolved_symbol,
                &a_variable);  // resolved successfully
    }

    for (const auto &n_a_ref : n_a_refs) {
      const ReferenceComponent &n_a_ref_comp(n_a_ref->components->Value());
      EXPECT_EQ(n_a_ref_comp.resolved_symbol,
                &n_a_variable);  // resolved successfully
    }
  }
}

TEST(BuildSymbolTableTest, ModuleExplicitAndImplicitDeclarations) {
  TestVerilogSourceFile src("foo.sv",
                            "module m;"
                            "wire b;"
                            "assign a = 1'b0;"
                            "assign b = 1'b1;"
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(module_m, root_symbol, "m");
  EXPECT_EQ(module_m_info.metatype, SymbolMetaType::kModule);
  EXPECT_EQ(module_m_info.file_origin, &src);
  EXPECT_EQ(module_m_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type

  MUST_ASSIGN_LOOKUP_SYMBOL(a_variable, module_m, "a");
  EXPECT_EQ(a_variable_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  const ReferenceComponentNode *a_type_ref =
      a_variable_info.declared_type.user_defined_type;
  EXPECT_EQ(a_type_ref, nullptr);  // implicit type is primitive type
  EXPECT_TRUE(a_variable_info.declared_type.implicit);

  MUST_ASSIGN_LOOKUP_SYMBOL(b_variable, module_m, "b");
  EXPECT_EQ(b_variable_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  const ReferenceComponentNode *b_type_ref =
      b_variable_info.declared_type.user_defined_type;
  EXPECT_EQ(b_type_ref, nullptr);
  EXPECT_FALSE(b_variable_info.declared_type.implicit);

  EXPECT_EQ(module_m_info.local_references_to_bind.size(), 2);
  const auto ref_map(module_m_info.LocalReferencesMapViewForTesting());

  ASSIGN_MUST_FIND(a_refs, ref_map, "a");
  ASSERT_EQ(a_refs.size(), 1);  // all references to "a" parameter
  for (const auto &a_ref : a_refs) {
    const ReferenceComponent &a_ref_comp(a_ref->components->Value());
    EXPECT_EQ(a_ref_comp.ref_type, ReferenceType::kUnqualified);
    EXPECT_EQ(a_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
    EXPECT_EQ(a_ref_comp.identifier, "a");
    EXPECT_EQ(a_ref_comp.resolved_symbol, &a_variable);  // pre-resolved
  }

  ASSIGN_MUST_FIND(b_refs, ref_map, "b");
  ASSERT_EQ(b_refs.size(), 1);
  for (const auto &b_ref : b_refs) {
    const ReferenceComponent &b_ref_comp(b_ref->components->Value());
    EXPECT_EQ(b_ref_comp.ref_type, ReferenceType::kUnqualified);
    EXPECT_EQ(b_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
    EXPECT_EQ(b_ref_comp.identifier, "b");
    EXPECT_EQ(b_ref_comp.resolved_symbol, nullptr);
  }

  {
    std::vector<absl::Status> resolve_diagnostics;
    // Resolve mustn't break anything
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // All references to "a" resolved.
    for (const auto &a_ref : a_refs) {
      const ReferenceComponent &a_ref_comp(a_ref->components->Value());
      EXPECT_EQ(a_ref_comp.resolved_symbol,
                &a_variable);  // resolved successfully
    }

    // All references to "b" resolved.
    for (const auto &b_ref : b_refs) {
      const ReferenceComponent &b_ref_comp(b_ref->components->Value());
      EXPECT_EQ(b_ref_comp.resolved_symbol,
                &b_variable);  // resolved successfully
    }
  }
}

TEST(BuildSymbolTableTest, ModuleImplicitRedeclared) {
  TestVerilogSourceFile src("foo.sv",
                            "module m;\n"
                            "assign a = 1'b0;\n"
                            "wire a;\n"
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EQ(build_diagnostics.size(), 1);
  EXPECT_FALSE(build_diagnostics.front().ok());
  EXPECT_EQ(build_diagnostics.front().message(),
            "foo.sv:3:6: Symbol \"a\" is already defined in the $root::m scope "
            "at 2:8:");
}

TEST(BuildSymbolTableTest, ClassDeclarationSingle) {
  TestVerilogSourceFile src("foobar.sv", "class ccc;\nendclass\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(ccc, root_symbol, "ccc");
  EXPECT_EQ(ccc_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(ccc_info.file_origin, &src);
  EXPECT_EQ(ccc_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, ClassDeclarationNested) {
  TestVerilogSourceFile src("foobar.sv",
                            "package pp;\n"
                            "  class c_outer;\n"
                            "    class c_inner;\n"
                            "    endclass\n"
                            "  endclass\n"
                            "endpackage\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(pp, root_symbol, "pp");
  EXPECT_EQ(pp_info.metatype, SymbolMetaType::kPackage);
  EXPECT_EQ(pp_info.file_origin, &src);
  EXPECT_EQ(pp_info.declared_type.syntax_origin,
            nullptr);  // there is no package meta-type
  {
    MUST_ASSIGN_LOOKUP_SYMBOL(c_outer, pp, "c_outer");
    EXPECT_EQ(c_outer_info.metatype, SymbolMetaType::kClass);
    EXPECT_EQ(c_outer_info.file_origin, &src);
    EXPECT_EQ(c_outer_info.declared_type.syntax_origin,
              nullptr);  // there is no class meta-type
    {
      MUST_ASSIGN_LOOKUP_SYMBOL(c_inner, c_outer, "c_inner");
      EXPECT_EQ(c_inner_info.metatype, SymbolMetaType::kClass);
      EXPECT_EQ(c_inner_info.file_origin, &src);
      EXPECT_EQ(c_inner_info.declared_type.syntax_origin,
                nullptr);  // there is no class meta-type
    }
  }
  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, ClassDeclarationWithParameter) {
  TestVerilogSourceFile src("foobar.sv",
                            "class cc #(\n"
                            "  int N = 2\n"
                            ");\n"
                            "endclass\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(class_cc, root_symbol, "cc");
  EXPECT_EQ(class_cc_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(class_cc_info.file_origin, &src);
  EXPECT_EQ(class_cc_info.declared_type.syntax_origin,
            nullptr);  // there is no class meta-type

  MUST_ASSIGN_LOOKUP_SYMBOL(n_param, class_cc, "N");
  EXPECT_EQ(n_param_info.metatype, SymbolMetaType::kParameter);
  const ReferenceComponentNode *n_type_ref =
      n_param_info.declared_type.user_defined_type;
  EXPECT_EQ(n_type_ref, nullptr);  // int is primitive type

  EXPECT_TRUE(class_cc_info.local_references_to_bind.empty());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, ClassDeclarationDataMember) {
  TestVerilogSourceFile src("member_accessor.sv",
                            "class cc;\n"
                            "  int size;\n"
                            "  int count = 0;\n"  // with initializer
                            "endclass\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(class_cc, root_symbol, "cc");
  EXPECT_EQ(class_cc_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(class_cc_info.file_origin, &src);
  EXPECT_EQ(class_cc_info.declared_type.syntax_origin, nullptr);

  MUST_ASSIGN_LOOKUP_SYMBOL(size_field, class_cc, "size");
  EXPECT_EQ(size_field_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  const ReferenceComponentNode *size_type_ref =
      size_field_info.declared_type.user_defined_type;
  EXPECT_EQ(size_type_ref, nullptr);  // int is primitive type
  EXPECT_EQ(
      verible::StringSpanOfSymbol(*size_field_info.declared_type.syntax_origin),
      "int");

  MUST_ASSIGN_LOOKUP_SYMBOL(count_field, class_cc, "count");
  EXPECT_EQ(count_field_info.metatype,
            SymbolMetaType::kDataNetVariableInstance);
  const ReferenceComponentNode *count_type_ref =
      count_field_info.declared_type.user_defined_type;
  EXPECT_EQ(count_type_ref, nullptr);  // int is primitive type
  EXPECT_EQ(verible::StringSpanOfSymbol(
                *count_field_info.declared_type.syntax_origin),
            "int");

  EXPECT_TRUE(class_cc_info.local_references_to_bind.empty());

  {  // No references.
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, ClassDeclarationDataMemberMultiDeclaration) {
  TestVerilogSourceFile src("member_accessor.sv",
                            "class cc;\n"
                            "  real height, width;\n"
                            "endclass\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(class_cc, root_symbol, "cc");
  EXPECT_EQ(class_cc_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(class_cc_info.file_origin, &src);
  EXPECT_EQ(class_cc_info.declared_type.syntax_origin, nullptr);

  MUST_ASSIGN_LOOKUP_SYMBOL(height_field, class_cc, "height");
  EXPECT_EQ(height_field_info.metatype,
            SymbolMetaType::kDataNetVariableInstance);
  const ReferenceComponentNode *height_type_ref =
      height_field_info.declared_type.user_defined_type;
  EXPECT_EQ(height_type_ref, nullptr);  // int is primitive type
  EXPECT_EQ(verible::StringSpanOfSymbol(
                *height_field_info.declared_type.syntax_origin),
            "real");

  MUST_ASSIGN_LOOKUP_SYMBOL(width_field, class_cc, "width");
  EXPECT_EQ(width_field_info.metatype,
            SymbolMetaType::kDataNetVariableInstance);
  const ReferenceComponentNode *width_type_ref =
      width_field_info.declared_type.user_defined_type;
  EXPECT_EQ(width_type_ref, nullptr);  // int is primitive type
  EXPECT_EQ(verible::StringSpanOfSymbol(
                *width_field_info.declared_type.syntax_origin),
            "real");

  EXPECT_TRUE(class_cc_info.local_references_to_bind.empty());

  {  // No references.
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, ClassDeclarationDataMemberAccessedFromMethod) {
  TestVerilogSourceFile src("member_accessor.sv",
                            "class cc;\n"
                            "  int size;\n"
                            "  function int get_size();\n"
                            "    return size;\n"
                            "  endfunction\n"
                            "endclass\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(class_cc, root_symbol, "cc");
  EXPECT_EQ(class_cc_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(class_cc_info.file_origin, &src);
  EXPECT_EQ(class_cc_info.declared_type.syntax_origin, nullptr);

  MUST_ASSIGN_LOOKUP_SYMBOL(size_field, class_cc, "size");
  EXPECT_EQ(size_field_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  const ReferenceComponentNode *size_type_ref =
      size_field_info.declared_type.user_defined_type;
  EXPECT_EQ(size_type_ref, nullptr);  // int is primitive type
  EXPECT_EQ(
      verible::StringSpanOfSymbol(*size_field_info.declared_type.syntax_origin),
      "int");

  MUST_ASSIGN_LOOKUP_SYMBOL(get_size, class_cc, "get_size");
  EXPECT_EQ(get_size_info.metatype, SymbolMetaType::kFunction);
  EXPECT_EQ(get_size_info.file_origin, &src);
  const auto ref_map(get_size_info.LocalReferencesMapViewForTesting());

  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(size_ref, ref_map, "size");
  const ReferenceComponent &size_ref_comp(size_ref->components->Value());
  EXPECT_EQ(size_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(size_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(size_ref_comp.resolved_symbol, nullptr);

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // "size" resolved to class data member
    EXPECT_EQ(size_ref_comp.resolved_symbol, &size_field);
  }
}

TEST(BuildSymbolTableTest, ClassDataMemberAccessedDirectly) {
  TestVerilogSourceFile src("member_accessor.sv",
                            "class cc;\n"
                            "  int size;\n"
                            "endclass\n"
                            "function int get_size();\n"
                            "  cc cc_data;\n"
                            "  return cc_data.size;\n"
                            "endfunction\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(class_cc, root_symbol, "cc");
  EXPECT_EQ(class_cc_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(class_cc_info.file_origin, &src);
  EXPECT_EQ(class_cc_info.declared_type.syntax_origin, nullptr);

  MUST_ASSIGN_LOOKUP_SYMBOL(size_field, class_cc, "size");
  EXPECT_EQ(size_field_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  const ReferenceComponentNode *size_type_ref =
      size_field_info.declared_type.user_defined_type;
  EXPECT_EQ(size_type_ref, nullptr);  // int is primitive type
  EXPECT_EQ(
      verible::StringSpanOfSymbol(*size_field_info.declared_type.syntax_origin),
      "int");

  MUST_ASSIGN_LOOKUP_SYMBOL(get_size, root_symbol, "get_size");
  EXPECT_EQ(get_size_info.metatype, SymbolMetaType::kFunction);
  EXPECT_EQ(get_size_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(cc_data, get_size, "cc_data");
  EXPECT_EQ(cc_data_info.metatype, SymbolMetaType::kDataNetVariableInstance);

  const auto ref_map(get_size_info.LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(cc_data_ref, ref_map, "cc_data");
  const ReferenceComponent &cc_data_ref_comp(cc_data_ref->components->Value());
  EXPECT_EQ(cc_data_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(cc_data_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(cc_data_ref_comp.resolved_symbol, nullptr);

  ASSERT_EQ(cc_data_ref->components->Children().size(), 1);
  const ReferenceComponentNode &size_ref(
      cc_data_ref->components->Children().front());
  const ReferenceComponent &size_ref_comp(size_ref.Value());
  EXPECT_EQ(size_ref_comp.ref_type, ReferenceType::kMemberOfTypeOfParent);
  EXPECT_EQ(size_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(size_ref_comp.resolved_symbol, nullptr);

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // "size" resolved to class data member
    EXPECT_EQ(size_ref_comp.resolved_symbol, &size_field);
  }
}

TEST(BuildSymbolTableTest, ClassDeclarationSingleInheritance) {
  TestVerilogSourceFile src("member_accessor.sv",
                            "class base;\n"
                            "endclass\n"
                            "class derived extends base;\n"
                            "endclass\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(base_class, root_symbol, "base");
  EXPECT_EQ(base_class_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(base_class_info.file_origin, &src);
  EXPECT_EQ(base_class_info.declared_type.syntax_origin, nullptr);
  EXPECT_TRUE(base_class_info.local_references_to_bind.empty());

  MUST_ASSIGN_LOOKUP_SYMBOL(derived_class, root_symbol, "derived");
  EXPECT_EQ(derived_class_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(derived_class_info.file_origin, &src);
  EXPECT_EQ(derived_class_info.declared_type.syntax_origin, nullptr);
  EXPECT_TRUE(derived_class_info.local_references_to_bind.empty());

  // "base" is referenced from the scope that contains "derived"
  EXPECT_EQ(root_symbol.Value().local_references_to_bind.size(), 1);
  const auto ref_map(root_symbol.Value().LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(base_ref, ref_map, "base");
  const ReferenceComponent &base_ref_comp(base_ref->components->Value());
  EXPECT_EQ(base_ref_comp.identifier, "base");
  EXPECT_EQ(base_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(base_ref_comp.required_metatype, SymbolMetaType::kClass);
  EXPECT_EQ(base_ref_comp.resolved_symbol, nullptr);

  // Make sure the "base" reference is linked from the "derived" class.
  ASSERT_EQ(
      derived_class_info.parent_type.user_defined_type,
      root_symbol.Value().local_references_to_bind.front().LastTypeComponent());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // Resolve the "base" type reference to the "base" class.
    EXPECT_EQ(derived_class_info.parent_type.user_defined_type->Value()
                  .resolved_symbol,
              &base_class);
  }
}

TEST(BuildSymbolTableTest, ClassDeclarationSingleInheritanceAcrossPackage) {
  TestVerilogSourceFile src("member_accessor.sv",
                            "package pp;\n"
                            "  class base;\n"
                            "  endclass\n"
                            "endpackage\n"
                            "class derived extends pp::base;\n"
                            "endclass\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(package_pp, root_symbol, "pp");
  EXPECT_EQ(package_pp_info.metatype, SymbolMetaType::kPackage);

  MUST_ASSIGN_LOOKUP_SYMBOL(base_class, package_pp, "base");
  EXPECT_EQ(base_class_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(base_class_info.file_origin, &src);
  EXPECT_EQ(base_class_info.declared_type.syntax_origin, nullptr);
  EXPECT_TRUE(base_class_info.local_references_to_bind.empty());

  MUST_ASSIGN_LOOKUP_SYMBOL(derived_class, root_symbol, "derived");
  EXPECT_EQ(derived_class_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(derived_class_info.file_origin, &src);
  EXPECT_EQ(derived_class_info.declared_type.syntax_origin, nullptr);
  EXPECT_TRUE(derived_class_info.local_references_to_bind.empty());

  // "pp::base" is referenced from the scope that contains "derived"
  EXPECT_EQ(root_symbol.Value().local_references_to_bind.size(), 1);
  const auto ref_map(root_symbol.Value().LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(pp_ref, ref_map, "pp");
  const ReferenceComponent &pp_ref_comp(pp_ref->components->Value());
  EXPECT_EQ(pp_ref_comp.identifier, "pp");
  EXPECT_EQ(pp_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(pp_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(pp_ref_comp.resolved_symbol, nullptr);

  ASSERT_EQ(pp_ref->components->Children().size(), 1);
  const ReferenceComponentNode &base_ref(
      pp_ref->components->Children().front());
  const ReferenceComponent &base_ref_comp(base_ref.Value());
  EXPECT_EQ(base_ref_comp.identifier, "base");
  EXPECT_EQ(base_ref_comp.ref_type, ReferenceType::kDirectMember);
  EXPECT_EQ(base_ref_comp.required_metatype, SymbolMetaType::kClass);
  EXPECT_EQ(base_ref_comp.resolved_symbol, nullptr);

  // Make sure the "pp::base" reference is linked from the "derived" class.
  ASSERT_EQ(
      derived_class_info.parent_type.user_defined_type,
      root_symbol.Value().local_references_to_bind.front().LastTypeComponent());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // Resolve the "pp::base" type reference to the "pp::base" class.
    EXPECT_EQ(pp_ref_comp.resolved_symbol, &package_pp);
    EXPECT_EQ(base_ref_comp.resolved_symbol, &base_class);
    EXPECT_EQ(derived_class_info.parent_type.user_defined_type->Value()
                  .resolved_symbol,
              &base_class);
  }
}

TEST(BuildSymbolTableTest, ClassDeclarationSingleInheritancePackageToPackage) {
  TestVerilogSourceFile src("member_accessor.sv",
                            "package pp;\n"
                            "  class base;\n"
                            "  endclass\n"
                            "endpackage\n"
                            "package qq;\n"
                            "  class derived extends pp::base;\n"
                            "  endclass\n"
                            "endpackage\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(package_pp, root_symbol, "pp");
  EXPECT_EQ(package_pp_info.metatype, SymbolMetaType::kPackage);

  MUST_ASSIGN_LOOKUP_SYMBOL(base_class, package_pp, "base");
  EXPECT_EQ(base_class_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(base_class_info.file_origin, &src);
  EXPECT_EQ(base_class_info.declared_type.syntax_origin, nullptr);
  EXPECT_TRUE(base_class_info.local_references_to_bind.empty());

  MUST_ASSIGN_LOOKUP_SYMBOL(package_qq, root_symbol, "qq");
  EXPECT_EQ(package_qq_info.metatype, SymbolMetaType::kPackage);

  MUST_ASSIGN_LOOKUP_SYMBOL(derived_class, package_qq, "derived");
  EXPECT_EQ(derived_class_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(derived_class_info.file_origin, &src);
  EXPECT_EQ(derived_class_info.declared_type.syntax_origin, nullptr);
  EXPECT_TRUE(derived_class_info.local_references_to_bind.empty());

  // "pp::base" is referenced from the scope that contains "derived",
  // which is package "qq".
  EXPECT_EQ(package_qq_info.local_references_to_bind.size(), 1);
  const auto ref_map(package_qq_info.LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(pp_ref, ref_map, "pp");
  const ReferenceComponent &pp_ref_comp(pp_ref->components->Value());
  EXPECT_EQ(pp_ref_comp.identifier, "pp");
  EXPECT_EQ(pp_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(pp_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(pp_ref_comp.resolved_symbol, nullptr);

  ASSERT_EQ(pp_ref->components->Children().size(), 1);
  const ReferenceComponentNode &base_ref(
      pp_ref->components->Children().front());
  const ReferenceComponent &base_ref_comp(base_ref.Value());
  EXPECT_EQ(base_ref_comp.identifier, "base");
  EXPECT_EQ(base_ref_comp.ref_type, ReferenceType::kDirectMember);
  EXPECT_EQ(base_ref_comp.required_metatype, SymbolMetaType::kClass);
  EXPECT_EQ(base_ref_comp.resolved_symbol, nullptr);

  // Make sure the "pp::base" reference is linked from the "qq::derived" class.
  ASSERT_EQ(
      derived_class_info.parent_type.user_defined_type,
      package_qq_info.local_references_to_bind.front().LastTypeComponent());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // Resolve the "pp::base" type reference to the "pp::base" class.
    EXPECT_EQ(pp_ref_comp.resolved_symbol, &package_pp);
    EXPECT_EQ(base_ref_comp.resolved_symbol, &base_class);
    EXPECT_EQ(derived_class_info.parent_type.user_defined_type->Value()
                  .resolved_symbol,
              &base_class);
  }
}

TEST(BuildSymbolTableTest, ClassDeclarationInheritanceFromNestedClass) {
  TestVerilogSourceFile src("classilicious.sv",
                            "class pp;\n"
                            "  class base;\n"
                            "  endclass\n"
                            "endclass\n"
                            "class qq;\n"
                            "  class derived extends pp::base;\n"
                            "  endclass\n"
                            "endclass\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(class_pp, root_symbol, "pp");
  EXPECT_EQ(class_pp_info.metatype, SymbolMetaType::kClass);

  MUST_ASSIGN_LOOKUP_SYMBOL(base_class, class_pp, "base");
  EXPECT_EQ(base_class_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(base_class_info.file_origin, &src);
  EXPECT_EQ(base_class_info.declared_type.syntax_origin, nullptr);
  EXPECT_TRUE(base_class_info.local_references_to_bind.empty());

  MUST_ASSIGN_LOOKUP_SYMBOL(class_qq, root_symbol, "qq");
  EXPECT_EQ(class_qq_info.metatype, SymbolMetaType::kClass);

  MUST_ASSIGN_LOOKUP_SYMBOL(derived_class, class_qq, "derived");
  EXPECT_EQ(derived_class_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(derived_class_info.file_origin, &src);
  EXPECT_EQ(derived_class_info.declared_type.syntax_origin, nullptr);
  EXPECT_TRUE(derived_class_info.local_references_to_bind.empty());

  // "pp::base" is referenced from the scope that contains "derived",
  // which is package "qq".
  EXPECT_EQ(class_qq_info.local_references_to_bind.size(), 1);
  const auto ref_map(class_qq_info.LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(pp_ref, ref_map, "pp");
  const ReferenceComponent &pp_ref_comp(pp_ref->components->Value());
  EXPECT_EQ(pp_ref_comp.identifier, "pp");
  EXPECT_EQ(pp_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(pp_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(pp_ref_comp.resolved_symbol, nullptr);

  ASSERT_EQ(pp_ref->components->Children().size(), 1);
  const ReferenceComponentNode &base_ref(
      pp_ref->components->Children().front());
  const ReferenceComponent &base_ref_comp(base_ref.Value());
  EXPECT_EQ(base_ref_comp.identifier, "base");
  EXPECT_EQ(base_ref_comp.ref_type, ReferenceType::kDirectMember);
  EXPECT_EQ(base_ref_comp.required_metatype, SymbolMetaType::kClass);
  EXPECT_EQ(base_ref_comp.resolved_symbol, nullptr);

  // Make sure the "pp::base" reference is linked from the "qq::derived" class.
  ASSERT_EQ(derived_class_info.parent_type.user_defined_type,
            class_qq_info.local_references_to_bind.front().LastTypeComponent());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // Resolve the "pp::base" type reference to the "pp::base" class.
    EXPECT_EQ(pp_ref_comp.resolved_symbol, &class_pp);
    EXPECT_EQ(base_ref_comp.resolved_symbol, &base_class);
    EXPECT_EQ(derived_class_info.parent_type.user_defined_type->Value()
                  .resolved_symbol,
              &base_class);
  }
}

TEST(BuildSymbolTableTest, ClassDeclarationInLineConstructorDefinition) {
  TestVerilogSourceFile src("ctor.sv",
                            "class C;\n"
                            "  function new();\n"
                            "  endfunction\n"
                            "endclass\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(class_c, root_symbol, "C");
  EXPECT_EQ(class_c_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(class_c_info.file_origin, &src);
  EXPECT_EQ(class_c_info.declared_type.syntax_origin, nullptr);

  MUST_ASSIGN_LOOKUP_SYMBOL(ctor, class_c, "new");
  EXPECT_EQ(ctor_info.metatype, SymbolMetaType::kFunction);
  EXPECT_EQ(ctor_info.file_origin, &src);
  EXPECT_NE(ctor_info.syntax_origin, nullptr);
  EXPECT_NE(ctor_info.declared_type.syntax_origin, nullptr);  // points to "new"
  // constructor is already known to "return" its class type
  EXPECT_EQ(ctor_info.declared_type.user_defined_type->Value().resolved_symbol,
            &class_c);

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, ClassDeclarationOutOfLineConstructorDefinition) {
  TestVerilogSourceFile src("ctor.sv",
                            "class C;\n"
                            "  extern function new;\n"
                            "endclass\n"
                            "function C::new ();\n"
                            "endfunction\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(class_c, root_symbol, "C");
  EXPECT_EQ(class_c_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(class_c_info.file_origin, &src);
  EXPECT_EQ(class_c_info.declared_type.syntax_origin, nullptr);

  MUST_ASSIGN_LOOKUP_SYMBOL(ctor, class_c, "new");
  EXPECT_EQ(ctor_info.metatype, SymbolMetaType::kFunction);
  EXPECT_EQ(ctor_info.file_origin, &src);
  EXPECT_NE(ctor_info.syntax_origin, nullptr);
  EXPECT_NE(ctor_info.declared_type.syntax_origin, nullptr);  // points to "new"
  // constructor is already known to "return" its class type
  EXPECT_EQ(ctor_info.declared_type.user_defined_type->Value().resolved_symbol,
            &class_c);

  // Expect a "C::new" reference from the out-of-line definition.
  const auto ref_map(root_symbol.Value().LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(class_c_ref, ref_map, "C");
  const ReferenceComponent &c_ref_comp(class_c_ref->components->Value());
  EXPECT_EQ(c_ref_comp.identifier, "C");
  EXPECT_EQ(c_ref_comp.ref_type, ReferenceType::kImmediate);
  EXPECT_EQ(c_ref_comp.required_metatype, SymbolMetaType::kClass);
  // out-of-line class and method reference must be resolved at build-time
  EXPECT_NE(c_ref_comp.resolved_symbol, nullptr);
  const ReferenceComponent &ctor_ref_comp(class_c_ref->LastLeaf()->Value());
  EXPECT_EQ(ctor_ref_comp.identifier, "new");
  EXPECT_EQ(ctor_ref_comp.ref_type, ReferenceType::kDirectMember);
  EXPECT_EQ(ctor_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_NE(ctor_ref_comp.resolved_symbol, nullptr);

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    EXPECT_EQ(c_ref_comp.resolved_symbol, &class_c);  // class C
    EXPECT_EQ(ctor_ref_comp.resolved_symbol, &ctor);  // function C::new
  }
}

TEST(BuildSymbolTableTest, ClassDeclarationReferenceInheritedMemberFromMethod) {
  TestVerilogSourceFile src("member_from_parent.sv",
                            "class base;\n"
                            "  int count;\n"
                            "endclass\n"
                            "class derived extends base;\n"
                            "  function int get_count();\n"
                            "    return count;\n"
                            "  endfunction\n"
                            "endclass\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(base_class, root_symbol, "base");
  EXPECT_EQ(base_class_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(base_class_info.file_origin, &src);
  EXPECT_EQ(base_class_info.declared_type.syntax_origin, nullptr);
  EXPECT_TRUE(base_class_info.local_references_to_bind.empty());

  MUST_ASSIGN_LOOKUP_SYMBOL(int_count, base_class, "count");
  EXPECT_EQ(int_count_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(int_count_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(derived_class, root_symbol, "derived");
  EXPECT_EQ(derived_class_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(derived_class_info.file_origin, &src);
  EXPECT_EQ(derived_class_info.declared_type.syntax_origin, nullptr);
  EXPECT_TRUE(derived_class_info.local_references_to_bind.empty());

  MUST_ASSIGN_LOOKUP_SYMBOL(get_count, derived_class, "get_count");
  EXPECT_EQ(get_count_info.metatype, SymbolMetaType::kFunction);
  ASSERT_EQ(get_count_info.local_references_to_bind.size(), 1);

  // "base::count" is referenced from the "derived::get_count" method
  EXPECT_EQ(get_count_info.local_references_to_bind.size(), 1);
  const auto ref_map(get_count_info.LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(count_ref, ref_map, "count");
  const ReferenceComponent &count_ref_comp(count_ref->components->Value());
  EXPECT_EQ(count_ref_comp.identifier, "count");
  EXPECT_EQ(count_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(count_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(count_ref_comp.resolved_symbol, nullptr);

  // Make sure the "base" reference is linked from the "derived" class.
  ASSERT_EQ(
      derived_class_info.parent_type.user_defined_type,
      root_symbol.Value().local_references_to_bind.front().LastTypeComponent());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // Resolve the "base" type reference to the "base" class.
    EXPECT_EQ(derived_class_info.parent_type.user_defined_type->Value()
                  .resolved_symbol,
              &base_class);
    // "count" in "get_count" resolved to "base::count"
    EXPECT_EQ(count_ref_comp.resolved_symbol, &int_count);
  }
}

TEST(BuildSymbolTableTest, ClassDeclarationReferenceGrandparentMember) {
  TestVerilogSourceFile src("member_from_parent.sv",
                            "class base;\n"
                            "  int count;\n"
                            "endclass\n"
                            "class derived extends base;\n"
                            "endclass\n"
                            "class more_derived extends derived;\n"
                            "  function int get_count();\n"
                            "    return count;\n"
                            "  endfunction\n"
                            "endclass\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(base_class, root_symbol, "base");
  EXPECT_EQ(base_class_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(base_class_info.file_origin, &src);
  EXPECT_EQ(base_class_info.declared_type.syntax_origin, nullptr);
  EXPECT_TRUE(base_class_info.local_references_to_bind.empty());

  MUST_ASSIGN_LOOKUP_SYMBOL(int_count, base_class, "count");
  EXPECT_EQ(int_count_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(int_count_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(derived_class, root_symbol, "derived");
  EXPECT_EQ(derived_class_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(derived_class_info.file_origin, &src);
  EXPECT_EQ(derived_class_info.declared_type.syntax_origin, nullptr);
  EXPECT_TRUE(derived_class_info.local_references_to_bind.empty());

  MUST_ASSIGN_LOOKUP_SYMBOL(more_derived_class, root_symbol, "more_derived");
  EXPECT_EQ(more_derived_class_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(more_derived_class_info.file_origin, &src);
  EXPECT_EQ(more_derived_class_info.declared_type.syntax_origin, nullptr);
  EXPECT_TRUE(more_derived_class_info.local_references_to_bind.empty());

  MUST_ASSIGN_LOOKUP_SYMBOL(get_count, more_derived_class, "get_count");
  EXPECT_EQ(get_count_info.metatype, SymbolMetaType::kFunction);
  ASSERT_EQ(get_count_info.local_references_to_bind.size(), 1);

  // "base::count" is referenced from the "more_derived::get_count" method
  EXPECT_EQ(get_count_info.local_references_to_bind.size(), 1);
  const auto ref_map(get_count_info.LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(count_ref, ref_map, "count");
  const ReferenceComponent &count_ref_comp(count_ref->components->Value());
  EXPECT_EQ(count_ref_comp.identifier, "count");
  EXPECT_EQ(count_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(count_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(count_ref_comp.resolved_symbol, nullptr);

  // Make sure the "base" reference is linked from the "derived" class.
  // Make sure the "derived" reference is linked from the "more_derived" class.
  const auto root_refs = root_symbol.Value().LocalReferencesMapViewForTesting();
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(base_ref, root_refs, "base");
  const ReferenceComponent &base_ref_comp(base_ref->components->Value());
  EXPECT_EQ(base_ref_comp.identifier, "base");
  EXPECT_EQ(base_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(base_ref_comp.required_metatype, SymbolMetaType::kClass);
  EXPECT_EQ(base_ref_comp.resolved_symbol, nullptr);

  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(derived_ref, root_refs, "derived");
  const ReferenceComponent &derived_ref_comp(derived_ref->components->Value());
  EXPECT_EQ(derived_ref_comp.identifier, "derived");
  EXPECT_EQ(derived_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(derived_ref_comp.required_metatype, SymbolMetaType::kClass);
  EXPECT_EQ(derived_ref_comp.resolved_symbol, nullptr);

  EXPECT_EQ(derived_class_info.parent_type.user_defined_type,
            base_ref->LastTypeComponent());
  EXPECT_EQ(more_derived_class_info.parent_type.user_defined_type,
            derived_ref->LastTypeComponent());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // Resolve the "base" and "derived" type references
    EXPECT_EQ(base_ref_comp.resolved_symbol, &base_class);
    EXPECT_EQ(derived_class_info.parent_type.user_defined_type->Value()
                  .resolved_symbol,
              &base_class);
    EXPECT_EQ(derived_ref_comp.resolved_symbol, &derived_class);
    EXPECT_EQ(more_derived_class_info.parent_type.user_defined_type->Value()
                  .resolved_symbol,
              &derived_class);
    // "count" in "more_derived::get_count" resolved to "base::count"
    EXPECT_EQ(count_ref_comp.resolved_symbol, &int_count);
  }
}

TEST(BuildSymbolTableTest,
     ClassDeclarationReferenceInheritedMemberDirectAccess) {
  TestVerilogSourceFile src("member_from_parent.sv",
                            "class base;\n"
                            "  int count;\n"
                            "endclass\n"
                            "class derived extends base;\n"
                            "endclass\n"
                            "function int get_count(derived dd);\n"
                            "  return dd.count;\n"  // direct member access
                            "endfunction\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(base_class, root_symbol, "base");
  EXPECT_EQ(base_class_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(base_class_info.file_origin, &src);
  EXPECT_EQ(base_class_info.declared_type.syntax_origin, nullptr);
  EXPECT_TRUE(base_class_info.local_references_to_bind.empty());

  MUST_ASSIGN_LOOKUP_SYMBOL(int_count, base_class, "count");
  EXPECT_EQ(int_count_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(int_count_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(derived_class, root_symbol, "derived");
  EXPECT_EQ(derived_class_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(derived_class_info.file_origin, &src);
  EXPECT_EQ(derived_class_info.declared_type.syntax_origin, nullptr);
  EXPECT_TRUE(derived_class_info.local_references_to_bind.empty());

  MUST_ASSIGN_LOOKUP_SYMBOL(get_count, root_symbol, "get_count");
  EXPECT_EQ(get_count_info.metatype, SymbolMetaType::kFunction);
  // references "derived" as a type and "dd" as an argument
  ASSERT_EQ(get_count_info.local_references_to_bind.size(), 2);

  MUST_ASSIGN_LOOKUP_SYMBOL(dd_arg, get_count, "dd");
  EXPECT_EQ(dd_arg_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  ASSERT_NE(dd_arg_info.declared_type.user_defined_type, nullptr);
  EXPECT_EQ(dd_arg_info.declared_type.user_defined_type->Value().identifier,
            "derived");
  EXPECT_EQ(
      dd_arg_info.declared_type.user_defined_type->Value().resolved_symbol,
      nullptr);

  // "base::count" is referenced from the "dd.count"
  const auto ref_map(get_count_info.LocalReferencesMapViewForTesting());

  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(derived_type_ref, ref_map, "derived");
  const ReferenceComponent &derived_type_ref_comp(
      derived_type_ref->components->Value());
  EXPECT_EQ(derived_type_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(derived_type_ref_comp.required_metatype,
            SymbolMetaType::kUnspecified);
  EXPECT_EQ(derived_type_ref_comp.identifier, "derived");
  EXPECT_EQ(derived_type_ref_comp.resolved_symbol, nullptr);
  // Make sure "derived dd"'s type uses this type reference.
  EXPECT_EQ(dd_arg_info.declared_type.user_defined_type,
            derived_type_ref->components.get());

  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(dd_ref, ref_map, "dd");
  const ReferenceComponent &dd_ref_comp(dd_ref->components->Value());
  EXPECT_EQ(dd_ref_comp.identifier, "dd");
  EXPECT_EQ(dd_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(dd_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(dd_ref_comp.resolved_symbol, nullptr);

  ASSERT_EQ(dd_ref->components->Children().size(), 1);
  const ReferenceComponentNode &dd_count_ref(
      dd_ref->components->Children().front());
  const ReferenceComponent &dd_count_ref_comp(dd_count_ref.Value());
  EXPECT_EQ(dd_count_ref_comp.identifier, "count");
  EXPECT_EQ(dd_count_ref_comp.ref_type, ReferenceType::kMemberOfTypeOfParent);
  EXPECT_EQ(dd_count_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(dd_count_ref_comp.resolved_symbol, nullptr);

  // Make sure the "base" reference is linked from the "derived" class.
  ASSERT_EQ(
      derived_class_info.parent_type.user_defined_type,
      root_symbol.Value().local_references_to_bind.front().LastTypeComponent());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // Resolve the "base" type reference to the "base" class.
    EXPECT_EQ(derived_class_info.parent_type.user_defined_type->Value()
                  .resolved_symbol,
              &base_class);
    // "dd"'s type resolved to "derived"
    EXPECT_EQ(
        dd_arg_info.declared_type.user_defined_type->Value().resolved_symbol,
        &derived_class);
    // "dd" references function parameter
    EXPECT_EQ(dd_ref_comp.resolved_symbol, &dd_arg);
    // "count" in "dd.count" resolved to "base::count"
    EXPECT_EQ(dd_count_ref_comp.resolved_symbol, &int_count);
  }
}

TEST(BuildSymbolTableTest, ClassDeclarationReferenceInheritedBaseClassMethod) {
  TestVerilogSourceFile src("member_from_parent.sv",
                            "class base;\n"
                            "  function int count();\n"
                            "  endfunction\n"
                            "endclass\n"
                            "class derived extends base;\n"
                            "  function int get_count();\n"
                            "    return count();\n"
                            "  endfunction\n"
                            "endclass\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(base_class, root_symbol, "base");
  EXPECT_EQ(base_class_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(base_class_info.file_origin, &src);
  EXPECT_EQ(base_class_info.declared_type.syntax_origin, nullptr);
  EXPECT_TRUE(base_class_info.local_references_to_bind.empty());

  MUST_ASSIGN_LOOKUP_SYMBOL(int_count, base_class, "count");
  EXPECT_EQ(int_count_info.metatype, SymbolMetaType::kFunction);
  EXPECT_EQ(int_count_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(derived_class, root_symbol, "derived");
  EXPECT_EQ(derived_class_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(derived_class_info.file_origin, &src);
  EXPECT_EQ(derived_class_info.declared_type.syntax_origin, nullptr);
  EXPECT_TRUE(derived_class_info.local_references_to_bind.empty());

  MUST_ASSIGN_LOOKUP_SYMBOL(get_count, derived_class, "get_count");
  EXPECT_EQ(get_count_info.metatype, SymbolMetaType::kFunction);
  ASSERT_EQ(get_count_info.local_references_to_bind.size(), 1);

  // "base::count" is referenced from the "derived::get_count" method
  EXPECT_EQ(get_count_info.local_references_to_bind.size(), 1);
  const auto ref_map(get_count_info.LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(count_ref, ref_map, "count");
  const ReferenceComponent &count_ref_comp(count_ref->components->Value());
  EXPECT_EQ(count_ref_comp.identifier, "count");
  EXPECT_EQ(count_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(count_ref_comp.required_metatype, SymbolMetaType::kCallable);
  EXPECT_EQ(count_ref_comp.resolved_symbol, nullptr);

  // Make sure the "base" reference is linked from the "derived" class.
  ASSERT_EQ(
      derived_class_info.parent_type.user_defined_type,
      root_symbol.Value().local_references_to_bind.front().LastTypeComponent());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // Resolve the "base" type reference to the "base" class.
    EXPECT_EQ(derived_class_info.parent_type.user_defined_type->Value()
                  .resolved_symbol,
              &base_class);
    // "count" in "get_count" resolved to "base::count"
    EXPECT_EQ(count_ref_comp.resolved_symbol, &int_count);
  }
}

TEST(BuildSymbolTableTest,
     ClassDeclarationReferenceInheritedBaseMethodFromObject) {
  TestVerilogSourceFile src("member_from_parent.sv",
                            "class base;\n"
                            "  function int count();\n"
                            "  endfunction\n"
                            "endclass\n"
                            "class derived extends base;\n"
                            "endclass\n"
                            "function int get_count(derived dd);\n"
                            "  return dd.count();\n"  // method call
                            "endfunction\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(base_class, root_symbol, "base");
  EXPECT_EQ(base_class_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(base_class_info.file_origin, &src);
  EXPECT_EQ(base_class_info.declared_type.syntax_origin, nullptr);
  EXPECT_TRUE(base_class_info.local_references_to_bind.empty());

  MUST_ASSIGN_LOOKUP_SYMBOL(int_count, base_class, "count");
  EXPECT_EQ(int_count_info.metatype, SymbolMetaType::kFunction);
  EXPECT_EQ(int_count_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(derived_class, root_symbol, "derived");
  EXPECT_EQ(derived_class_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(derived_class_info.file_origin, &src);
  EXPECT_EQ(derived_class_info.declared_type.syntax_origin, nullptr);
  EXPECT_TRUE(derived_class_info.local_references_to_bind.empty());

  MUST_ASSIGN_LOOKUP_SYMBOL(get_count, root_symbol, "get_count");
  EXPECT_EQ(get_count_info.metatype, SymbolMetaType::kFunction);
  // references "derived" as a type and "dd" as an argument
  ASSERT_EQ(get_count_info.local_references_to_bind.size(), 2);

  MUST_ASSIGN_LOOKUP_SYMBOL(dd_arg, get_count, "dd");
  EXPECT_EQ(dd_arg_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  ASSERT_NE(dd_arg_info.declared_type.user_defined_type, nullptr);
  EXPECT_EQ(dd_arg_info.declared_type.user_defined_type->Value().identifier,
            "derived");
  EXPECT_EQ(
      dd_arg_info.declared_type.user_defined_type->Value().resolved_symbol,
      nullptr);

  // "base::count" is referenced from the "dd.count"
  const auto ref_map(get_count_info.LocalReferencesMapViewForTesting());

  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(derived_type_ref, ref_map, "derived");
  const ReferenceComponent &derived_type_ref_comp(
      derived_type_ref->components->Value());
  EXPECT_EQ(derived_type_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(derived_type_ref_comp.required_metatype,
            SymbolMetaType::kUnspecified);
  EXPECT_EQ(derived_type_ref_comp.identifier, "derived");
  EXPECT_EQ(derived_type_ref_comp.resolved_symbol, nullptr);
  // Make sure "derived dd"'s type uses this type reference.
  EXPECT_EQ(dd_arg_info.declared_type.user_defined_type,
            derived_type_ref->components.get());

  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(dd_ref, ref_map, "dd");
  const ReferenceComponent &dd_ref_comp(dd_ref->components->Value());
  EXPECT_EQ(dd_ref_comp.identifier, "dd");
  EXPECT_EQ(dd_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(dd_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(dd_ref_comp.resolved_symbol, nullptr);

  ASSERT_EQ(dd_ref->components->Children().size(), 1);
  const ReferenceComponentNode &dd_count_ref(
      dd_ref->components->Children().front());
  const ReferenceComponent &dd_count_ref_comp(dd_count_ref.Value());
  EXPECT_EQ(dd_count_ref_comp.identifier, "count");
  EXPECT_EQ(dd_count_ref_comp.ref_type, ReferenceType::kMemberOfTypeOfParent);
  EXPECT_EQ(dd_count_ref_comp.required_metatype, SymbolMetaType::kCallable);
  EXPECT_EQ(dd_count_ref_comp.resolved_symbol, nullptr);

  // Make sure the "base" reference is linked from the "derived" class.
  ASSERT_EQ(
      derived_class_info.parent_type.user_defined_type,
      root_symbol.Value().local_references_to_bind.front().LastTypeComponent());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // Resolve the "base" type reference to the "base" class.
    EXPECT_EQ(derived_class_info.parent_type.user_defined_type->Value()
                  .resolved_symbol,
              &base_class);
    // "dd"'s type resolved to "derived"
    EXPECT_EQ(
        dd_arg_info.declared_type.user_defined_type->Value().resolved_symbol,
        &derived_class);
    // "dd" references function parameter
    EXPECT_EQ(dd_ref_comp.resolved_symbol, &dd_arg);
    // "count()" in "dd.count()" resolved to "base::count()"
    EXPECT_EQ(dd_count_ref_comp.resolved_symbol, &int_count);
  }
}

TEST(BuildSymbolTableTest, TypeParameterizedModuleDeclaration) {
  TestVerilogSourceFile src("camelot_param_alot.sv",
                            "module mm #(parameter type T = bit);\n"
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(mm_module, root_symbol, "mm");
  EXPECT_EQ(mm_module_info.metatype, SymbolMetaType::kModule);
  EXPECT_EQ(mm_module_info.file_origin, &src);
  EXPECT_EQ(mm_module_info.declared_type.syntax_origin, nullptr);
  EXPECT_TRUE(mm_module_info.local_references_to_bind.empty());

  MUST_ASSIGN_LOOKUP_SYMBOL(t_type_param, mm_module, "T");
  EXPECT_EQ(t_type_param_info.metatype, SymbolMetaType::kParameter);
  EXPECT_EQ(t_type_param_info.file_origin, &src);

  EXPECT_TRUE(root_symbol.Value().local_references_to_bind.empty());

  {  // No references.
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, TypeParameterizedClassDataDeclarations) {
  TestVerilogSourceFile src("i_push_the_param_alot.sv",
                            "class cc #(parameter type T = bit);\n"
                            "endclass\n"
                            "cc#(cc#(int)) data;\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(cc_class, root_symbol, "cc");
  EXPECT_EQ(cc_class_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(cc_class_info.file_origin, &src);
  EXPECT_EQ(cc_class_info.declared_type.syntax_origin, nullptr);
  EXPECT_TRUE(cc_class_info.local_references_to_bind.empty());

  MUST_ASSIGN_LOOKUP_SYMBOL(t_type_param, cc_class, "T");
  EXPECT_EQ(t_type_param_info.metatype, SymbolMetaType::kParameter);
  EXPECT_EQ(t_type_param_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(data, root_symbol, "data");
  EXPECT_EQ(data_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(data_info.file_origin, &src);

  ASSERT_EQ(root_symbol.Value().local_references_to_bind.size(), 2);

  const auto ref_map(root_symbol.Value().LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND(cc_refs, ref_map, "cc");
  EXPECT_EQ(cc_refs.size(), 2);

  for (const auto &cc_ref : cc_refs) {
    const ReferenceComponent &cc_ref_comp(cc_ref->components->Value());
    EXPECT_EQ(cc_ref_comp.ref_type, ReferenceType::kUnqualified);
    EXPECT_EQ(cc_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
    EXPECT_EQ(cc_ref_comp.identifier, "cc");
    EXPECT_EQ(cc_ref_comp.resolved_symbol, nullptr);
  }

  // Of the two "cc" type refs, the outer one is the first one, by ordering of
  // textual position among references that start with the same identifier.
  const DependentReferences &data_cc_type(**cc_refs.begin());
  EXPECT_EQ(data_info.declared_type.user_defined_type,
            data_cc_type.LastTypeComponent());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    for (const auto &cc_ref : cc_refs) {
      const ReferenceComponent &cc_ref_comp(cc_ref->components->Value());
      EXPECT_EQ(cc_ref_comp.resolved_symbol, &cc_class);
    }
    // type of "data" is resolved
    EXPECT_EQ(
        data_info.declared_type.user_defined_type->Value().resolved_symbol,
        &cc_class);
  }
}

TEST(BuildSymbolTableTest,
     TypeParameterizedClassDataDeclarationsPackageQualifiedTwoParams) {
  TestVerilogSourceFile src(
      "i_eat_ham_and_jam_and_spam_alot.sv",
      "package pp;\n"
      "  class cc #(\n"
      "    parameter type T1 = bit,\n"
      "    parameter type T2 = bit\n"
      "  );\n"
      "  endclass\n"
      "endpackage\n"
      "pp::cc#(pp::cc#(int, bit), pp::cc#(bit, int)) data;\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(pp_package, root_symbol, "pp");
  EXPECT_EQ(pp_package_info.metatype, SymbolMetaType::kPackage);

  MUST_ASSIGN_LOOKUP_SYMBOL(cc_class, pp_package, "cc");
  EXPECT_EQ(cc_class_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(cc_class_info.file_origin, &src);
  EXPECT_EQ(cc_class_info.declared_type.syntax_origin, nullptr);
  EXPECT_TRUE(cc_class_info.local_references_to_bind.empty());

  MUST_ASSIGN_LOOKUP_SYMBOL(t1_type_param, cc_class, "T1");
  EXPECT_EQ(t1_type_param_info.metatype, SymbolMetaType::kParameter);
  EXPECT_EQ(t1_type_param_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(t2_type_param, cc_class, "T2");
  EXPECT_EQ(t2_type_param_info.metatype, SymbolMetaType::kParameter);
  EXPECT_EQ(t2_type_param_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(data, root_symbol, "data");
  EXPECT_EQ(data_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(data_info.file_origin, &src);

  ASSERT_EQ(root_symbol.Value().local_references_to_bind.size(), 3);

  const auto ref_map(root_symbol.Value().LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND(pp_refs, ref_map, "pp");
  EXPECT_EQ(pp_refs.size(), 3);

  // all "pp::cc" references have the same structure
  for (const auto &pp_ref : pp_refs) {
    const ReferenceComponent &pp_ref_comp(pp_ref->components->Value());
    EXPECT_EQ(pp_ref_comp.identifier, "pp");
    EXPECT_EQ(pp_ref_comp.ref_type, ReferenceType::kUnqualified);
    EXPECT_EQ(pp_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
    EXPECT_EQ(pp_ref_comp.resolved_symbol, nullptr);

    ASSERT_EQ(pp_ref->components->Children().size(), 1);
    const ReferenceComponentNode &cc_ref(
        pp_ref->components->Children().front());
    const ReferenceComponent &cc_ref_comp(cc_ref.Value());
    EXPECT_EQ(cc_ref_comp.ref_type, ReferenceType::kDirectMember);
    EXPECT_EQ(cc_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
    EXPECT_EQ(cc_ref_comp.identifier, "cc");
    EXPECT_EQ(cc_ref_comp.resolved_symbol, nullptr);
  }

  // Of all the "pp::cc" type refs, the outer one is the first one, by ordering
  // of textual position among references that start with the same identifier.
  const DependentReferences &data_cc_type(**pp_refs.begin());
  EXPECT_EQ(data_info.declared_type.user_defined_type,
            data_cc_type.LastTypeComponent());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    for (const auto &pp_ref : pp_refs) {
      const ReferenceComponent &pp_ref_comp(pp_ref->components->Value());
      EXPECT_EQ(pp_ref_comp.resolved_symbol, &pp_package);

      const ReferenceComponentNode &cc_ref(
          pp_ref->components->Children().front());
      const ReferenceComponent &cc_ref_comp(cc_ref.Value());
      EXPECT_EQ(cc_ref_comp.resolved_symbol, &cc_class);
    }
    // type of "data" is resolved
    EXPECT_EQ(
        data_info.declared_type.user_defined_type->Value().resolved_symbol,
        &cc_class);
  }
}

TEST(BuildSymbolTableTest, NestedTypeParameterizedClassDataDeclaration) {
  TestVerilogSourceFile src(
      "its_fun_down_here_in_Camelot.sv",
      "class outer #(parameter type S = int);\n"
      "  class cc #(parameter type T = bit);\n"
      "  endclass\n"
      "endclass\n"
      "outer#(outer#(int)::cc#(int))::cc#(outer#(bit)::cc#(bit)) data;\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(outer_class, root_symbol, "outer");
  EXPECT_EQ(outer_class_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(outer_class_info.file_origin, &src);
  EXPECT_EQ(outer_class_info.declared_type.syntax_origin, nullptr);
  EXPECT_TRUE(outer_class_info.local_references_to_bind.empty());

  MUST_ASSIGN_LOOKUP_SYMBOL(s_type_param, outer_class, "S");
  EXPECT_EQ(s_type_param_info.metatype, SymbolMetaType::kParameter);
  EXPECT_EQ(s_type_param_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(cc_class, outer_class, "cc");
  EXPECT_EQ(cc_class_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(cc_class_info.file_origin, &src);
  EXPECT_EQ(cc_class_info.declared_type.syntax_origin, nullptr);
  EXPECT_TRUE(cc_class_info.local_references_to_bind.empty());

  MUST_ASSIGN_LOOKUP_SYMBOL(t_type_param, cc_class, "T");
  EXPECT_EQ(t_type_param_info.metatype, SymbolMetaType::kParameter);
  EXPECT_EQ(t_type_param_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(data, root_symbol, "data");
  EXPECT_EQ(data_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(data_info.file_origin, &src);

  ASSERT_EQ(root_symbol.Value().local_references_to_bind.size(), 3);

  const auto ref_map(root_symbol.Value().LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND(outer_refs, ref_map, "outer");
  EXPECT_EQ(outer_refs.size(), 3);

  // all "pp::cc" references have the same structure
  for (const auto &outer_ref : outer_refs) {
    const ReferenceComponent &outer_ref_comp(outer_ref->components->Value());
    EXPECT_EQ(outer_ref_comp.identifier, "outer");
    EXPECT_EQ(outer_ref_comp.ref_type, ReferenceType::kUnqualified);
    EXPECT_EQ(outer_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
    EXPECT_EQ(outer_ref_comp.resolved_symbol, nullptr);

    ASSERT_EQ(outer_ref->components->Children().size(), 1);
    const ReferenceComponentNode &cc_ref(
        outer_ref->components->Children().front());
    const ReferenceComponent &cc_ref_comp(cc_ref.Value());
    EXPECT_EQ(cc_ref_comp.ref_type, ReferenceType::kDirectMember);
    EXPECT_EQ(cc_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
    EXPECT_EQ(cc_ref_comp.identifier, "cc");
    EXPECT_EQ(cc_ref_comp.resolved_symbol, nullptr);
  }

  // Of all the "outer::cc" type refs, the outer one is the first one, by
  // ordering of textual position among references that start with the same
  // identifier.
  const DependentReferences &data_cc_type(**outer_refs.begin());
  EXPECT_EQ(data_info.declared_type.user_defined_type,
            data_cc_type.LastTypeComponent());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    for (const auto &outer_ref : outer_refs) {
      const ReferenceComponent &outer_ref_comp(outer_ref->components->Value());
      EXPECT_EQ(outer_ref_comp.resolved_symbol, &outer_class);

      const ReferenceComponentNode &cc_ref(
          outer_ref->components->Children().front());
      const ReferenceComponent &cc_ref_comp(cc_ref.Value());
      EXPECT_EQ(cc_ref_comp.resolved_symbol, &cc_class);
    }
    // type of "data" is resolved
    EXPECT_EQ(
        data_info.declared_type.user_defined_type->Value().resolved_symbol,
        &cc_class);
  }
}

TEST(BuildSymbolTableTest,
     TypeParameterizedClassDataDeclarationNamedParameters) {
  TestVerilogSourceFile src("its_fun_down_here_in_Camelot.sv",
                            "class cc #(\n"
                            "  parameter type S = int,\n"
                            "  parameter type T = bit\n"
                            ");\n"
                            "endclass\n"
                            "cc#(.S(int), .T(int)) data;\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(cc_class, root_symbol, "cc");
  EXPECT_EQ(cc_class_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(cc_class_info.file_origin, &src);
  EXPECT_EQ(cc_class_info.declared_type.syntax_origin, nullptr);
  EXPECT_TRUE(cc_class_info.local_references_to_bind.empty());

  MUST_ASSIGN_LOOKUP_SYMBOL(s_type_param, cc_class, "S");
  EXPECT_EQ(s_type_param_info.metatype, SymbolMetaType::kParameter);
  EXPECT_EQ(s_type_param_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(t_type_param, cc_class, "T");
  EXPECT_EQ(t_type_param_info.metatype, SymbolMetaType::kParameter);
  EXPECT_EQ(t_type_param_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(data, root_symbol, "data");
  EXPECT_EQ(data_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(data_info.file_origin, &src);

  ASSERT_EQ(root_symbol.Value().local_references_to_bind.size(), 1);
  const auto ref_map(root_symbol.Value().LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND(cc_refs, ref_map, "cc");
  ASSIGN_MUST_HAVE_UNIQUE(cc_ref, cc_refs);
  const ReferenceComponent &cc_ref_comp(cc_ref->components->Value());
  EXPECT_EQ(cc_ref_comp.identifier, "cc");
  EXPECT_EQ(cc_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(cc_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(cc_ref_comp.resolved_symbol, nullptr);

  const ReferenceComponentMap param_ref_map(
      ReferenceComponentNodeMapView(*cc_ref->components));
  ASSIGN_MUST_FIND(s_ref, param_ref_map, "S");
  const ReferenceComponent &s_ref_comp(s_ref->Value());
  EXPECT_EQ(s_ref_comp.identifier, "S");
  EXPECT_EQ(s_ref_comp.ref_type, ReferenceType::kDirectMember);
  EXPECT_EQ(s_ref_comp.required_metatype, SymbolMetaType::kParameter);
  EXPECT_EQ(s_ref_comp.resolved_symbol, nullptr);

  ASSIGN_MUST_FIND(t_ref, param_ref_map, "T");
  const ReferenceComponent &t_ref_comp(t_ref->Value());
  EXPECT_EQ(t_ref_comp.identifier, "T");
  EXPECT_EQ(t_ref_comp.ref_type, ReferenceType::kDirectMember);
  EXPECT_EQ(t_ref_comp.required_metatype, SymbolMetaType::kParameter);
  EXPECT_EQ(t_ref_comp.resolved_symbol, nullptr);

  const DependentReferences &data_cc_type(*cc_ref);
  EXPECT_EQ(data_info.declared_type.user_defined_type,
            data_cc_type.components.get());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    EXPECT_EQ(cc_ref_comp.resolved_symbol, &cc_class);
    EXPECT_EQ(s_ref_comp.resolved_symbol, &s_type_param);
    EXPECT_EQ(t_ref_comp.resolved_symbol, &t_type_param);
    // type of "data" is resolved
    EXPECT_EQ(
        data_info.declared_type.user_defined_type->Value().resolved_symbol,
        &cc_class);
  }
}

TEST(BuildSymbolTableTest,
     NestedTypeParameterizedClassDataDeclarationNamedParameters) {
  TestVerilogSourceFile src(
      "i_need_to_upgrade_my_RAM_alot.sv",
      "class outer #(parameter type S = int);\n"
      "  class cc #(parameter type T = bit);\n"
      "  endclass\n"
      "endclass\n"
      "outer#(.S(outer#(.S(int))::cc#(.T(int))))\n"
      "    ::cc#(.T(outer#(.S(bit))::cc#(.T(bit)))) data;\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(outer_class, root_symbol, "outer");
  EXPECT_EQ(outer_class_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(outer_class_info.file_origin, &src);
  EXPECT_EQ(outer_class_info.declared_type.syntax_origin, nullptr);
  EXPECT_TRUE(outer_class_info.local_references_to_bind.empty());

  MUST_ASSIGN_LOOKUP_SYMBOL(s_type_param, outer_class, "S");
  EXPECT_EQ(s_type_param_info.metatype, SymbolMetaType::kParameter);
  EXPECT_EQ(s_type_param_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(cc_class, outer_class, "cc");
  EXPECT_EQ(cc_class_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(cc_class_info.file_origin, &src);
  EXPECT_EQ(cc_class_info.declared_type.syntax_origin, nullptr);
  EXPECT_TRUE(cc_class_info.local_references_to_bind.empty());

  MUST_ASSIGN_LOOKUP_SYMBOL(t_type_param, cc_class, "T");
  EXPECT_EQ(t_type_param_info.metatype, SymbolMetaType::kParameter);
  EXPECT_EQ(t_type_param_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(data, root_symbol, "data");
  EXPECT_EQ(data_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(data_info.file_origin, &src);

  ASSERT_EQ(root_symbol.Value().local_references_to_bind.size(), 3);

  const auto ref_map(root_symbol.Value().LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND(outer_refs, ref_map, "outer");
  EXPECT_EQ(outer_refs.size(), 3);

  // all "outer::cc" references have the same structure
  for (const auto &outer_ref : outer_refs) {
    const ReferenceComponent &outer_ref_comp(outer_ref->components->Value());
    EXPECT_EQ(outer_ref_comp.identifier, "outer");
    EXPECT_EQ(outer_ref_comp.ref_type, ReferenceType::kUnqualified);
    EXPECT_EQ(outer_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
    EXPECT_EQ(outer_ref_comp.resolved_symbol, nullptr);

    ASSERT_EQ(outer_ref->components->Children().size(), 2);

    const ReferenceComponentNode &s_param_ref(
        outer_ref->components->Children().front());
    const ReferenceComponent &s_param_ref_comp(s_param_ref.Value());
    EXPECT_EQ(s_param_ref_comp.ref_type, ReferenceType::kDirectMember);
    EXPECT_EQ(s_param_ref_comp.required_metatype, SymbolMetaType::kParameter);
    EXPECT_EQ(s_param_ref_comp.identifier, "S");
    EXPECT_EQ(s_param_ref_comp.resolved_symbol, nullptr);

    const ReferenceComponentNode &cc_ref(
        outer_ref->components->Children().back());
    const ReferenceComponent &cc_ref_comp(cc_ref.Value());
    EXPECT_EQ(cc_ref_comp.ref_type, ReferenceType::kDirectMember);
    EXPECT_EQ(cc_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
    EXPECT_EQ(cc_ref_comp.identifier, "cc");
    EXPECT_EQ(cc_ref_comp.resolved_symbol, nullptr);

    ASSERT_EQ(cc_ref.Children().size(), 1);
    const ReferenceComponentNode &t_param_ref(cc_ref.Children().front());
    const ReferenceComponent &t_param_ref_comp(t_param_ref.Value());
    EXPECT_EQ(t_param_ref_comp.ref_type, ReferenceType::kDirectMember);
    EXPECT_EQ(t_param_ref_comp.required_metatype, SymbolMetaType::kParameter);
    EXPECT_EQ(t_param_ref_comp.identifier, "T");
    EXPECT_EQ(t_param_ref_comp.resolved_symbol, nullptr);
  }

  // Of all the "outer::cc" type refs, the outer one is the first one, by
  // ordering of textual position among references that start with the same
  // identifier.
  const DependentReferences &data_cc_type(**outer_refs.begin());
  EXPECT_EQ(data_info.declared_type.user_defined_type,
            data_cc_type.LastTypeComponent());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    for (const auto &outer_ref : outer_refs) {
      const ReferenceComponent &outer_ref_comp(outer_ref->components->Value());
      EXPECT_EQ(outer_ref_comp.resolved_symbol, &outer_class);

      const ReferenceComponentNode &s_param_ref(
          outer_ref->components->Children().front());
      const ReferenceComponent &s_param_ref_comp(s_param_ref.Value());
      EXPECT_EQ(s_param_ref_comp.resolved_symbol, &s_type_param);

      const ReferenceComponentNode &cc_ref(
          outer_ref->components->Children().back());
      const ReferenceComponent &cc_ref_comp(cc_ref.Value());
      EXPECT_EQ(cc_ref_comp.resolved_symbol, &cc_class);

      const ReferenceComponentNode &t_param_ref(cc_ref.Children().front());
      const ReferenceComponent &t_param_ref_comp(t_param_ref.Value());
      EXPECT_EQ(t_param_ref_comp.resolved_symbol, &t_type_param);
    }
    // type of "data" is resolved
    EXPECT_EQ(
        data_info.declared_type.user_defined_type->Value().resolved_symbol,
        &cc_class);
  }
}

TEST(BuildSymbolTableTest, FunctionDeclarationNoReturnType) {
  TestVerilogSourceFile src("funkytown.sv",
                            "function ff;\n"
                            "endfunction\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(function_ff, root_symbol, "ff");
  EXPECT_EQ(function_ff_info.metatype, SymbolMetaType::kFunction);
  EXPECT_EQ(function_ff_info.file_origin, &src);
  // no return type
  EXPECT_EQ(function_ff_info.declared_type.syntax_origin, nullptr);

  EXPECT_TRUE(function_ff_info.local_references_to_bind.empty());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, FunctionDeclarationWithPort) {
  TestVerilogSourceFile src("funkytown.sv",
                            "function ff(int g);\n"
                            "endfunction\n");
  // TODO: propagate type for ports like "int g, h"
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(function_ff, root_symbol, "ff");
  EXPECT_EQ(function_ff_info.metatype, SymbolMetaType::kFunction);
  EXPECT_EQ(function_ff_info.file_origin, &src);
  EXPECT_EQ(function_ff_info.declared_type.syntax_origin,
            nullptr);  // there is no function return type

  MUST_ASSIGN_LOOKUP_SYMBOL(param_g, function_ff, "g");
  EXPECT_EQ(param_g_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(param_g_info.file_origin, &src);
  ASSERT_NE(param_g_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(
      verible::StringSpanOfSymbol(*param_g_info.declared_type.syntax_origin),
      "int");

  EXPECT_TRUE(function_ff_info.local_references_to_bind.empty());
  EXPECT_TRUE(param_g_info.local_references_to_bind.empty());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, FunctionDeclarationWithLocalVariable) {
  TestVerilogSourceFile src("funkytown.sv",
                            "function ff();\n"
                            "  logic g;\n"
                            "endfunction\n");
  // TODO: propagate type for ports like "int g, h"
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(function_ff, root_symbol, "ff");
  EXPECT_EQ(function_ff_info.metatype, SymbolMetaType::kFunction);
  EXPECT_EQ(function_ff_info.file_origin, &src);
  EXPECT_EQ(function_ff_info.declared_type.syntax_origin,
            nullptr);  // there is no function return type

  MUST_ASSIGN_LOOKUP_SYMBOL(local_g, function_ff, "g");
  EXPECT_EQ(local_g_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(local_g_info.file_origin, &src);
  ASSERT_NE(local_g_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(
      verible::StringSpanOfSymbol(*local_g_info.declared_type.syntax_origin),
      "logic");

  EXPECT_TRUE(function_ff_info.local_references_to_bind.empty());
  EXPECT_TRUE(local_g_info.local_references_to_bind.empty());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, FunctionDeclarationVoidReturnType) {
  TestVerilogSourceFile src("funkytown.sv",
                            "function void ff;\n"
                            "endfunction\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(function_ff, root_symbol, "ff");
  EXPECT_EQ(function_ff_info.metatype, SymbolMetaType::kFunction);
  EXPECT_EQ(function_ff_info.file_origin, &src);
  ASSERT_NE(function_ff_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(verible::StringSpanOfSymbol(
                *function_ff_info.declared_type.syntax_origin),
            "void");

  EXPECT_TRUE(function_ff_info.local_references_to_bind.empty());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, FunctionDeclarationClassReturnType) {
  TestVerilogSourceFile src("funkytown.sv",
                            "class cc;\n"
                            "endclass\n"
                            "function cc ff;\n"  // user-defined return type
                            "endfunction\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(class_cc, root_symbol, "cc");
  MUST_ASSIGN_LOOKUP_SYMBOL(function_ff, root_symbol, "ff");
  EXPECT_EQ(function_ff_info.metatype, SymbolMetaType::kFunction);
  EXPECT_EQ(function_ff_info.file_origin, &src);
  ASSERT_NE(function_ff_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(verible::StringSpanOfSymbol(
                *function_ff_info.declared_type.syntax_origin),
            "cc");
  const ReferenceComponentNode *cc_ref =
      function_ff_info.declared_type.user_defined_type;
  ASSERT_NE(cc_ref, nullptr);
  const ReferenceComponent &cc_ref_comp(cc_ref->Value());
  EXPECT_EQ(cc_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(cc_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(cc_ref_comp.identifier, "cc");
  EXPECT_EQ(cc_ref_comp.resolved_symbol, nullptr);

  // There should be one reference to return type "cc" of function "ff".
  EXPECT_EQ(root_symbol.Value().local_references_to_bind.size(), 1);

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // Expect "cc" return type to resolve to class declaration.
    EXPECT_EQ(cc_ref_comp.resolved_symbol, &class_cc);
  }
}

TEST(BuildSymbolTableTest, FunctionDeclarationInModule) {
  TestVerilogSourceFile src("funkytown.sv",
                            "module mm;\n"
                            "function void ff();\n"
                            "endfunction\n"
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(module_mm, root_symbol, "mm");
  MUST_ASSIGN_LOOKUP_SYMBOL(function_ff, module_mm, "ff");
  EXPECT_EQ(function_ff_info.metatype, SymbolMetaType::kFunction);
  EXPECT_EQ(function_ff_info.file_origin, &src);
  ASSERT_NE(function_ff_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(verible::StringSpanOfSymbol(
                *function_ff_info.declared_type.syntax_origin),
            "void");
  const ReferenceComponentNode *ff_type =
      function_ff_info.declared_type.user_defined_type;
  EXPECT_EQ(ff_type, nullptr);

  // There are no references to resolve.
  EXPECT_TRUE(root_symbol.Value().local_references_to_bind.empty());
  EXPECT_TRUE(module_mm.Value().local_references_to_bind.empty());
  EXPECT_TRUE(function_ff.Value().local_references_to_bind.empty());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, ClassMethodFunctionDeclaration) {
  TestVerilogSourceFile src("funkytown.sv",
                            "class cc;\n"
                            "function int ff;\n"
                            "endfunction\n"
                            "endclass\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(class_cc, root_symbol, "cc");
  MUST_ASSIGN_LOOKUP_SYMBOL(function_ff, class_cc, "ff");
  EXPECT_EQ(function_ff_info.metatype, SymbolMetaType::kFunction);
  EXPECT_EQ(function_ff_info.file_origin, &src);
  ASSERT_NE(function_ff_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(verible::StringSpanOfSymbol(
                *function_ff_info.declared_type.syntax_origin),
            "int");
  const ReferenceComponentNode *ff_type =
      function_ff_info.declared_type.user_defined_type;
  EXPECT_EQ(ff_type, nullptr);

  // There are no references to resolve.
  EXPECT_TRUE(root_symbol.Value().local_references_to_bind.empty());
  EXPECT_TRUE(class_cc.Value().local_references_to_bind.empty());
  EXPECT_TRUE(function_ff.Value().local_references_to_bind.empty());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest,
     ClassMethodFunctionDeclarationPackageTypeReturnType) {
  TestVerilogSourceFile src("funkytown.sv",
                            "package aa;\n"
                            "class vv;\n"
                            "endclass\n"
                            "endpackage\n"
                            "package bb;\n"
                            "class cc;\n"
                            "function aa::vv ff();\n"
                            "endfunction\n"
                            "endclass\n"
                            "endpackage\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(package_aa, root_symbol, "aa");
  MUST_ASSIGN_LOOKUP_SYMBOL(package_bb, root_symbol, "bb");
  MUST_ASSIGN_LOOKUP_SYMBOL(class_vv, package_aa, "vv");
  MUST_ASSIGN_LOOKUP_SYMBOL(class_cc, package_bb, "cc");
  MUST_ASSIGN_LOOKUP_SYMBOL(function_ff, class_cc, "ff");

  EXPECT_EQ(function_ff_info.metatype, SymbolMetaType::kFunction);
  EXPECT_EQ(function_ff_info.file_origin, &src);
  ASSERT_NE(function_ff_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(verible::StringSpanOfSymbol(
                *function_ff_info.declared_type.syntax_origin),
            "aa::vv");

  // return type points to the last component of the chain, "vv"
  const ReferenceComponentNode *vv_ref =
      function_ff_info.declared_type.user_defined_type;
  ASSERT_NE(vv_ref, nullptr);
  const ReferenceComponent &vv_ref_comp(vv_ref->Value());
  EXPECT_EQ(vv_ref_comp.ref_type, ReferenceType::kDirectMember);
  EXPECT_EQ(vv_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(vv_ref_comp.identifier, "vv");
  EXPECT_EQ(vv_ref_comp.resolved_symbol, nullptr);

  // dependent reference parent is "aa" in "aa::vv"
  const ReferenceComponentNode *aa_ref = vv_ref->Parent();
  ASSERT_NE(aa_ref, nullptr);
  const ReferenceComponent &aa_ref_comp(aa_ref->Value());
  EXPECT_EQ(aa_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(aa_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(aa_ref_comp.identifier, "aa");
  EXPECT_EQ(aa_ref_comp.resolved_symbol, nullptr);

  // There is only one (type) reference chain to resolve: "aa::vv".
  EXPECT_TRUE(root_symbol.Value().local_references_to_bind.empty());
  EXPECT_TRUE(package_aa.Value().local_references_to_bind.empty());
  EXPECT_TRUE(package_bb.Value().local_references_to_bind.empty());
  EXPECT_EQ(class_cc.Value().local_references_to_bind.size(), 1);
  EXPECT_TRUE(function_ff.Value().local_references_to_bind.empty());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // Expect to resolve type reference chain "aa:vv"
    EXPECT_EQ(aa_ref_comp.resolved_symbol, &package_aa);
    EXPECT_EQ(vv_ref_comp.resolved_symbol, &class_vv);
  }
}

TEST(BuildSymbolTableTest, FunctionDeclarationOutOfLineMissingOuterClass) {
  TestVerilogSourceFile src("outofline_func.sv",
                            "function cc::ff;\n"  // "cc" undeclared
                            "endfunction\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  {
    ASSIGN_MUST_HAVE_UNIQUE(err, build_diagnostics);
    EXPECT_EQ(err.code(), absl::StatusCode::kNotFound);
    EXPECT_THAT(
        err.message(),
        HasSubstr("No member symbol \"cc\" in parent scope (<root>) $root"));
  }

  // out-of-line declaration creates a self-reference.
  EXPECT_EQ(root_symbol.Value().local_references_to_bind.size(), 1);

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);

    // Same diagnostic as before.
    ASSIGN_MUST_HAVE_UNIQUE(err, resolve_diagnostics);
    EXPECT_EQ(err.code(), absl::StatusCode::kNotFound);
    EXPECT_THAT(
        err.message(),
        HasSubstr("No member symbol \"cc\" in parent scope (<root>) $root"));
  }
}

TEST(BuildSymbolTableTest, FunctionDeclarationOutOfLineInvalidModuleInjection) {
  TestVerilogSourceFile src("outofline_func.sv",
                            "module mm;\n"
                            "endmodule\n"
                            "function mm::ff;\n"
                            "endfunction\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  {
    // Expect that "tt" will not injected into "mm" because it is a module,
    // not a class.
    ASSIGN_MUST_HAVE_UNIQUE(err, build_diagnostics);
    EXPECT_EQ(err.code(), absl::StatusCode::kInvalidArgument);
    EXPECT_THAT(err.message(),
                HasSubstr("Expecting reference \"mm\" to resolve to a class, "
                          "but found a module"));
  }
  MUST_ASSIGN_LOOKUP_SYMBOL(module_mm, root_symbol, "mm");
  EXPECT_EQ(module_mm_info.metatype, SymbolMetaType::kModule);
  EXPECT_EQ(module_mm.Find("ff"), module_mm.end());

  // Reference must be resolved at Build-time.
  EXPECT_EQ(root_symbol.Value().local_references_to_bind.size(), 1);
  const auto ref_map(root_symbol.Value().LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(mm_ref, ref_map, "mm");
  EXPECT_EQ(mm_ref->components->Value().resolved_symbol, nullptr);

  // Method injection will not happen for modules.
  const ReferenceComponentNode *ff_ref = mm_ref->LastLeaf();
  const ReferenceComponent &ref(ff_ref->Value());
  EXPECT_EQ(ref.identifier, "ff");
  EXPECT_EQ(ref.resolved_symbol, nullptr);

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    ASSERT_EQ(resolve_diagnostics.size(), 1);

    // Still remain unresolved.
    EXPECT_EQ(mm_ref->components->Value().resolved_symbol, nullptr);
    EXPECT_EQ(ref.resolved_symbol, nullptr);
  }
}

TEST(BuildSymbolTableTest, FunctionDeclarationOutOfLineMissingPrototype) {
  TestVerilogSourceFile src("outofline_func.sv",
                            "class cc;\n"
                            // no "ff" prototype
                            "endclass\n"
                            "function cc::ff;\n"
                            "endfunction\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  {
    // This diagnostic is non-fatal.
    // Expect that "ff" will be injected into "cc" when its method prototype is
    // missing.
    ASSIGN_MUST_HAVE_UNIQUE(err, build_diagnostics);
    EXPECT_EQ(err.code(), absl::StatusCode::kNotFound);
    EXPECT_THAT(
        err.message(),
        HasSubstr("No member symbol \"ff\" in parent scope (class) cc"));
  }
  MUST_ASSIGN_LOOKUP_SYMBOL(class_cc, root_symbol, "cc");
  MUST_ASSIGN_LOOKUP_SYMBOL(method_ff, class_cc, "ff");
  EXPECT_EQ(method_ff_info.metatype, SymbolMetaType::kFunction);

  // out-of-line declaration creates a self-reference.
  // Reference must be resolved at Build-time.
  EXPECT_EQ(root_symbol.Value().local_references_to_bind.size(), 1);
  const auto ref_map(root_symbol.Value().LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(cc_ref, ref_map, "cc");
  EXPECT_EQ(cc_ref->components->Value().resolved_symbol, &class_cc);

  // Method reference is resolved to the injected symbol.
  const ReferenceComponentNode *ff_ref = cc_ref->LastLeaf();
  const ReferenceComponent &ref(ff_ref->Value());
  EXPECT_EQ(ref.identifier, "ff");
  EXPECT_EQ(ref.resolved_symbol, &method_ff);

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // Already resolved before, still remains resolved.
    EXPECT_EQ(cc_ref->components->Value().resolved_symbol, &class_cc);
    EXPECT_EQ(ref.resolved_symbol, &method_ff);
  }
}

TEST(BuildSymbolTableTest, FunctionDeclarationMethodPrototypeOnly) {
  TestVerilogSourceFile src("outofline_func.sv",
                            "class cc;\n"
                            "  extern function int ff(logic ll);\n"
                            "endclass\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(class_cc, root_symbol, "cc");
  MUST_ASSIGN_LOOKUP_SYMBOL(method_ff, class_cc, "ff");
  EXPECT_EQ(method_ff_info.metatype, SymbolMetaType::kFunction);
  ASSERT_NE(method_ff_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(
      verible::StringSpanOfSymbol(*method_ff_info.declared_type.syntax_origin),
      "int");

  MUST_ASSIGN_LOOKUP_SYMBOL(port_ll, method_ff, "ll");
  EXPECT_EQ(port_ll_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  ASSERT_NE(port_ll_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(
      verible::StringSpanOfSymbol(*port_ll_info.declared_type.syntax_origin),
      "logic");

  // No references to resolve.
  EXPECT_TRUE(root_symbol.Value().local_references_to_bind.empty());
  EXPECT_TRUE(class_cc_info.local_references_to_bind.empty());
  EXPECT_TRUE(method_ff_info.local_references_to_bind.empty());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, FunctionDeclarationOutOfLineWithMethodPrototype) {
  TestVerilogSourceFile src(
      "outofline_func.sv",
      "class cc;\n"
      "  extern function int ff(logic ll);\n"  // prototype
      "endclass\n"
      "function int cc::ff(logic ll);\n"  // definition
      "  bit bb;\n"                       // local variable
      "endfunction\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(class_cc, root_symbol, "cc");
  MUST_ASSIGN_LOOKUP_SYMBOL(method_ff, class_cc, "ff");
  EXPECT_EQ(method_ff_info.metatype, SymbolMetaType::kFunction);
  ASSERT_NE(method_ff_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(
      verible::StringSpanOfSymbol(*method_ff_info.declared_type.syntax_origin),
      "int");

  MUST_ASSIGN_LOOKUP_SYMBOL(port_ll, method_ff, "ll");
  EXPECT_EQ(port_ll_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  ASSERT_NE(port_ll_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(
      verible::StringSpanOfSymbol(*port_ll_info.declared_type.syntax_origin),
      "logic");

  MUST_ASSIGN_LOOKUP_SYMBOL(local_bb, method_ff, "bb");
  EXPECT_EQ(local_bb_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  ASSERT_NE(local_bb_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(
      verible::StringSpanOfSymbol(*local_bb_info.declared_type.syntax_origin),
      "bit");

  // out-of-line declaration creates a self-reference.
  // Reference must be resolved at Build-time.
  EXPECT_EQ(root_symbol.Value().local_references_to_bind.size(), 1);
  const auto ref_map(root_symbol.Value().LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(cc_ref, ref_map, "cc");
  EXPECT_EQ(cc_ref->components->Value().resolved_symbol, &class_cc);

  // Method reference is resolved to the injected symbol.
  const ReferenceComponentNode *ff_ref = cc_ref->LastLeaf();
  const ReferenceComponent &ref(ff_ref->Value());
  EXPECT_EQ(ref.identifier, "ff");
  EXPECT_EQ(ref.resolved_symbol, &method_ff);

  EXPECT_TRUE(class_cc_info.local_references_to_bind.empty());
  EXPECT_TRUE(method_ff_info.local_references_to_bind.empty());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // Already resolved.
    EXPECT_EQ(cc_ref->components->Value().resolved_symbol, &class_cc);
    EXPECT_EQ(ref.resolved_symbol, &method_ff);
  }
}

TEST(BuildSymbolTableTest, TaskDeclaration) {
  TestVerilogSourceFile src("taskrabbit.sv",
                            "task tt;\n"
                            "endtask\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(task_tt, root_symbol, "tt");
  EXPECT_EQ(task_tt_info.metatype, SymbolMetaType::kTask);
  EXPECT_EQ(task_tt_info.file_origin, &src);
  // no return type
  EXPECT_EQ(task_tt_info.declared_type.syntax_origin, nullptr);

  EXPECT_TRUE(task_tt_info.local_references_to_bind.empty());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, TaskDeclarationInPackage) {
  TestVerilogSourceFile src("taskrabbit.sv",
                            "package pp;\n"
                            "task tt();\n"
                            "endtask\n"
                            "endpackage\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(package_pp, root_symbol, "pp");
  MUST_ASSIGN_LOOKUP_SYMBOL(task_tt, package_pp, "tt");
  EXPECT_EQ(task_tt_info.metatype, SymbolMetaType::kTask);
  EXPECT_EQ(task_tt_info.file_origin, &src);
  // no return type
  EXPECT_EQ(task_tt_info.declared_type.syntax_origin, nullptr);

  EXPECT_TRUE(task_tt_info.local_references_to_bind.empty());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, TaskDeclarationInModule) {
  TestVerilogSourceFile src("taskrabbit.sv",
                            "module mm;\n"
                            "task tt();\n"
                            "endtask\n"
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(module_mm, root_symbol, "mm");
  MUST_ASSIGN_LOOKUP_SYMBOL(task_tt, module_mm, "tt");
  EXPECT_EQ(task_tt_info.metatype, SymbolMetaType::kTask);
  EXPECT_EQ(task_tt_info.file_origin, &src);
  // no return type
  EXPECT_EQ(task_tt_info.declared_type.syntax_origin, nullptr);

  EXPECT_TRUE(task_tt_info.local_references_to_bind.empty());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, TaskDeclarationInClass) {
  TestVerilogSourceFile src("taskrabbit.sv",
                            "class cc;\n"
                            "task tt();\n"
                            "endtask\n"
                            "endclass\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(class_cc, root_symbol, "cc");
  MUST_ASSIGN_LOOKUP_SYMBOL(task_tt, class_cc, "tt");
  EXPECT_EQ(task_tt_info.metatype, SymbolMetaType::kTask);
  EXPECT_EQ(task_tt_info.file_origin, &src);
  // no return type
  EXPECT_EQ(task_tt_info.declared_type.syntax_origin, nullptr);

  EXPECT_TRUE(task_tt_info.local_references_to_bind.empty());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, TaskDeclarationWithPorts) {
  TestVerilogSourceFile src("taskrabbit.sv",
                            "task tt(logic ll);\n"
                            "endtask\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(task_tt, root_symbol, "tt");
  EXPECT_EQ(task_tt_info.metatype, SymbolMetaType::kTask);
  EXPECT_EQ(task_tt_info.file_origin, &src);
  // no return type
  EXPECT_EQ(task_tt_info.declared_type.syntax_origin, nullptr);

  MUST_ASSIGN_LOOKUP_SYMBOL(logic_ll, task_tt, "ll");
  EXPECT_EQ(logic_ll_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(logic_ll_info.file_origin, &src);
  // primitive type
  ASSERT_NE(logic_ll_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(
      verible::StringSpanOfSymbol(*logic_ll_info.declared_type.syntax_origin),
      "logic");

  EXPECT_TRUE(task_tt_info.local_references_to_bind.empty());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, TaskDeclarationOutOfLineMissingOuterClass) {
  TestVerilogSourceFile src("outofline_task.sv",
                            "task cc::tt;\n"  // "cc" undeclared
                            "endtask\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  {
    ASSIGN_MUST_HAVE_UNIQUE(err, build_diagnostics);
    EXPECT_EQ(err.code(), absl::StatusCode::kNotFound);
    EXPECT_THAT(
        err.message(),
        HasSubstr("No member symbol \"cc\" in parent scope (<root>) $root"));
  }

  // out-of-line declaration creates a self-reference.
  EXPECT_EQ(root_symbol.Value().local_references_to_bind.size(), 1);

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);

    // Same diagnostic as before.
    ASSIGN_MUST_HAVE_UNIQUE(err, resolve_diagnostics);
    EXPECT_EQ(err.code(), absl::StatusCode::kNotFound);
    EXPECT_THAT(
        err.message(),
        HasSubstr("No member symbol \"cc\" in parent scope (<root>) $root"));
  }
}

TEST(BuildSymbolTableTest, TaskDeclarationOutOfLineMissingPrototype) {
  TestVerilogSourceFile src("outofline_task.sv",
                            "class cc;\n"
                            // no "tt" prototype
                            "endclass\n"
                            "task cc::tt;\n"
                            "endtask\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  {
    // This diagnostic is non-fatal.
    // Expect that "tt" will be injected into "cc" when its method prototype is
    // missing.
    ASSIGN_MUST_HAVE_UNIQUE(err, build_diagnostics);
    EXPECT_EQ(err.code(), absl::StatusCode::kNotFound);
    EXPECT_THAT(
        err.message(),
        HasSubstr("No member symbol \"tt\" in parent scope (class) cc"));
  }
  MUST_ASSIGN_LOOKUP_SYMBOL(class_cc, root_symbol, "cc");
  MUST_ASSIGN_LOOKUP_SYMBOL(method_tt, class_cc, "tt");
  EXPECT_EQ(method_tt_info.metatype, SymbolMetaType::kTask);

  // out-of-line declaration creates a self-reference.
  // Reference must be resolved at Build-time.
  EXPECT_EQ(root_symbol.Value().local_references_to_bind.size(), 1);
  const auto ref_map(root_symbol.Value().LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(cc_ref, ref_map, "cc");
  EXPECT_EQ(cc_ref->components->Value().resolved_symbol, &class_cc);

  // Method reference is resolved to the injected symbol.
  const ReferenceComponentNode *tt_ref = cc_ref->LastLeaf();
  const ReferenceComponent &ref(tt_ref->Value());
  EXPECT_EQ(ref.identifier, "tt");
  EXPECT_EQ(ref.resolved_symbol, &method_tt);

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // Already resolved before, still remains resolved.
    EXPECT_EQ(cc_ref->components->Value().resolved_symbol, &class_cc);
    EXPECT_EQ(ref.resolved_symbol, &method_tt);
  }
}

TEST(BuildSymbolTableTest, TaskDeclarationOutOfLineInvalidPackageInjection) {
  TestVerilogSourceFile src("outofline_task.sv",
                            "package pp;\n"
                            "endpackage\n"
                            "task pp::tt;\n"
                            "endtask\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  {
    // Expect that "tt" will not injected into "pp" because it is a package,
    // not a class.
    ASSIGN_MUST_HAVE_UNIQUE(err, build_diagnostics);
    EXPECT_EQ(err.code(), absl::StatusCode::kInvalidArgument);
    EXPECT_THAT(err.message(),
                HasSubstr("Expecting reference \"pp\" to resolve to a class, "
                          "but found a package"));
  }
  MUST_ASSIGN_LOOKUP_SYMBOL(package_pp, root_symbol, "pp");
  EXPECT_EQ(package_pp_info.metatype, SymbolMetaType::kPackage);
  EXPECT_EQ(package_pp.Find("tt"), package_pp.end());

  // Reference must be resolved at Build-time.
  EXPECT_EQ(root_symbol.Value().local_references_to_bind.size(), 1);
  const auto ref_map(root_symbol.Value().LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(pp_ref, ref_map, "pp");
  EXPECT_EQ(pp_ref->components->Value().resolved_symbol, nullptr);

  // Method injection will not happen for packages.
  const ReferenceComponentNode *tt_ref = pp_ref->LastLeaf();
  const ReferenceComponent &ref(tt_ref->Value());
  EXPECT_EQ(ref.identifier, "tt");
  EXPECT_EQ(ref.resolved_symbol, nullptr);

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    ASSERT_EQ(resolve_diagnostics.size(), 1);

    // Still remain unresolved.
    EXPECT_EQ(pp_ref->components->Value().resolved_symbol, nullptr);
    EXPECT_EQ(ref.resolved_symbol, nullptr);
  }
}

TEST(BuildSymbolTableTest, TaskDeclarationMethodPrototypeOnly) {
  TestVerilogSourceFile src("outofline_task.sv",
                            "class cc;\n"
                            "  extern task tt(logic ll);\n"
                            "endclass\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(class_cc, root_symbol, "cc");
  MUST_ASSIGN_LOOKUP_SYMBOL(method_tt, class_cc, "tt");
  EXPECT_EQ(method_tt_info.metatype, SymbolMetaType::kTask);
  EXPECT_EQ(method_tt_info.declared_type.syntax_origin, nullptr);

  MUST_ASSIGN_LOOKUP_SYMBOL(port_ll, method_tt, "ll");
  EXPECT_EQ(port_ll_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  ASSERT_NE(port_ll_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(
      verible::StringSpanOfSymbol(*port_ll_info.declared_type.syntax_origin),
      "logic");

  // No references to resolve.
  EXPECT_TRUE(root_symbol.Value().local_references_to_bind.empty());
  EXPECT_TRUE(class_cc_info.local_references_to_bind.empty());
  EXPECT_TRUE(method_tt_info.local_references_to_bind.empty());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, TaskDeclarationOutOfLineWithMethodPrototype) {
  TestVerilogSourceFile src("outofline_task.sv",
                            "class cc;\n"
                            "  extern task tt(logic ll);\n"  // prototype
                            "endclass\n"
                            "task cc::tt(logic ll);\n"  // definition
                            "  bit bb;\n"               // local variable
                            "endtask\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(class_cc, root_symbol, "cc");
  MUST_ASSIGN_LOOKUP_SYMBOL(method_tt, class_cc, "tt");
  EXPECT_EQ(method_tt_info.metatype, SymbolMetaType::kTask);
  EXPECT_EQ(method_tt_info.declared_type.syntax_origin, nullptr);

  MUST_ASSIGN_LOOKUP_SYMBOL(port_ll, method_tt, "ll");
  EXPECT_EQ(port_ll_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  ASSERT_NE(port_ll_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(
      verible::StringSpanOfSymbol(*port_ll_info.declared_type.syntax_origin),
      "logic");

  MUST_ASSIGN_LOOKUP_SYMBOL(local_bb, method_tt, "bb");
  EXPECT_EQ(local_bb_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  ASSERT_NE(local_bb_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(
      verible::StringSpanOfSymbol(*local_bb_info.declared_type.syntax_origin),
      "bit");

  // out-of-line declaration creates a self-reference.
  // Reference must be resolved at Build-time.
  EXPECT_EQ(root_symbol.Value().local_references_to_bind.size(), 1);
  const auto ref_map(root_symbol.Value().LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(cc_ref, ref_map, "cc");
  EXPECT_EQ(cc_ref->components->Value().resolved_symbol, &class_cc);

  // Method reference is resolved to the injected symbol.
  const ReferenceComponentNode *tt_ref = cc_ref->LastLeaf();
  const ReferenceComponent &ref(tt_ref->Value());
  EXPECT_EQ(ref.identifier, "tt");
  EXPECT_EQ(ref.resolved_symbol, &method_tt);

  EXPECT_TRUE(class_cc_info.local_references_to_bind.empty());
  EXPECT_TRUE(method_tt_info.local_references_to_bind.empty());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // Already resolved.
    EXPECT_EQ(cc_ref->components->Value().resolved_symbol, &class_cc);
    EXPECT_EQ(ref.resolved_symbol, &method_tt);
  }
}

TEST(BuildSymbolTableTest, OutOfLineDefinitionMismatchesPrototype) {
  TestVerilogSourceFile src(
      "outofline_task_or_func.sv",
      "class cc;\n"
      "  extern task tt(logic ll);\n"  // task prototype
      "endclass\n"
      "function int cc::tt(logic ll);\n"  // function definition (wrong type)
      "endfunction\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  ASSIGN_MUST_HAVE_UNIQUE(err, build_diagnostics);
  EXPECT_EQ(err.code(), absl::StatusCode::kAlreadyExists);
  EXPECT_THAT(
      err.message(),
      HasSubstr(
          "task $root::cc::tt cannot be redefined out-of-line as a function"));

  MUST_ASSIGN_LOOKUP_SYMBOL(class_cc, root_symbol, "cc");
  MUST_ASSIGN_LOOKUP_SYMBOL(method_tt, class_cc, "tt");
  EXPECT_EQ(method_tt_info.metatype, SymbolMetaType::kTask);
  EXPECT_EQ(method_tt_info.declared_type.syntax_origin, nullptr);

  MUST_ASSIGN_LOOKUP_SYMBOL(port_ll, method_tt, "ll");
  EXPECT_EQ(port_ll_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  ASSERT_NE(port_ll_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(
      verible::StringSpanOfSymbol(*port_ll_info.declared_type.syntax_origin),
      "logic");

  // Reference must be resolved at Build-time.
  EXPECT_EQ(root_symbol.Value().local_references_to_bind.size(), 1);
  const auto ref_map(root_symbol.Value().LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(cc_ref, ref_map, "cc");
  EXPECT_EQ(cc_ref->components->Value().resolved_symbol, &class_cc);

  // Method reference "tt" fails to resolve due to metatype mismatch.
  const ReferenceComponentNode *tt_ref = cc_ref->LastLeaf();
  const ReferenceComponent &ref(tt_ref->Value());
  EXPECT_EQ(ref.identifier, "tt");
  EXPECT_EQ(ref.resolved_symbol, nullptr);

  EXPECT_TRUE(class_cc_info.local_references_to_bind.empty());
  EXPECT_TRUE(method_tt_info.local_references_to_bind.empty());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    ASSIGN_MUST_HAVE_UNIQUE(err, resolve_diagnostics);
    EXPECT_EQ(err.code(), absl::StatusCode::kInvalidArgument);
    EXPECT_THAT(err.message(),
                HasSubstr("Expecting reference \"tt\" to resolve to a "
                          "function, but found a task"));

    EXPECT_EQ(cc_ref->components->Value().resolved_symbol, &class_cc);
    EXPECT_EQ(ref.resolved_symbol, nullptr);  // Still fails to resolve.
  }
}

TEST(BuildSymbolTableTest, FunctionCallResolvedSameScope) {
  TestVerilogSourceFile src("call_me.sv",
                            "function int tt();\n"
                            "  return 1;\n"
                            "endfunction\n"
                            "function int vv();\n"
                            "  return tt();\n"
                            "endfunction\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(function_tt, root_symbol, "tt");
  MUST_ASSIGN_LOOKUP_SYMBOL(function_vv, root_symbol, "vv");
  EXPECT_EQ(function_tt_info.metatype, SymbolMetaType::kFunction);
  ASSERT_NE(function_tt_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(function_vv_info.metatype, SymbolMetaType::kFunction);
  ASSERT_NE(function_vv_info.declared_type.syntax_origin, nullptr);

  EXPECT_EQ(function_vv_info.local_references_to_bind.size(), 1);
  const auto ref_map(function_vv_info.LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(tt_ref, ref_map, "tt");
  const ReferenceComponent &tt_ref_comp(tt_ref->components->Value());
  EXPECT_EQ(tt_ref_comp.required_metatype, SymbolMetaType::kCallable);
  EXPECT_EQ(tt_ref_comp.resolved_symbol, nullptr);

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // Call to "tt" is resolved.
    EXPECT_EQ(tt_ref_comp.resolved_symbol, &function_tt);
  }
}

TEST(BuildSymbolTableTest, FunctionCallUnresolved) {
  TestVerilogSourceFile src("call_me_not.sv",
                            "function int vv();\n"
                            "  return tt();\n"  // undefined
                            "endfunction\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(function_vv, root_symbol, "vv");
  EXPECT_EQ(function_vv_info.metatype, SymbolMetaType::kFunction);
  ASSERT_NE(function_vv_info.declared_type.syntax_origin, nullptr);

  EXPECT_EQ(function_vv_info.local_references_to_bind.size(), 1);
  const auto ref_map(function_vv_info.LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(tt_ref, ref_map, "tt");
  const ReferenceComponent &tt_ref_comp(tt_ref->components->Value());
  EXPECT_EQ(tt_ref_comp.required_metatype, SymbolMetaType::kCallable);
  EXPECT_EQ(tt_ref_comp.resolved_symbol, nullptr);

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    ASSIGN_MUST_HAVE_UNIQUE(err, resolve_diagnostics);
    EXPECT_EQ(err.code(), absl::StatusCode::kNotFound);
    EXPECT_THAT(
        err.message(),
        HasSubstr("Unable to resolve symbol \"tt\" from context $root::vv"));

    // Call to "tt" is unresolved.
    EXPECT_EQ(tt_ref_comp.resolved_symbol, nullptr);
  }
}

TEST(BuildSymbolTableTest, FunctionCallUnresolvedNamedParameters) {
  TestVerilogSourceFile src("call_me_not.sv",
                            "function int vv();\n"
                            "  return tt(.a(1), .b(2));\n"  // undefined
                            "endfunction\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(function_vv, root_symbol, "vv");
  EXPECT_EQ(function_vv_info.metatype, SymbolMetaType::kFunction);
  ASSERT_NE(function_vv_info.declared_type.syntax_origin, nullptr);

  EXPECT_EQ(function_vv_info.local_references_to_bind.size(), 1);
  const auto ref_map(function_vv_info.LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(tt_ref, ref_map, "tt");
  const ReferenceComponent &tt_ref_comp(tt_ref->components->Value());
  EXPECT_EQ(tt_ref_comp.required_metatype, SymbolMetaType::kCallable);
  EXPECT_EQ(tt_ref_comp.resolved_symbol, nullptr);

  const ReferenceComponentMap param_refs(
      ReferenceComponentNodeMapView(*tt_ref->components));

  ASSIGN_MUST_FIND(a_ref, param_refs, "a");
  const ReferenceComponent &a_ref_comp(a_ref->Value());
  EXPECT_EQ(a_ref_comp.identifier, "a");
  EXPECT_EQ(a_ref_comp.ref_type, ReferenceType::kDirectMember);
  EXPECT_EQ(a_ref_comp.required_metatype,
            SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(a_ref_comp.resolved_symbol, nullptr);  // not yet resolved

  ASSIGN_MUST_FIND(b_ref, param_refs, "b");
  const ReferenceComponent &b_ref_comp(b_ref->Value());
  EXPECT_EQ(b_ref_comp.identifier, "b");
  EXPECT_EQ(b_ref_comp.ref_type, ReferenceType::kDirectMember);
  EXPECT_EQ(b_ref_comp.required_metatype,
            SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(b_ref_comp.resolved_symbol, nullptr);  // not yet resolved

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    ASSIGN_MUST_HAVE_UNIQUE(err, resolve_diagnostics);
    EXPECT_EQ(err.code(), absl::StatusCode::kNotFound);
    EXPECT_THAT(
        err.message(),
        HasSubstr("Unable to resolve symbol \"tt\" from context $root::vv"));

    // Call to "tt" is unresolved, as are its named parameters.
    EXPECT_EQ(tt_ref_comp.resolved_symbol, nullptr);
    EXPECT_EQ(a_ref_comp.resolved_symbol, nullptr);  // not yet resolved
    EXPECT_EQ(b_ref_comp.resolved_symbol, nullptr);  // not yet resolved
  }
}

TEST(BuildSymbolTableTest, FunctionCallResolvedNamedParameters) {
  TestVerilogSourceFile src("call_me_not.sv",
                            "function int tt(int a, int b);\n"
                            "  return 0;\n"
                            "endfunction\n"
                            "function int vv();\n"
                            "  return tt(.a(1), .b(2));\n"  // valid
                            "endfunction\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(function_tt, root_symbol, "tt");
  EXPECT_EQ(function_tt_info.metatype, SymbolMetaType::kFunction);
  ASSERT_NE(function_tt_info.declared_type.syntax_origin,
            nullptr);  // returns int

  MUST_ASSIGN_LOOKUP_SYMBOL(param_a, function_tt, "a");
  EXPECT_EQ(param_a_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  ASSERT_NE(param_a_info.declared_type.syntax_origin, nullptr);  // int a

  MUST_ASSIGN_LOOKUP_SYMBOL(param_b, function_tt, "b");
  EXPECT_EQ(param_b_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  ASSERT_NE(param_b_info.declared_type.syntax_origin, nullptr);  // int b

  MUST_ASSIGN_LOOKUP_SYMBOL(function_vv, root_symbol, "vv");
  EXPECT_EQ(function_vv_info.metatype, SymbolMetaType::kFunction);
  ASSERT_NE(function_vv_info.declared_type.syntax_origin, nullptr);

  EXPECT_EQ(function_vv_info.local_references_to_bind.size(), 1);
  const auto ref_map(function_vv_info.LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(tt_ref, ref_map, "tt");
  const ReferenceComponent &tt_ref_comp(tt_ref->components->Value());
  EXPECT_EQ(tt_ref_comp.required_metatype, SymbolMetaType::kCallable);
  EXPECT_EQ(tt_ref_comp.resolved_symbol, nullptr);

  const ReferenceComponentMap param_refs(
      ReferenceComponentNodeMapView(*tt_ref->components));

  ASSIGN_MUST_FIND(a_ref, param_refs, "a");
  const ReferenceComponent &a_ref_comp(a_ref->Value());
  EXPECT_EQ(a_ref_comp.identifier, "a");
  EXPECT_EQ(a_ref_comp.ref_type, ReferenceType::kDirectMember);
  EXPECT_EQ(a_ref_comp.required_metatype,
            SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(a_ref_comp.resolved_symbol, nullptr);  // not yet resolved

  ASSIGN_MUST_FIND(b_ref, param_refs, "b");
  const ReferenceComponent &b_ref_comp(b_ref->Value());
  EXPECT_EQ(b_ref_comp.identifier, "b");
  EXPECT_EQ(b_ref_comp.ref_type, ReferenceType::kDirectMember);
  EXPECT_EQ(b_ref_comp.required_metatype,
            SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(b_ref_comp.resolved_symbol, nullptr);  // not yet resolved

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // Call to "tt" is resolved, along with its named parameters.
    EXPECT_EQ(tt_ref_comp.resolved_symbol, &function_tt);
    EXPECT_EQ(a_ref_comp.resolved_symbol, &param_a);
    EXPECT_EQ(b_ref_comp.resolved_symbol, &param_b);
  }
}

TEST(BuildSymbolTableTest, CallNonFunction) {
  TestVerilogSourceFile src("call_me.sv",
                            "module tt();\n"
                            "endmodule\n"
                            "function int vv();\n"
                            "  return tt();\n"  // not a function
                            "endfunction\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(module_tt, root_symbol, "tt");
  MUST_ASSIGN_LOOKUP_SYMBOL(function_vv, root_symbol, "vv");
  EXPECT_EQ(module_tt_info.metatype, SymbolMetaType::kModule);
  EXPECT_EQ(module_tt_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(function_vv_info.metatype, SymbolMetaType::kFunction);
  EXPECT_NE(function_vv_info.declared_type.syntax_origin, nullptr);

  EXPECT_EQ(function_vv_info.local_references_to_bind.size(), 1);
  const auto ref_map(function_vv_info.LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(tt_ref, ref_map, "tt");
  const ReferenceComponent &tt_ref_comp(tt_ref->components->Value());
  EXPECT_EQ(tt_ref_comp.required_metatype, SymbolMetaType::kCallable);
  EXPECT_EQ(tt_ref_comp.resolved_symbol, nullptr);

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    ASSIGN_MUST_HAVE_UNIQUE(err, resolve_diagnostics);
    EXPECT_EQ(err.code(), absl::StatusCode::kInvalidArgument);
    EXPECT_THAT(err.message(),
                HasSubstr("Expecting reference \"tt\" to resolve to a "
                          "<callable>, but found a module"));

    EXPECT_EQ(tt_ref_comp.resolved_symbol, nullptr);
  }
}

TEST(BuildSymbolTableTest, NestedCallsArguments) {
  TestVerilogSourceFile src("call_me.sv",
                            "function int tt(int aa);\n"
                            "  return aa + 1;\n"
                            "endfunction\n"
                            "function int vv();\n"
                            "  return tt(tt(tt(2)));\n"
                            "endfunction\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(function_tt, root_symbol, "tt");
  MUST_ASSIGN_LOOKUP_SYMBOL(function_vv, root_symbol, "vv");
  EXPECT_EQ(function_tt_info.metatype, SymbolMetaType::kFunction);
  ASSERT_NE(function_tt_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(function_vv_info.metatype, SymbolMetaType::kFunction);
  ASSERT_NE(function_vv_info.declared_type.syntax_origin, nullptr);

  MUST_ASSIGN_LOOKUP_SYMBOL(arg_aa, function_tt, "aa");
  EXPECT_EQ(arg_aa_info.metatype, SymbolMetaType::kDataNetVariableInstance);

  EXPECT_EQ(function_tt_info.local_references_to_bind.size(), 1);
  const auto tt_ref_map(function_tt_info.LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(aa_ref, tt_ref_map, "aa");
  const ReferenceComponent &aa_ref_comp(aa_ref->components->Value());
  EXPECT_EQ(aa_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(aa_ref_comp.resolved_symbol, nullptr);

  // Expect 3 calls to "tt" from the same scope.
  EXPECT_EQ(function_vv_info.local_references_to_bind.size(), 3);
  const auto vv_ref_map(function_vv_info.LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND(tt_refs, vv_ref_map, "tt");
  for (const auto &tt_ref : tt_refs) {
    const ReferenceComponent &tt_ref_comp(tt_ref->components->Value());
    EXPECT_EQ(tt_ref_comp.required_metatype, SymbolMetaType::kCallable);
    EXPECT_EQ(tt_ref_comp.resolved_symbol, nullptr);
  }

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    EXPECT_EQ(aa_ref_comp.resolved_symbol, &arg_aa);

    // Calls to "tt" are all resolved.
    for (const auto &tt_ref : tt_refs) {
      const ReferenceComponent &tt_ref_comp(tt_ref->components->Value());
      EXPECT_EQ(tt_ref_comp.resolved_symbol, &function_tt);
    }
  }
}

TEST(BuildSymbolTableTest, SelfRecursion) {
  TestVerilogSourceFile src("call_me_from_me.sv",
                            "function int tt();\n"
                            "  return 1 - tt();\n"
                            "endfunction\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(function_tt, root_symbol, "tt");
  EXPECT_EQ(function_tt_info.metatype, SymbolMetaType::kFunction);
  ASSERT_NE(function_tt_info.declared_type.syntax_origin, nullptr);

  EXPECT_EQ(function_tt_info.local_references_to_bind.size(), 1);
  const auto tt_ref_map(function_tt_info.LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(tt_ref, tt_ref_map, "tt");
  const ReferenceComponent &tt_ref_comp(tt_ref->components->Value());
  EXPECT_EQ(tt_ref_comp.required_metatype, SymbolMetaType::kCallable);
  EXPECT_EQ(tt_ref_comp.resolved_symbol, nullptr);

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // Call to "tt" (recursive) is resolved.
    EXPECT_EQ(tt_ref_comp.resolved_symbol, &function_tt);
  }
}

TEST(BuildSymbolTableTest, MutualRecursion) {
  TestVerilogSourceFile src("call_me_back.sv",
                            "function int tt();\n"
                            "  return vv();\n"
                            "endfunction\n"
                            "function int vv();\n"
                            "  return tt();\n"
                            "endfunction\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(function_tt, root_symbol, "tt");
  MUST_ASSIGN_LOOKUP_SYMBOL(function_vv, root_symbol, "vv");
  EXPECT_EQ(function_tt_info.metatype, SymbolMetaType::kFunction);
  ASSERT_NE(function_tt_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(function_vv_info.metatype, SymbolMetaType::kFunction);
  ASSERT_NE(function_vv_info.declared_type.syntax_origin, nullptr);

  EXPECT_EQ(function_tt_info.local_references_to_bind.size(), 1);
  const auto tt_ref_map(function_tt_info.LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(vv_ref, tt_ref_map, "vv");
  const ReferenceComponent &vv_ref_comp(vv_ref->components->Value());
  EXPECT_EQ(vv_ref_comp.required_metatype, SymbolMetaType::kCallable);
  EXPECT_EQ(vv_ref_comp.resolved_symbol, nullptr);

  EXPECT_EQ(function_vv_info.local_references_to_bind.size(), 1);
  const auto vv_ref_map(function_vv_info.LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(tt_ref, vv_ref_map, "tt");
  const ReferenceComponent &tt_ref_comp(tt_ref->components->Value());
  EXPECT_EQ(tt_ref_comp.required_metatype, SymbolMetaType::kCallable);
  EXPECT_EQ(tt_ref_comp.resolved_symbol, nullptr);

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // Calls to "tt" and "vv" are all resolved.
    EXPECT_EQ(vv_ref_comp.resolved_symbol, &function_vv);
    EXPECT_EQ(tt_ref_comp.resolved_symbol, &function_tt);
  }
}

TEST(BuildSymbolTableTest, PackageQualifiedFunctionCall) {
  TestVerilogSourceFile src("call_me.sv",
                            "package pp;\n"
                            "  function int tt();\n"
                            "    return 1;\n"
                            "  endfunction\n"
                            "endpackage\n"
                            "function int vv();\n"
                            "  return pp::tt();\n"
                            "endfunction\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(package_pp, root_symbol, "pp");
  MUST_ASSIGN_LOOKUP_SYMBOL(function_tt, package_pp, "tt");
  EXPECT_EQ(package_pp_info.metatype, SymbolMetaType::kPackage);
  MUST_ASSIGN_LOOKUP_SYMBOL(function_vv, root_symbol, "vv");
  EXPECT_EQ(function_tt_info.metatype, SymbolMetaType::kFunction);
  ASSERT_NE(function_tt_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(function_vv_info.metatype, SymbolMetaType::kFunction);
  ASSERT_NE(function_vv_info.declared_type.syntax_origin, nullptr);

  EXPECT_EQ(function_vv_info.local_references_to_bind.size(), 1);
  const auto ref_map(function_vv_info.LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(pp_ref, ref_map, "pp");
  ASSERT_EQ(pp_ref->components->Children().size(), 1);
  const ReferenceComponent &pp_ref_comp(pp_ref->components->Value());
  EXPECT_EQ(pp_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(pp_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(pp_ref_comp.resolved_symbol, nullptr);

  const ReferenceComponent &tt_ref_comp(
      pp_ref->components->Children().front().Value());
  EXPECT_EQ(tt_ref_comp.identifier, "tt");
  EXPECT_EQ(tt_ref_comp.ref_type, ReferenceType::kDirectMember);
  EXPECT_EQ(tt_ref_comp.required_metatype, SymbolMetaType::kCallable);
  EXPECT_EQ(tt_ref_comp.resolved_symbol, nullptr);

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // Call to "tt" is resolved.
    EXPECT_EQ(pp_ref_comp.resolved_symbol, &package_pp);
    EXPECT_EQ(tt_ref_comp.resolved_symbol, &function_tt);
  }
}

TEST(BuildSymbolTableTest, ClassQualifiedFunctionCall) {
  TestVerilogSourceFile src("call_me.sv",
                            "class cc;\n"
                            "  function static int tt();\n"
                            "    return 1;\n"
                            "  endfunction\n"
                            "endclass\n"
                            "function int vv();\n"
                            "  return cc::tt();\n"
                            "endfunction\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(class_cc, root_symbol, "cc");
  MUST_ASSIGN_LOOKUP_SYMBOL(function_tt, class_cc, "tt");
  EXPECT_EQ(class_cc_info.metatype, SymbolMetaType::kClass);
  MUST_ASSIGN_LOOKUP_SYMBOL(function_vv, root_symbol, "vv");
  EXPECT_EQ(function_tt_info.metatype, SymbolMetaType::kFunction);
  ASSERT_NE(function_tt_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(function_vv_info.metatype, SymbolMetaType::kFunction);
  ASSERT_NE(function_vv_info.declared_type.syntax_origin, nullptr);

  EXPECT_EQ(function_vv_info.local_references_to_bind.size(), 1);
  const auto ref_map(function_vv_info.LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(cc_ref, ref_map, "cc");
  ASSERT_EQ(cc_ref->components->Children().size(), 1);
  const ReferenceComponent &cc_ref_comp(cc_ref->components->Value());
  EXPECT_EQ(cc_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(cc_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(cc_ref_comp.resolved_symbol, nullptr);

  const ReferenceComponent &tt_ref_comp(
      cc_ref->components->Children().front().Value());
  EXPECT_EQ(tt_ref_comp.identifier, "tt");
  EXPECT_EQ(tt_ref_comp.ref_type, ReferenceType::kDirectMember);
  EXPECT_EQ(tt_ref_comp.required_metatype, SymbolMetaType::kCallable);
  EXPECT_EQ(tt_ref_comp.resolved_symbol, nullptr);

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // Call to "tt" is resolved.
    EXPECT_EQ(cc_ref_comp.resolved_symbol, &class_cc);
    EXPECT_EQ(tt_ref_comp.resolved_symbol, &function_tt);
  }
}

TEST(BuildSymbolTableTest, ClassQualifiedFunctionCallUnresolved) {
  TestVerilogSourceFile src("call_me.sv",
                            "class cc;\n"
                            "endclass\n"
                            "function int vv();\n"
                            "  return cc::tt();\n"
                            "endfunction\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(class_cc, root_symbol, "cc");
  EXPECT_EQ(class_cc_info.metatype, SymbolMetaType::kClass);
  MUST_ASSIGN_LOOKUP_SYMBOL(function_vv, root_symbol, "vv");
  EXPECT_EQ(function_vv_info.metatype, SymbolMetaType::kFunction);
  ASSERT_NE(function_vv_info.declared_type.syntax_origin, nullptr);

  EXPECT_EQ(function_vv_info.local_references_to_bind.size(), 1);
  const auto ref_map(function_vv_info.LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(cc_ref, ref_map, "cc");
  ASSERT_EQ(cc_ref->components->Children().size(), 1);
  const ReferenceComponent &cc_ref_comp(cc_ref->components->Value());
  EXPECT_EQ(cc_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(cc_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(cc_ref_comp.resolved_symbol, nullptr);

  const ReferenceComponent &tt_ref_comp(
      cc_ref->components->Children().front().Value());
  EXPECT_EQ(tt_ref_comp.identifier, "tt");
  EXPECT_EQ(tt_ref_comp.ref_type, ReferenceType::kDirectMember);
  EXPECT_EQ(tt_ref_comp.required_metatype, SymbolMetaType::kCallable);
  EXPECT_EQ(tt_ref_comp.resolved_symbol, nullptr);

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    ASSIGN_MUST_HAVE_UNIQUE(err, resolve_diagnostics);
    EXPECT_EQ(err.code(), absl::StatusCode::kNotFound);

    EXPECT_EQ(cc_ref_comp.resolved_symbol, &class_cc);
    EXPECT_EQ(tt_ref_comp.resolved_symbol, nullptr);  // error
  }
}

TEST(BuildSymbolTableTest, ClassMethodCall) {
  TestVerilogSourceFile src("call_me.sv",
                            "class cc;\n"
                            "  function int tt();\n"
                            "    return 1;\n"
                            "  endfunction\n"
                            "endclass\n"
                            "function int vv();\n"
                            "  cc cc_obj;\n"
                            "  return cc_obj.tt();\n"
                            "endfunction\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(class_cc, root_symbol, "cc");
  MUST_ASSIGN_LOOKUP_SYMBOL(function_tt, class_cc, "tt");
  EXPECT_EQ(class_cc_info.metatype, SymbolMetaType::kClass);
  MUST_ASSIGN_LOOKUP_SYMBOL(function_vv, root_symbol, "vv");
  EXPECT_EQ(function_tt_info.metatype, SymbolMetaType::kFunction);
  ASSERT_NE(function_tt_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(function_vv_info.metatype, SymbolMetaType::kFunction);
  ASSERT_NE(function_vv_info.declared_type.syntax_origin, nullptr);
  MUST_ASSIGN_LOOKUP_SYMBOL(cc_obj, function_vv, "cc_obj");
  EXPECT_EQ(cc_obj_info.metatype, SymbolMetaType::kDataNetVariableInstance);

  EXPECT_EQ(function_vv_info.local_references_to_bind.size(), 2);
  const auto ref_map(function_vv_info.LocalReferencesMapViewForTesting());

  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(cc_type_ref, ref_map,
                                   "cc");  // "cc" is a type
  const ReferenceComponent &cc_type_ref_comp(cc_type_ref->components->Value());

  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(cc_obj_ref, ref_map,
                                   "cc_obj");  // "cc_obj" is data
  ASSERT_EQ(cc_obj_ref->components->Children().size(), 1);
  const ReferenceComponent &cc_obj_ref_comp(cc_obj_ref->components->Value());
  EXPECT_EQ(cc_obj_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(cc_obj_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(cc_obj_ref_comp.resolved_symbol, nullptr);

  const ReferenceComponent &tt_ref_comp(
      cc_obj_ref->components->Children().front().Value());
  EXPECT_EQ(tt_ref_comp.identifier, "tt");
  EXPECT_EQ(tt_ref_comp.ref_type, ReferenceType::kMemberOfTypeOfParent);
  EXPECT_EQ(tt_ref_comp.required_metatype, SymbolMetaType::kCallable);
  EXPECT_EQ(tt_ref_comp.resolved_symbol, nullptr);

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // Call to ".tt" is resolved.
    EXPECT_EQ(cc_type_ref_comp.resolved_symbol, &class_cc);
    EXPECT_EQ(cc_obj_ref_comp.resolved_symbol, &cc_obj);
    EXPECT_EQ(tt_ref_comp.resolved_symbol, &function_tt);
  }
}

TEST(BuildSymbolTableTest, ClassMethodCallUnresolved) {
  TestVerilogSourceFile src("call_me.sv",
                            "class cc;\n"
                            "endclass\n"
                            "function int vv();\n"
                            "  cc cc_obj;\n"
                            "  return cc_obj.tt();\n"
                            "endfunction\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(class_cc, root_symbol, "cc");
  EXPECT_EQ(class_cc_info.metatype, SymbolMetaType::kClass);
  MUST_ASSIGN_LOOKUP_SYMBOL(function_vv, root_symbol, "vv");
  EXPECT_EQ(function_vv_info.metatype, SymbolMetaType::kFunction);
  ASSERT_NE(function_vv_info.declared_type.syntax_origin, nullptr);
  MUST_ASSIGN_LOOKUP_SYMBOL(cc_obj, function_vv, "cc_obj");
  EXPECT_EQ(cc_obj_info.metatype, SymbolMetaType::kDataNetVariableInstance);

  EXPECT_EQ(function_vv_info.local_references_to_bind.size(), 2);
  const auto ref_map(function_vv_info.LocalReferencesMapViewForTesting());

  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(cc_type_ref, ref_map,
                                   "cc");  // "cc" is a type
  const ReferenceComponent &cc_type_ref_comp(cc_type_ref->components->Value());

  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(cc_obj_ref, ref_map,
                                   "cc_obj");  // "cc_obj" is data
  ASSERT_EQ(cc_obj_ref->components->Children().size(), 1);
  const ReferenceComponent &cc_obj_ref_comp(cc_obj_ref->components->Value());
  EXPECT_EQ(cc_obj_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(cc_obj_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(cc_obj_ref_comp.resolved_symbol, nullptr);

  const ReferenceComponent &tt_ref_comp(
      cc_obj_ref->components->Children().front().Value());
  EXPECT_EQ(tt_ref_comp.identifier, "tt");
  EXPECT_EQ(tt_ref_comp.ref_type, ReferenceType::kMemberOfTypeOfParent);
  EXPECT_EQ(tt_ref_comp.required_metatype, SymbolMetaType::kCallable);
  EXPECT_EQ(tt_ref_comp.resolved_symbol, nullptr);

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    ASSIGN_MUST_HAVE_UNIQUE(err, resolve_diagnostics);
    EXPECT_EQ(err.code(), absl::StatusCode::kNotFound);

    EXPECT_EQ(cc_type_ref_comp.resolved_symbol, &class_cc);
    EXPECT_EQ(cc_obj_ref_comp.resolved_symbol, &cc_obj);
    EXPECT_EQ(tt_ref_comp.resolved_symbol, nullptr);  // unresolved
  }
}

TEST(BuildSymbolTableTest, ChainedMethodCall) {
  TestVerilogSourceFile src("call_me.sv",
                            "class cc;\n"
                            "  function dd tt();\n"
                            "  endfunction\n"
                            "endclass\n"
                            "class dd;\n"
                            "  function cc gg();\n"
                            "  endfunction\n"
                            "endclass\n"
                            "function dd vv();\n"
                            "  dd dd_obj;\n"
                            "  return dd_obj.gg().tt();\n"
                            // .gg() -> cc
                            // .tt() -> dd
                            "endfunction\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(class_cc, root_symbol, "cc");
  EXPECT_EQ(class_cc_info.metatype, SymbolMetaType::kClass);

  MUST_ASSIGN_LOOKUP_SYMBOL(class_dd, root_symbol, "dd");
  EXPECT_EQ(class_dd_info.metatype, SymbolMetaType::kClass);

  MUST_ASSIGN_LOOKUP_SYMBOL(function_tt, class_cc, "tt");
  EXPECT_EQ(function_tt_info.metatype, SymbolMetaType::kFunction);
  ASSERT_NE(function_tt_info.declared_type.syntax_origin, nullptr);
  ASSERT_NE(function_tt_info.declared_type.user_defined_type, nullptr);

  MUST_ASSIGN_LOOKUP_SYMBOL(function_gg, class_dd, "gg");
  EXPECT_EQ(function_gg_info.metatype, SymbolMetaType::kFunction);
  ASSERT_NE(function_gg_info.declared_type.syntax_origin, nullptr);
  ASSERT_NE(function_gg_info.declared_type.user_defined_type, nullptr);

  MUST_ASSIGN_LOOKUP_SYMBOL(function_vv, root_symbol, "vv");
  EXPECT_EQ(function_vv_info.metatype, SymbolMetaType::kFunction);
  ASSERT_NE(function_vv_info.declared_type.syntax_origin, nullptr);

  MUST_ASSIGN_LOOKUP_SYMBOL(dd_obj, function_vv, "dd_obj");
  EXPECT_EQ(dd_obj_info.metatype, SymbolMetaType::kDataNetVariableInstance);

  EXPECT_EQ(function_vv_info.local_references_to_bind.size(), 2);
  const auto ref_map(function_vv_info.LocalReferencesMapViewForTesting());

  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(dd_type_ref, ref_map,
                                   "dd");  // "dd" is a type
  const ReferenceComponent &dd_type_ref_comp(dd_type_ref->components->Value());

  // Examine the dd_obj.gg().tt() reference chain.
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(dd_obj_ref, ref_map,
                                   "dd_obj");  // "dd_obj" is data
  ASSERT_EQ(dd_obj_ref->components->Children().size(), 1);
  const ReferenceComponent &dd_obj_ref_comp(dd_obj_ref->components->Value());
  EXPECT_EQ(dd_obj_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(dd_obj_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(dd_obj_ref_comp.resolved_symbol, nullptr);

  const ReferenceComponentNode &dd_gg_ref(
      dd_obj_ref->components->Children().front());
  const ReferenceComponent &dd_gg_ref_comp(dd_gg_ref.Value());
  EXPECT_EQ(dd_gg_ref_comp.identifier, "gg");
  EXPECT_EQ(dd_gg_ref_comp.ref_type, ReferenceType::kMemberOfTypeOfParent);
  EXPECT_EQ(dd_gg_ref_comp.required_metatype, SymbolMetaType::kCallable);
  EXPECT_EQ(dd_gg_ref_comp.resolved_symbol, nullptr);

  const ReferenceComponentNode &dd_gg_tt_ref(dd_gg_ref.Children().front());
  const ReferenceComponent &dd_gg_tt_ref_comp(dd_gg_tt_ref.Value());
  EXPECT_EQ(dd_gg_tt_ref_comp.identifier, "tt");
  EXPECT_EQ(dd_gg_tt_ref_comp.ref_type, ReferenceType::kMemberOfTypeOfParent);
  EXPECT_EQ(dd_gg_tt_ref_comp.required_metatype, SymbolMetaType::kCallable);
  EXPECT_EQ(dd_gg_tt_ref_comp.resolved_symbol, nullptr);

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // Return types of methods are resolved.
    EXPECT_EQ(function_tt_info.declared_type.user_defined_type->Value()
                  .resolved_symbol,
              &class_dd);
    EXPECT_EQ(function_gg_info.declared_type.user_defined_type->Value()
                  .resolved_symbol,
              &class_cc);

    // Chained call is resolved.
    EXPECT_EQ(dd_type_ref_comp.resolved_symbol, &class_dd);
    EXPECT_EQ(dd_obj_ref_comp.resolved_symbol, &dd_obj);
    EXPECT_EQ(dd_gg_ref_comp.resolved_symbol, &function_gg);
    EXPECT_EQ(dd_gg_tt_ref_comp.resolved_symbol, &function_tt);
  }
}

TEST(BuildSymbolTableTest, ChainedMethodCallReturnTypeNotAClass) {
  TestVerilogSourceFile src(
      "call_me.sv",
      "class cc;\n"
      "  function dd tt();\n"
      "  endfunction\n"
      "endclass\n"
      "class dd;\n"
      "  function int gg();\n"  // return type is a primitive
      "  endfunction\n"
      "endclass\n"
      "function dd vv();\n"
      "  dd dd_obj;\n"
      "  return dd_obj.gg().tt();\n"
      // .gg() -> cc
      // .tt() -> <error>
      "endfunction\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(class_cc, root_symbol, "cc");
  EXPECT_EQ(class_cc_info.metatype, SymbolMetaType::kClass);

  MUST_ASSIGN_LOOKUP_SYMBOL(class_dd, root_symbol, "dd");
  EXPECT_EQ(class_dd_info.metatype, SymbolMetaType::kClass);

  MUST_ASSIGN_LOOKUP_SYMBOL(function_tt, class_cc, "tt");
  EXPECT_EQ(function_tt_info.metatype, SymbolMetaType::kFunction);
  ASSERT_NE(function_tt_info.declared_type.syntax_origin, nullptr);
  ASSERT_NE(function_tt_info.declared_type.user_defined_type, nullptr);

  MUST_ASSIGN_LOOKUP_SYMBOL(function_gg, class_dd, "gg");
  EXPECT_EQ(function_gg_info.metatype, SymbolMetaType::kFunction);
  ASSERT_NE(function_gg_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(function_gg_info.declared_type.user_defined_type, nullptr);

  MUST_ASSIGN_LOOKUP_SYMBOL(function_vv, root_symbol, "vv");
  EXPECT_EQ(function_vv_info.metatype, SymbolMetaType::kFunction);
  ASSERT_NE(function_vv_info.declared_type.syntax_origin, nullptr);

  MUST_ASSIGN_LOOKUP_SYMBOL(dd_obj, function_vv, "dd_obj");
  EXPECT_EQ(dd_obj_info.metatype, SymbolMetaType::kDataNetVariableInstance);

  EXPECT_EQ(function_vv_info.local_references_to_bind.size(), 2);
  const auto ref_map(function_vv_info.LocalReferencesMapViewForTesting());

  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(dd_type_ref, ref_map,
                                   "dd");  // "dd" is a type
  const ReferenceComponent &dd_type_ref_comp(dd_type_ref->components->Value());

  // Examine the dd_obj.gg().tt() reference chain.
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(dd_obj_ref, ref_map,
                                   "dd_obj");  // "dd_obj" is data
  ASSERT_EQ(dd_obj_ref->components->Children().size(), 1);
  const ReferenceComponent &dd_obj_ref_comp(dd_obj_ref->components->Value());
  EXPECT_EQ(dd_obj_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(dd_obj_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(dd_obj_ref_comp.resolved_symbol, nullptr);

  const ReferenceComponentNode &dd_gg_ref(
      dd_obj_ref->components->Children().front());
  const ReferenceComponent &dd_gg_ref_comp(dd_gg_ref.Value());
  EXPECT_EQ(dd_gg_ref_comp.identifier, "gg");
  EXPECT_EQ(dd_gg_ref_comp.ref_type, ReferenceType::kMemberOfTypeOfParent);
  EXPECT_EQ(dd_gg_ref_comp.required_metatype, SymbolMetaType::kCallable);
  EXPECT_EQ(dd_gg_ref_comp.resolved_symbol, nullptr);

  const ReferenceComponentNode &dd_gg_tt_ref(dd_gg_ref.Children().front());
  const ReferenceComponent &dd_gg_tt_ref_comp(dd_gg_tt_ref.Value());
  EXPECT_EQ(dd_gg_tt_ref_comp.identifier, "tt");
  EXPECT_EQ(dd_gg_tt_ref_comp.ref_type, ReferenceType::kMemberOfTypeOfParent);
  EXPECT_EQ(dd_gg_tt_ref_comp.required_metatype, SymbolMetaType::kCallable);
  EXPECT_EQ(dd_gg_tt_ref_comp.resolved_symbol, nullptr);

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    ASSIGN_MUST_HAVE_UNIQUE(err, resolve_diagnostics);
    EXPECT_EQ(err.code(), absl::StatusCode::kInvalidArgument);
    EXPECT_THAT(err.message(), HasSubstr("Type of parent reference"));
    // reference text in diagnostic looks like: "@dd_obj.gg[<callable>]"
    EXPECT_THAT(err.message(), HasSubstr("(int) does not have any members"));

    // Return types of methods are resolved (where non-primitive).
    EXPECT_EQ(function_tt_info.declared_type.user_defined_type->Value()
                  .resolved_symbol,
              &class_dd);

    // Chained call is partially resolved.
    EXPECT_EQ(dd_type_ref_comp.resolved_symbol, &class_dd);
    EXPECT_EQ(dd_obj_ref_comp.resolved_symbol, &dd_obj);
    EXPECT_EQ(dd_gg_ref_comp.resolved_symbol, &function_gg);
    EXPECT_EQ(dd_gg_tt_ref_comp.resolved_symbol, nullptr);  // failed to resolve
  }
}

TEST(BuildSymbolTableTest, AnonymousStructTypeData) {
  TestVerilogSourceFile src("structy.sv",
                            "struct {\n"
                            "  int size;\n"
                            "} data;\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  // Expect one anonymous struct definition and reference.
  EXPECT_EQ(root_symbol.Value().anonymous_scope_names.size(), 1);
  ASSERT_EQ(root_symbol.Children().size(), 2);
  // Find the symbol that is a struct (anon), which is not "data".
  const auto found =
      std::find_if(root_symbol.Children().begin(), root_symbol.Children().end(),
                   [](const SymbolTableNode::key_value_type &p) {
                     return p.first != "data";
                   });
  ASSERT_NE(found, root_symbol.Children().end());
  const SymbolTableNode &anon_struct(found->second);
  const SymbolInfo &anon_struct_info(anon_struct.Value());
  EXPECT_EQ(anon_struct_info.metatype, SymbolMetaType::kStruct);
  EXPECT_TRUE(anon_struct_info.local_references_to_bind.empty());

  // Struct has one member.
  MUST_ASSIGN_LOOKUP_SYMBOL(int_size, anon_struct, "size");
  EXPECT_EQ(int_size_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(int_size_info.file_origin, &src);
  EXPECT_EQ(
      verible::StringSpanOfSymbol(*int_size_info.declared_type.syntax_origin),
      "int");
  EXPECT_EQ(int_size_info.declared_type.user_defined_type, nullptr);

  // Expect to bind anonymous struct immediately.
  ASSERT_EQ(root_symbol.Value().local_references_to_bind.size(), 1);
  const DependentReferences &anon_struct_ref(
      root_symbol.Value().local_references_to_bind.front());
  const ReferenceComponent &anon_struct_ref_comp(
      anon_struct_ref.components->Value());
  EXPECT_EQ(anon_struct_ref_comp.ref_type, ReferenceType::kImmediate);
  EXPECT_EQ(anon_struct_ref_comp.required_metatype, SymbolMetaType::kStruct);
  EXPECT_EQ(anon_struct_ref_comp.resolved_symbol, &anon_struct);

  // "data"'s type is the (internal) anonymous struct type reference.
  MUST_ASSIGN_LOOKUP_SYMBOL(data, root_symbol, "data");
  EXPECT_EQ(data_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(data_info.file_origin, &src);
  EXPECT_EQ(data_info.declared_type.user_defined_type,
            anon_struct_ref.LastLeaf());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    EXPECT_EQ(anon_struct_ref_comp.resolved_symbol, &anon_struct);  // unchanged
  }
}

TEST(BuildSymbolTableTest, AnonymousStructTypeDataMultiFields) {
  TestVerilogSourceFile src("structy.sv",
                            "struct {\n"
                            "  int size;\n"
                            "  real weight;\n"
                            "} data;\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  // Expect one anonymous struct definition and reference.
  EXPECT_EQ(root_symbol.Value().anonymous_scope_names.size(), 1);
  ASSERT_EQ(root_symbol.Children().size(), 2);
  // Find the symbol that is a struct (anon), which is not "data".
  const auto found =
      std::find_if(root_symbol.Children().begin(), root_symbol.Children().end(),
                   [](const SymbolTableNode::key_value_type &p) {
                     return p.first != "data";
                   });
  ASSERT_NE(found, root_symbol.Children().end());
  const SymbolTableNode &anon_struct(found->second);
  const SymbolInfo &anon_struct_info(anon_struct.Value());
  EXPECT_EQ(anon_struct_info.metatype, SymbolMetaType::kStruct);
  EXPECT_TRUE(anon_struct_info.local_references_to_bind.empty());

  // Struct has two members.
  MUST_ASSIGN_LOOKUP_SYMBOL(int_size, anon_struct, "size");
  EXPECT_EQ(int_size_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(int_size_info.file_origin, &src);
  EXPECT_EQ(
      verible::StringSpanOfSymbol(*int_size_info.declared_type.syntax_origin),
      "int");
  EXPECT_EQ(int_size_info.declared_type.user_defined_type, nullptr);

  MUST_ASSIGN_LOOKUP_SYMBOL(int_weight, anon_struct, "weight");
  EXPECT_EQ(int_weight_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(int_weight_info.file_origin, &src);
  EXPECT_EQ(
      verible::StringSpanOfSymbol(*int_weight_info.declared_type.syntax_origin),
      "real");
  EXPECT_EQ(int_weight_info.declared_type.user_defined_type, nullptr);

  // Expect to bind anonymous struct immediately.
  ASSERT_EQ(root_symbol.Value().local_references_to_bind.size(), 1);
  const DependentReferences &anon_struct_ref(
      root_symbol.Value().local_references_to_bind.front());
  const ReferenceComponent &anon_struct_ref_comp(
      anon_struct_ref.components->Value());
  EXPECT_EQ(anon_struct_ref_comp.ref_type, ReferenceType::kImmediate);
  EXPECT_EQ(anon_struct_ref_comp.required_metatype, SymbolMetaType::kStruct);
  EXPECT_EQ(anon_struct_ref_comp.resolved_symbol, &anon_struct);

  // "data"'s type is the (internal) anonymous struct type reference.
  MUST_ASSIGN_LOOKUP_SYMBOL(data, root_symbol, "data");
  EXPECT_EQ(data_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(data_info.file_origin, &src);
  EXPECT_EQ(data_info.declared_type.user_defined_type,
            anon_struct_ref.LastLeaf());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    EXPECT_EQ(anon_struct_ref_comp.resolved_symbol, &anon_struct);  // unchanged
  }
}

TEST(BuildSymbolTableTest, AnonymousStructTypeDataMultiDeclaration) {
  TestVerilogSourceFile src("structy.sv",
                            "struct {\n"
                            "  int size, weight;\n"
                            // Note: the syntax tree structure for "weight"
                            // looks different than that of the the first
                            // variable "size". Make sure this test continues to
                            // work after CST restructuring.
                            "} data;\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  // Expect one anonymous struct definition and reference.
  EXPECT_EQ(root_symbol.Value().anonymous_scope_names.size(), 1);
  ASSERT_EQ(root_symbol.Children().size(), 2);
  // Find the symbol that is a struct (anon), which is not "data".
  const auto found =
      std::find_if(root_symbol.Children().begin(), root_symbol.Children().end(),
                   [](const SymbolTableNode::key_value_type &p) {
                     return p.first != "data";
                   });
  ASSERT_NE(found, root_symbol.Children().end());
  const SymbolTableNode &anon_struct(found->second);
  const SymbolInfo &anon_struct_info(anon_struct.Value());
  EXPECT_EQ(anon_struct_info.metatype, SymbolMetaType::kStruct);
  EXPECT_TRUE(anon_struct_info.local_references_to_bind.empty());

  // Struct has two members.
  MUST_ASSIGN_LOOKUP_SYMBOL(int_size, anon_struct, "size");
  EXPECT_EQ(int_size_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(int_size_info.file_origin, &src);
  EXPECT_EQ(
      verible::StringSpanOfSymbol(*int_size_info.declared_type.syntax_origin),
      "int");
  EXPECT_EQ(int_size_info.declared_type.user_defined_type, nullptr);

  MUST_ASSIGN_LOOKUP_SYMBOL(int_weight, anon_struct, "weight");
  EXPECT_EQ(int_weight_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(int_weight_info.file_origin, &src);
  EXPECT_EQ(
      verible::StringSpanOfSymbol(*int_weight_info.declared_type.syntax_origin),
      "int");
  EXPECT_EQ(int_weight_info.declared_type.user_defined_type, nullptr);

  // Expect to bind anonymous struct immediately.
  ASSERT_EQ(root_symbol.Value().local_references_to_bind.size(), 1);
  const DependentReferences &anon_struct_ref(
      root_symbol.Value().local_references_to_bind.front());
  const ReferenceComponent &anon_struct_ref_comp(
      anon_struct_ref.components->Value());
  EXPECT_EQ(anon_struct_ref_comp.ref_type, ReferenceType::kImmediate);
  EXPECT_EQ(anon_struct_ref_comp.required_metatype, SymbolMetaType::kStruct);
  EXPECT_EQ(anon_struct_ref_comp.resolved_symbol, &anon_struct);

  // "data"'s type is the (internal) anonymous struct type reference.
  MUST_ASSIGN_LOOKUP_SYMBOL(data, root_symbol, "data");
  EXPECT_EQ(data_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(data_info.file_origin, &src);
  EXPECT_EQ(data_info.declared_type.user_defined_type,
            anon_struct_ref.LastLeaf());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    EXPECT_EQ(anon_struct_ref_comp.resolved_symbol, &anon_struct);  // unchanged
  }
}

TEST(BuildSymbolTableTest, AnonymousStructTypeDataMultiVariables) {
  TestVerilogSourceFile src("structy.sv",
                            "struct {\n"
                            "  int size;\n"
                            "} data, foobar;\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  // Expect one anonymous struct definition and two type references.
  EXPECT_EQ(root_symbol.Value().anonymous_scope_names.size(), 1);
  ASSERT_EQ(root_symbol.Children().size(), 3);
  // Find the first symbol that is a struct (anon).
  const auto found = std::find_if(
      root_symbol.Children().begin(), root_symbol.Children().end(),
      [](const SymbolTableNode::key_value_type &p) {
        return p.second.Value().metatype == SymbolMetaType::kStruct;
      });
  ASSERT_NE(found, root_symbol.Children().end());
  const SymbolTableNode &anon_struct(found->second);
  const SymbolInfo &anon_struct_info(anon_struct.Value());
  EXPECT_EQ(anon_struct_info.metatype, SymbolMetaType::kStruct);
  EXPECT_TRUE(anon_struct_info.local_references_to_bind.empty());

  // Struct has one member.
  MUST_ASSIGN_LOOKUP_SYMBOL(int_size, anon_struct, "size");
  EXPECT_EQ(int_size_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(int_size_info.file_origin, &src);
  EXPECT_EQ(
      verible::StringSpanOfSymbol(*int_size_info.declared_type.syntax_origin),
      "int");
  EXPECT_EQ(int_size_info.declared_type.user_defined_type, nullptr);

  // Expect to bind anonymous struct immediately.
  ASSERT_EQ(root_symbol.Value().local_references_to_bind.size(), 1);
  const DependentReferences &anon_struct_ref(
      root_symbol.Value().local_references_to_bind.front());
  const ReferenceComponent &anon_struct_ref_comp(
      anon_struct_ref.components->Value());
  EXPECT_EQ(anon_struct_ref_comp.ref_type, ReferenceType::kImmediate);
  EXPECT_EQ(anon_struct_ref_comp.required_metatype, SymbolMetaType::kStruct);
  EXPECT_EQ(anon_struct_ref_comp.resolved_symbol, &anon_struct);

  // "data"'s type is the (internal) anonymous struct type reference.
  MUST_ASSIGN_LOOKUP_SYMBOL(data, root_symbol, "data");
  EXPECT_EQ(data_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(data_info.file_origin, &src);
  EXPECT_EQ(data_info.declared_type.user_defined_type,
            anon_struct_ref.LastLeaf());

  // "foobar" has same anonymous struct type
  MUST_ASSIGN_LOOKUP_SYMBOL(foobar, root_symbol, "foobar");
  EXPECT_EQ(foobar_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(foobar_info.file_origin, &src);
  EXPECT_EQ(foobar_info.declared_type.user_defined_type,
            anon_struct_ref.LastLeaf());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // "data" and "foobar" share the same anonymous struct type.
    EXPECT_EQ(anon_struct_ref_comp.resolved_symbol, &anon_struct);  // unchanged
    EXPECT_EQ(
        data_info.declared_type.user_defined_type->Value().resolved_symbol,
        &anon_struct);
    EXPECT_EQ(
        foobar_info.declared_type.user_defined_type->Value().resolved_symbol,
        &anon_struct);
  }
}

static bool IsStruct(const SymbolTableNode::key_value_type &p) {
  return p.second.Value().metatype == SymbolMetaType::kStruct;
}

TEST(BuildSymbolTableTest, AnonymousStructTypeDataMultiVariablesDistinctTypes) {
  TestVerilogSourceFile src("structy.sv",
                            "struct {\n"
                            "  int size;\n"
                            "} data;\n"
                            "struct {\n"
                            "  int size;\n"
                            "} foobar;\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  // Expect two anonymous struct definitions and two type references.
  EXPECT_EQ(root_symbol.Value().anonymous_scope_names.size(), 2);
  ASSERT_EQ(root_symbol.Children().size(), 4);
  // Find the symbol that is a struct (anon).
  const auto found = std::find_if(root_symbol.Children().begin(),
                                  root_symbol.Children().end(), IsStruct);
  ASSERT_NE(found, root_symbol.Children().end());
  const SymbolTableNode &anon_struct(found->second);
  const SymbolInfo &anon_struct_info(anon_struct.Value());
  EXPECT_EQ(anon_struct_info.metatype, SymbolMetaType::kStruct);
  EXPECT_TRUE(anon_struct_info.local_references_to_bind.empty());

  const auto found_2 =
      std::find_if(std::next(found), root_symbol.Children().end(), IsStruct);
  ASSERT_NE(found_2, root_symbol.Children().end());
  const SymbolTableNode &anon_struct_2(found_2->second);
  const SymbolInfo &anon_struct_2_info(anon_struct.Value());
  EXPECT_EQ(anon_struct_2_info.metatype, SymbolMetaType::kStruct);
  EXPECT_TRUE(anon_struct_2_info.local_references_to_bind.empty());

  // Struct has one member.  Both structs have the same elements and structure,
  // but have distinct scopes in the symbol table.
  for (const auto *anon_struct_iter : {&anon_struct, &anon_struct_2}) {
    MUST_ASSIGN_LOOKUP_SYMBOL(int_size, *anon_struct_iter, "size");
    EXPECT_EQ(int_size_info.metatype, SymbolMetaType::kDataNetVariableInstance);
    EXPECT_EQ(int_size_info.file_origin, &src);
    EXPECT_EQ(
        verible::StringSpanOfSymbol(*int_size_info.declared_type.syntax_origin),
        "int");
    EXPECT_EQ(int_size_info.declared_type.user_defined_type, nullptr);
  }

  // Expect to bind both anonymous structs immediately.
  ASSERT_EQ(root_symbol.Value().local_references_to_bind.size(), 2);
  const DependentReferences &anon_struct_ref(
      root_symbol.Value().local_references_to_bind.front());
  const ReferenceComponent &anon_struct_ref_comp(
      anon_struct_ref.components->Value());
  EXPECT_EQ(anon_struct_ref_comp.ref_type, ReferenceType::kImmediate);
  EXPECT_EQ(anon_struct_ref_comp.required_metatype, SymbolMetaType::kStruct);
  EXPECT_EQ(anon_struct_ref_comp.resolved_symbol, &anon_struct);

  const DependentReferences &anon_struct_2_ref(
      root_symbol.Value().local_references_to_bind.back());
  const ReferenceComponent &anon_struct_2_ref_comp(
      anon_struct_2_ref.components->Value());
  EXPECT_EQ(anon_struct_2_ref_comp.ref_type, ReferenceType::kImmediate);
  EXPECT_EQ(anon_struct_2_ref_comp.required_metatype, SymbolMetaType::kStruct);
  EXPECT_EQ(anon_struct_2_ref_comp.resolved_symbol, &anon_struct_2);

  // "data"'s type is the (internal) anonymous struct type reference.
  MUST_ASSIGN_LOOKUP_SYMBOL(data, root_symbol, "data");
  EXPECT_EQ(data_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(data_info.file_origin, &src);
  EXPECT_EQ(data_info.declared_type.user_defined_type,
            anon_struct_ref.LastLeaf());

  // "foobar" has a different anonymous struct type
  MUST_ASSIGN_LOOKUP_SYMBOL(foobar, root_symbol, "foobar");
  EXPECT_EQ(foobar_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(foobar_info.file_origin, &src);
  EXPECT_EQ(foobar_info.declared_type.user_defined_type,
            anon_struct_2_ref.LastLeaf());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // "data" and "foobar" have different anonymous struct types.
    EXPECT_EQ(anon_struct_ref_comp.resolved_symbol, &anon_struct);  // unchanged
    EXPECT_EQ(anon_struct_2_ref_comp.resolved_symbol,
              &anon_struct_2);  // unchanged
    EXPECT_EQ(
        data_info.declared_type.user_defined_type->Value().resolved_symbol,
        &anon_struct);
    EXPECT_EQ(
        foobar_info.declared_type.user_defined_type->Value().resolved_symbol,
        &anon_struct_2);
  }
}

TEST(BuildSymbolTableTest, AnonymousStructTypeFunctionParameter) {
  TestVerilogSourceFile src("structy_funky.sv",
                            "function int ff(struct {\n"
                            "      int weight;\n"
                            "    } data);\n"
                            "  return data.weight;\n"
                            "endfunction\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(ff_function, root_symbol, "ff");
  EXPECT_EQ(ff_function_info.metatype, SymbolMetaType::kFunction);

  // Expect one anonymous struct definition and reference.
  EXPECT_EQ(ff_function_info.anonymous_scope_names.size(), 1);
  ASSERT_EQ(ff_function.Children().size(), 2);
  // Find the symbol that is a struct (anon).
  const auto found = std::find_if(
      ff_function.Children().begin(), ff_function.Children().end(),
      [](const SymbolTableNode::key_value_type &p) {
        return p.second.Value().metatype == SymbolMetaType::kStruct;
      });
  ASSERT_NE(found, ff_function.Children().end());
  const SymbolTableNode &anon_struct(found->second);
  const SymbolInfo &anon_struct_info(anon_struct.Value());
  EXPECT_EQ(anon_struct_info.metatype, SymbolMetaType::kStruct);
  EXPECT_TRUE(anon_struct_info.local_references_to_bind.empty());

  MUST_ASSIGN_LOOKUP_SYMBOL(int_weight, anon_struct, "weight");
  EXPECT_EQ(int_weight_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(int_weight_info.file_origin, &src);
  EXPECT_EQ(
      verible::StringSpanOfSymbol(*int_weight_info.declared_type.syntax_origin),
      "int");
  EXPECT_EQ(int_weight_info.declared_type.user_defined_type, nullptr);

  // Expect to bind anonymous struct immediately.
  const auto ref_map(ff_function_info.LocalReferencesMapViewForTesting());
  // Expect one type reference and one reference rooted at "data".
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(data_ref, ref_map, "data");
  const ReferenceComponent &data_ref_comp(data_ref->components->Value());
  EXPECT_EQ(data_ref_comp.identifier, "data");
  EXPECT_EQ(data_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(data_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(data_ref_comp.resolved_symbol, nullptr);

  // "data.weight"
  ASSIGN_MUST_HAVE_UNIQUE(weight_ref, data_ref->components->Children());
  const ReferenceComponent &weight_ref_comp(weight_ref.Value());
  EXPECT_EQ(weight_ref_comp.identifier, "weight");
  EXPECT_EQ(weight_ref_comp.ref_type, ReferenceType::kMemberOfTypeOfParent);
  EXPECT_EQ(weight_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(weight_ref_comp.resolved_symbol, nullptr);

  const DependentReferences &anon_struct_ref(
      **std::find_if(ref_map.begin(), ref_map.end(),
                     [](const decltype(ref_map)::value_type &r) {
                       return (*r.second.begin())
                                  ->components->Value()
                                  .required_metatype == SymbolMetaType::kStruct;
                     })
            ->second.begin());
  const ReferenceComponent &anon_struct_ref_comp(
      anon_struct_ref.components->Value());
  EXPECT_EQ(anon_struct_ref_comp.ref_type, ReferenceType::kImmediate);
  EXPECT_EQ(anon_struct_ref_comp.required_metatype, SymbolMetaType::kStruct);
  EXPECT_EQ(anon_struct_ref_comp.resolved_symbol, &anon_struct);

  // "data"'s type is the (internal) anonymous struct type reference.
  MUST_ASSIGN_LOOKUP_SYMBOL(data, ff_function, "data");
  EXPECT_EQ(data_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(data_info.file_origin, &src);
  EXPECT_EQ(data_info.declared_type.user_defined_type,
            anon_struct_ref.LastLeaf());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    EXPECT_EQ(data_ref_comp.resolved_symbol, &data);          // "data"
    EXPECT_EQ(weight_ref_comp.resolved_symbol, &int_weight);  // ".weight"
    EXPECT_EQ(data_info.declared_type.user_defined_type,
              anon_struct_ref.LastLeaf());
    EXPECT_EQ(anon_struct_ref_comp.resolved_symbol, &anon_struct);  // unchanged
  }
}

TEST(BuildSymbolTableTest, AnonymousStructTypeNested) {
  TestVerilogSourceFile src("structy.sv",
                            "struct {\n"
                            "  struct {\n"
                            "    int size;\n"
                            "  } foo;\n"
                            "} data;\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  // Expect one anonymous struct definition and reference at root level.
  EXPECT_EQ(root_symbol.Value().anonymous_scope_names.size(), 1);
  ASSERT_EQ(root_symbol.Children().size(), 2);
  // Find the symbol that is a struct (anon), which is not "data".
  const auto outer_found = std::find_if(root_symbol.Children().begin(),
                                        root_symbol.Children().end(), IsStruct);
  ASSERT_NE(outer_found, root_symbol.Children().end());
  const SymbolTableNode &outer_anon_struct(outer_found->second);
  const SymbolInfo &outer_anon_struct_info(outer_anon_struct.Value());
  EXPECT_EQ(outer_anon_struct_info.metatype, SymbolMetaType::kStruct);
  EXPECT_EQ(outer_anon_struct_info.local_references_to_bind.size(), 1);
  // Expect one anonymous struct definition inside the outer struct.
  EXPECT_EQ(outer_anon_struct_info.anonymous_scope_names.size(), 1);

  // Outer struct has one member.
  MUST_ASSIGN_LOOKUP_SYMBOL(struct_foo, outer_anon_struct, "foo");
  EXPECT_TRUE(struct_foo.Children().empty());
  EXPECT_EQ(struct_foo_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(struct_foo_info.file_origin, &src);
  EXPECT_NE(struct_foo_info.declared_type.syntax_origin, nullptr);

  // Inner struct lives in the scope of the outer struct.
  const auto inner_found =
      std::find_if(outer_anon_struct.Children().begin(),
                   outer_anon_struct.Children().end(), IsStruct);
  ASSERT_NE(inner_found, outer_anon_struct.Children().end());
  const SymbolTableNode &inner_anon_struct(inner_found->second);
  const SymbolInfo &inner_anon_struct_info(inner_anon_struct.Value());
  EXPECT_EQ(inner_anon_struct_info.metatype, SymbolMetaType::kStruct);
  EXPECT_TRUE(inner_anon_struct_info.local_references_to_bind.empty());

  // "foo"'s type is pre-bound to the inner anonymous struct.
  const ReferenceComponentNode *foo_type =
      struct_foo_info.declared_type.user_defined_type;
  ASSERT_NE(foo_type, nullptr);
  const ReferenceComponent &foo_type_comp(foo_type->Value());
  EXPECT_EQ(foo_type_comp.ref_type, ReferenceType::kImmediate);
  EXPECT_EQ(foo_type_comp.required_metatype, SymbolMetaType::kStruct);
  EXPECT_EQ(foo_type_comp.resolved_symbol, &inner_anon_struct);

  // Inner struct has one member.
  MUST_ASSIGN_LOOKUP_SYMBOL(int_size, inner_anon_struct, "size");
  EXPECT_EQ(int_size_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(int_size_info.file_origin, &src);
  EXPECT_EQ(
      verible::StringSpanOfSymbol(*int_size_info.declared_type.syntax_origin),
      "int");
  EXPECT_EQ(int_size_info.declared_type.user_defined_type, nullptr);

  // Expect to bind (outer) anonymous struct immediately.
  ASSERT_EQ(root_symbol.Value().local_references_to_bind.size(), 1);
  const DependentReferences &anon_struct_ref(
      root_symbol.Value().local_references_to_bind.front());
  const ReferenceComponent &anon_struct_ref_comp(
      anon_struct_ref.components->Value());
  EXPECT_EQ(anon_struct_ref_comp.ref_type, ReferenceType::kImmediate);
  EXPECT_EQ(anon_struct_ref_comp.required_metatype, SymbolMetaType::kStruct);
  EXPECT_EQ(anon_struct_ref_comp.resolved_symbol, &outer_anon_struct);

  // "data"'s type is the (internal) anonymous struct type reference.
  MUST_ASSIGN_LOOKUP_SYMBOL(data, root_symbol, "data");
  EXPECT_EQ(data_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(data_info.file_origin, &src);
  EXPECT_EQ(data_info.declared_type.user_defined_type,
            anon_struct_ref.LastLeaf());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // No change, anonymous types were already bound.
    EXPECT_EQ(foo_type_comp.resolved_symbol, &inner_anon_struct);
    EXPECT_EQ(anon_struct_ref_comp.resolved_symbol, &outer_anon_struct);
  }
}

TEST(BuildSymbolTableTest, AnonymousStructTypeNestedMemberReference) {
  TestVerilogSourceFile src("funky_structy.sv",
                            "function int ff();\n"
                            "  struct {\n"
                            "    struct {\n"
                            "      int size;\n"
                            "    } foo;\n"
                            "  } data;\n"
                            "  return data.foo.size;\n"
                            "endfunction\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(function_ff, root_symbol, "ff");
  EXPECT_EQ(function_ff_info.metatype, SymbolMetaType::kFunction);
  EXPECT_EQ(function_ff_info.file_origin, &src);

  // Expect one anonymous struct definition and reference in function.
  EXPECT_EQ(function_ff_info.anonymous_scope_names.size(), 1);
  ASSERT_EQ(function_ff.Children().size(), 2);
  // Find the symbol that is a struct (anon), which is not "data".
  const auto outer_found = std::find_if(function_ff.Children().begin(),
                                        function_ff.Children().end(), IsStruct);
  ASSERT_NE(outer_found, function_ff.Children().end());
  const SymbolTableNode &outer_anon_struct(outer_found->second);
  const SymbolInfo &outer_anon_struct_info(outer_anon_struct.Value());
  EXPECT_EQ(outer_anon_struct_info.metatype, SymbolMetaType::kStruct);
  EXPECT_EQ(outer_anon_struct_info.local_references_to_bind.size(), 1);
  // Expect one anonymous struct definition inside the outer struct.
  EXPECT_EQ(outer_anon_struct_info.anonymous_scope_names.size(), 1);

  // Outer struct has one member.
  MUST_ASSIGN_LOOKUP_SYMBOL(struct_foo, outer_anon_struct, "foo");
  EXPECT_TRUE(struct_foo.Children().empty());
  EXPECT_EQ(struct_foo_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(struct_foo_info.file_origin, &src);
  EXPECT_NE(struct_foo_info.declared_type.syntax_origin, nullptr);

  // Inner struct lives in the scope of the outer struct.
  const auto inner_found =
      std::find_if(outer_anon_struct.Children().begin(),
                   outer_anon_struct.Children().end(), IsStruct);
  ASSERT_NE(inner_found, outer_anon_struct.Children().end());
  const SymbolTableNode &inner_anon_struct(inner_found->second);
  const SymbolInfo &inner_anon_struct_info(inner_anon_struct.Value());
  EXPECT_EQ(inner_anon_struct_info.metatype, SymbolMetaType::kStruct);
  EXPECT_TRUE(inner_anon_struct_info.local_references_to_bind.empty());

  // "foo"'s type is pre-bound to the inner anonymous struct.
  const ReferenceComponentNode *foo_type =
      struct_foo_info.declared_type.user_defined_type;
  ASSERT_NE(foo_type, nullptr);
  const ReferenceComponent &foo_type_comp(foo_type->Value());
  EXPECT_EQ(foo_type_comp.ref_type, ReferenceType::kImmediate);
  EXPECT_EQ(foo_type_comp.required_metatype, SymbolMetaType::kStruct);
  EXPECT_EQ(foo_type_comp.resolved_symbol, &inner_anon_struct);

  // Inner struct has one member.
  MUST_ASSIGN_LOOKUP_SYMBOL(int_size, inner_anon_struct, "size");
  EXPECT_EQ(int_size_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(int_size_info.file_origin, &src);
  EXPECT_EQ(
      verible::StringSpanOfSymbol(*int_size_info.declared_type.syntax_origin),
      "int");
  EXPECT_EQ(int_size_info.declared_type.user_defined_type, nullptr);

  // Expect to bind (outer) anonymous struct immediately.
  // First reference to anonymous struct, second reference to "data".
  ASSERT_EQ(function_ff_info.local_references_to_bind.size(), 2);

  const DependentReferences &anon_struct_ref(
      function_ff_info.local_references_to_bind.front());
  const ReferenceComponent &anon_struct_ref_comp(
      anon_struct_ref.components->Value());
  EXPECT_EQ(anon_struct_ref_comp.ref_type, ReferenceType::kImmediate);
  EXPECT_EQ(anon_struct_ref_comp.required_metatype, SymbolMetaType::kStruct);
  EXPECT_EQ(anon_struct_ref_comp.resolved_symbol, &outer_anon_struct);

  // "data"'s type is the (internal) anonymous struct type reference.
  MUST_ASSIGN_LOOKUP_SYMBOL(data, function_ff, "data");
  EXPECT_EQ(data_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(data_info.file_origin, &src);
  EXPECT_EQ(data_info.declared_type.user_defined_type,
            anon_struct_ref.LastLeaf());

  // Find the "data.foo.size" reference
  const DependentReferences &data_ref(
      function_ff_info.local_references_to_bind.back());
  const ReferenceComponent &data_ref_comp(data_ref.components->Value());
  EXPECT_EQ(data_ref_comp.identifier, "data");
  EXPECT_EQ(data_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(data_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(data_ref_comp.resolved_symbol, nullptr);

  ASSIGN_MUST_HAVE_UNIQUE(data_foo_ref, data_ref.components->Children());
  const ReferenceComponent &data_foo_ref_comp(data_foo_ref.Value());
  EXPECT_EQ(data_foo_ref_comp.identifier, "foo");
  EXPECT_EQ(data_foo_ref_comp.ref_type, ReferenceType::kMemberOfTypeOfParent);
  EXPECT_EQ(data_foo_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(data_foo_ref_comp.resolved_symbol, nullptr);

  ASSIGN_MUST_HAVE_UNIQUE(data_foo_size_ref, data_foo_ref.Children());
  const ReferenceComponent &data_foo_size_ref_comp(data_foo_size_ref.Value());
  EXPECT_EQ(data_foo_size_ref_comp.identifier, "size");
  EXPECT_EQ(data_foo_size_ref_comp.ref_type,
            ReferenceType::kMemberOfTypeOfParent);
  EXPECT_EQ(data_foo_size_ref_comp.required_metatype,
            SymbolMetaType::kUnspecified);
  EXPECT_EQ(data_foo_size_ref_comp.resolved_symbol, nullptr);

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    EXPECT_EQ(foo_type_comp.resolved_symbol, &inner_anon_struct);
    EXPECT_EQ(anon_struct_ref_comp.resolved_symbol, &outer_anon_struct);
    // Resolve the reference chain "data.foo.size".
    EXPECT_EQ(data_ref_comp.resolved_symbol, &data);
    EXPECT_EQ(data_foo_ref_comp.resolved_symbol, &struct_foo);
    EXPECT_EQ(data_foo_size_ref_comp.resolved_symbol, &int_size);
  }
}

TEST(BuildSymbolTableTest, AnonymousEnumTypeData) {
  TestVerilogSourceFile src("simple_enum.sv",
                            "enum {\n"
                            "  idle, busy\n"
                            "} data;\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  // Expect one anonymous enum definition and reference.
  EXPECT_EQ(root_symbol.Value().anonymous_scope_names.size(), 1);

  // Expect four symbols (enum, data, idle, busy)
  ASSERT_EQ(root_symbol.Children().size(), 4);

  // Find the symbol that is a enum (anon)
  const auto found = std::find_if(
      root_symbol.Children().begin(), root_symbol.Children().end(),
      [](const SymbolTableNode::key_value_type &p) {
        return p.first != "data" && p.first != "idle" && p.first != "busy";
      });
  ASSERT_NE(found, root_symbol.Children().end());
  const SymbolTableNode &anon_enum(found->second);
  const SymbolInfo &anon_enum_info(anon_enum.Value());
  EXPECT_EQ(anon_enum_info.metatype, SymbolMetaType::kEnumType);
  EXPECT_TRUE(anon_enum_info.local_references_to_bind.empty());

  // Enum has two members.
  EXPECT_EQ(anon_enum.Children().size(), 2);

  MUST_ASSIGN_LOOKUP_SYMBOL(idle, anon_enum, "idle");
  EXPECT_EQ(idle_info.metatype, SymbolMetaType::kEnumConstant);
  EXPECT_EQ(idle_info.file_origin, &src);
  EXPECT_EQ(idle_info.declared_type.user_defined_type, nullptr);

  MUST_ASSIGN_LOOKUP_SYMBOL(busy, anon_enum, "busy");
  EXPECT_EQ(busy_info.metatype, SymbolMetaType::kEnumConstant);
  EXPECT_EQ(busy_info.file_origin, &src);
  EXPECT_EQ(busy_info.declared_type.user_defined_type, nullptr);

  // Find idle symbol
  const auto found_enum_idle =
      std::find_if(root_symbol.Children().begin(), root_symbol.Children().end(),
                   [](const SymbolTableNode::key_value_type &p) {
                     return p.first == "idle";
                   });
  ASSERT_NE(found_enum_idle, root_symbol.Children().end());
  const SymbolTableNode &enum_idle(found_enum_idle->second);
  const SymbolInfo &enum_idle_info(enum_idle.Value());
  EXPECT_EQ(enum_idle_info.metatype, SymbolMetaType::kTypeAlias);
  EXPECT_TRUE(enum_idle_info.local_references_to_bind.empty());

  // Find busy symbol
  const auto found_enum_busy =
      std::find_if(root_symbol.Children().begin(), root_symbol.Children().end(),
                   [](const SymbolTableNode::key_value_type &p) {
                     return p.first == "busy";
                   });
  ASSERT_NE(found_enum_busy, root_symbol.Children().end());
  const SymbolTableNode &enum_busy(found_enum_busy->second);
  const SymbolInfo &enum_busy_info(enum_busy.Value());
  EXPECT_EQ(enum_busy_info.metatype, SymbolMetaType::kTypeAlias);
  EXPECT_TRUE(enum_busy_info.local_references_to_bind.empty());

  // Three references: data and two enum constants
  ASSERT_EQ(root_symbol.Value().local_references_to_bind.size(), 3);

  // Expect them to bind immediately.

  const ReferenceComponent &anon_enum_ref_comp(
      root_symbol.Value().local_references_to_bind[2].LastLeaf()->Value());
  EXPECT_EQ(anon_enum_ref_comp.ref_type, ReferenceType::kImmediate);
  EXPECT_EQ(anon_enum_ref_comp.required_metatype, SymbolMetaType::kEnumType);
  EXPECT_EQ(anon_enum_ref_comp.resolved_symbol, &anon_enum);

  const ReferenceComponent &enum_idle_ref_comp(
      root_symbol.Value().local_references_to_bind[0].LastLeaf()->Value());
  EXPECT_EQ(enum_idle_ref_comp.ref_type, ReferenceType::kImmediate);
  EXPECT_EQ(enum_idle_ref_comp.required_metatype,
            SymbolMetaType::kEnumConstant);
  EXPECT_EQ(enum_idle_ref_comp.resolved_symbol, &busy);

  const ReferenceComponent &enum_busy_ref_comp(
      root_symbol.Value().local_references_to_bind[1].LastLeaf()->Value());
  EXPECT_EQ(enum_busy_ref_comp.ref_type, ReferenceType::kImmediate);
  EXPECT_EQ(enum_busy_ref_comp.required_metatype,
            SymbolMetaType::kEnumConstant);
  EXPECT_EQ(enum_busy_ref_comp.resolved_symbol, &idle);

  // "data"'s type is the (internal) anonymous enum type reference.
  MUST_ASSIGN_LOOKUP_SYMBOL(data, root_symbol, "data");
  EXPECT_EQ(data_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(data_info.file_origin, &src);

  const DependentReferences &anon_enum_ref(
      root_symbol.Value().local_references_to_bind[2]);
  EXPECT_EQ(data_info.declared_type.user_defined_type,
            anon_enum_ref.LastLeaf());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // Make sure that resolve doesn't change/break anything
    EXPECT_EQ(anon_enum_ref_comp.resolved_symbol, &anon_enum);
    EXPECT_EQ(enum_idle_ref_comp.resolved_symbol, &busy);
    EXPECT_EQ(enum_busy_ref_comp.resolved_symbol, &idle);
  }
}

TEST(BuildSymbolTableTest, TypedefPrimitive) {
  TestVerilogSourceFile src("typedef.sv",
                            "typedef int number;\n"
                            "number one = 1;\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(number_typedef, root_symbol, "number");
  EXPECT_EQ(number_typedef_info.metatype, SymbolMetaType::kTypeAlias);
  EXPECT_EQ(number_typedef_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(one_var, root_symbol, "one");
  EXPECT_EQ(one_var_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(one_var_info.file_origin, &src);

  // Expect one type reference to "number".
  ASSIGN_MUST_HAVE_UNIQUE(number_ref,
                          root_symbol.Value().local_references_to_bind);
  const ReferenceComponent &number_ref_comp(number_ref.components->Value());
  EXPECT_EQ(number_ref_comp.identifier, "number");
  EXPECT_EQ(number_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(number_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(number_ref_comp.resolved_symbol, nullptr);

  EXPECT_EQ(one_var_info.declared_type.user_defined_type,
            number_ref.components.get());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // Resolve type "number" to the typedef.
    EXPECT_EQ(number_ref_comp.resolved_symbol, &number_typedef);
  }
}

TEST(BuildSymbolTableTest, TypedefTransitive) {
  TestVerilogSourceFile src("typedef.sv",
                            "typedef int num;\n"
                            "typedef num number;\n"
                            "number one = 1;\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(num_typedef, root_symbol, "num");
  EXPECT_EQ(num_typedef_info.metatype, SymbolMetaType::kTypeAlias);
  EXPECT_EQ(num_typedef_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(number_typedef, root_symbol, "number");
  EXPECT_EQ(number_typedef_info.metatype, SymbolMetaType::kTypeAlias);
  EXPECT_EQ(number_typedef_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(one_var, root_symbol, "one");
  EXPECT_EQ(one_var_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(one_var_info.file_origin, &src);

  const auto ref_map(root_symbol.Value().LocalReferencesMapViewForTesting());

  // Expect one type reference to "num".
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(num_ref, ref_map, "num");
  const ReferenceComponent &num_ref_comp(num_ref->components->Value());
  EXPECT_EQ(num_ref_comp.identifier, "num");
  EXPECT_EQ(num_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(num_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(num_ref_comp.resolved_symbol, nullptr);

  // Expect one type reference to "number".
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(number_ref, ref_map, "number");
  const ReferenceComponent &number_ref_comp(number_ref->components->Value());
  EXPECT_EQ(number_ref_comp.identifier, "number");
  EXPECT_EQ(number_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(number_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(number_ref_comp.resolved_symbol, nullptr);

  EXPECT_EQ(number_typedef_info.declared_type.user_defined_type,
            num_ref->components.get());
  EXPECT_EQ(one_var_info.declared_type.user_defined_type,
            number_ref->components.get());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // Resolve type "num" to the typedef.
    EXPECT_EQ(num_ref_comp.resolved_symbol, &num_typedef);
    // Resolve type "number" to the typedef.
    EXPECT_EQ(number_ref_comp.resolved_symbol, &number_typedef);
  }
}

TEST(BuildSymbolTableTest, TypedefClass) {
  TestVerilogSourceFile src("typedef.sv",
                            "class cc;\n"
                            "endclass\n"
                            "typedef cc number;\n"
                            "number foo;\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(cc_class, root_symbol, "cc");
  EXPECT_EQ(cc_class_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(cc_class_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(number_typedef, root_symbol, "number");
  EXPECT_EQ(number_typedef_info.metatype, SymbolMetaType::kTypeAlias);
  EXPECT_EQ(number_typedef_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(foo_var, root_symbol, "foo");
  EXPECT_EQ(foo_var_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(foo_var_info.file_origin, &src);

  // Expect one type reference to "number", and one to "cc".
  const auto &ref_map(root_symbol.Value().LocalReferencesMapViewForTesting());

  ASSIGN_MUST_FIND(cc_type_refs, ref_map, "cc");
  ASSIGN_MUST_HAVE_UNIQUE(cc_type_ref, cc_type_refs);
  const ReferenceComponent &cc_type_ref_comp(cc_type_ref->components->Value());
  EXPECT_EQ(cc_type_ref_comp.identifier, "cc");
  EXPECT_EQ(cc_type_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(cc_type_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(cc_type_ref_comp.resolved_symbol, nullptr);

  ASSIGN_MUST_FIND(number_refs, ref_map, "number");
  ASSIGN_MUST_HAVE_UNIQUE(number_ref, number_refs);
  const ReferenceComponent &number_ref_comp(number_ref->components->Value());
  EXPECT_EQ(number_ref_comp.identifier, "number");
  EXPECT_EQ(number_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(number_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(number_ref_comp.resolved_symbol, nullptr);

  EXPECT_EQ(foo_var_info.declared_type.user_defined_type,
            number_ref->components.get());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    EXPECT_EQ(cc_type_ref_comp.resolved_symbol, &cc_class);
    EXPECT_EQ(number_ref_comp.resolved_symbol, &number_typedef);
  }
}

TEST(BuildSymbolTableTest, TypedefClassPackageQualified) {
  TestVerilogSourceFile src("typedef.sv",
                            "package pp;\n"
                            "  class cc;\n"
                            "  endclass\n"
                            "endpackage\n"
                            "typedef pp::cc number;\n"
                            "number foo;\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(pp_package, root_symbol, "pp");
  EXPECT_EQ(pp_package_info.metatype, SymbolMetaType::kPackage);
  EXPECT_EQ(pp_package_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(cc_class, pp_package, "cc");
  EXPECT_EQ(cc_class_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(cc_class_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(number_typedef, root_symbol, "number");
  EXPECT_EQ(number_typedef_info.metatype, SymbolMetaType::kTypeAlias);
  EXPECT_EQ(number_typedef_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(foo_var, root_symbol, "foo");
  EXPECT_EQ(foo_var_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(foo_var_info.file_origin, &src);

  const auto &ref_map(root_symbol.Value().LocalReferencesMapViewForTesting());

  // Expect one type reference to "pp::cc".
  ASSIGN_MUST_FIND(pp_type_refs, ref_map, "pp");
  ASSIGN_MUST_HAVE_UNIQUE(pp_type_ref, pp_type_refs);
  const ReferenceComponent &pp_type_ref_comp(pp_type_ref->components->Value());
  EXPECT_EQ(pp_type_ref_comp.identifier, "pp");
  EXPECT_EQ(pp_type_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(pp_type_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(pp_type_ref_comp.resolved_symbol, nullptr);

  ASSIGN_MUST_HAVE_UNIQUE(cc_type_ref, pp_type_ref->components->Children());
  const ReferenceComponent &cc_type_ref_comp(cc_type_ref.Value());
  EXPECT_EQ(cc_type_ref_comp.identifier, "cc");
  EXPECT_EQ(cc_type_ref_comp.ref_type, ReferenceType::kDirectMember);
  EXPECT_EQ(cc_type_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(cc_type_ref_comp.resolved_symbol, nullptr);

  // Expect one type reference to "number".
  ASSIGN_MUST_FIND(number_refs, ref_map, "number");
  ASSIGN_MUST_HAVE_UNIQUE(number_ref, number_refs);
  const ReferenceComponent &number_ref_comp(number_ref->components->Value());
  EXPECT_EQ(number_ref_comp.identifier, "number");
  EXPECT_EQ(number_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(number_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(number_ref_comp.resolved_symbol, nullptr);

  EXPECT_EQ(foo_var_info.declared_type.user_defined_type,
            number_ref->components.get());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    EXPECT_EQ(pp_type_ref_comp.resolved_symbol, &pp_package);
    EXPECT_EQ(cc_type_ref_comp.resolved_symbol, &cc_class);
    EXPECT_EQ(number_ref_comp.resolved_symbol, &number_typedef);
  }
}

TEST(BuildSymbolTableTest, TypedefClassUnresolvedQualifiedReferenceBase) {
  TestVerilogSourceFile src("typedef.sv",
                            "typedef pp::cc number;\n"  // unresolved
                            "number foo;\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(number_typedef, root_symbol, "number");
  EXPECT_EQ(number_typedef_info.metatype, SymbolMetaType::kTypeAlias);
  EXPECT_EQ(number_typedef_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(foo_var, root_symbol, "foo");
  EXPECT_EQ(foo_var_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(foo_var_info.file_origin, &src);

  const auto &ref_map(root_symbol.Value().LocalReferencesMapViewForTesting());

  // Expect one type reference to "pp::cc".
  ASSIGN_MUST_FIND(pp_type_refs, ref_map, "pp");
  ASSIGN_MUST_HAVE_UNIQUE(pp_type_ref, pp_type_refs);
  const ReferenceComponent &pp_type_ref_comp(pp_type_ref->components->Value());
  EXPECT_EQ(pp_type_ref_comp.identifier, "pp");
  EXPECT_EQ(pp_type_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(pp_type_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(pp_type_ref_comp.resolved_symbol, nullptr);

  ASSIGN_MUST_HAVE_UNIQUE(cc_type_ref, pp_type_ref->components->Children());
  const ReferenceComponent &cc_type_ref_comp(cc_type_ref.Value());
  EXPECT_EQ(cc_type_ref_comp.identifier, "cc");
  EXPECT_EQ(cc_type_ref_comp.ref_type, ReferenceType::kDirectMember);
  EXPECT_EQ(cc_type_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(cc_type_ref_comp.resolved_symbol, nullptr);

  // Expect one type reference to "number".
  ASSIGN_MUST_FIND(number_refs, ref_map, "number");
  ASSIGN_MUST_HAVE_UNIQUE(number_ref, number_refs);
  const ReferenceComponent &number_ref_comp(number_ref->components->Value());
  EXPECT_EQ(number_ref_comp.identifier, "number");
  EXPECT_EQ(number_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(number_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(number_ref_comp.resolved_symbol, nullptr);

  EXPECT_EQ(foo_var_info.declared_type.user_defined_type,
            number_ref->components.get());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    ASSIGN_MUST_HAVE_UNIQUE(err, resolve_diagnostics);
    EXPECT_EQ(err.code(), absl::StatusCode::kNotFound);

    EXPECT_EQ(pp_type_ref_comp.resolved_symbol, nullptr);
    EXPECT_EQ(cc_type_ref_comp.resolved_symbol, nullptr);
    EXPECT_EQ(number_ref_comp.resolved_symbol, &number_typedef);
  }
}

TEST(BuildSymbolTableTest, TypedefClassPartiallyResolvedQualifiedReference) {
  TestVerilogSourceFile src(
      "typedef.sv",
      "package pp;\n"  // empty
      "endpackage\n"
      "typedef pp::cc::dd number;\n"  // unresolved at "cc"
      "number foo;\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(package_pp, root_symbol, "pp");
  EXPECT_EQ(package_pp_info.metatype, SymbolMetaType::kPackage);

  MUST_ASSIGN_LOOKUP_SYMBOL(number_typedef, root_symbol, "number");
  EXPECT_EQ(number_typedef_info.metatype, SymbolMetaType::kTypeAlias);
  EXPECT_EQ(number_typedef_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(foo_var, root_symbol, "foo");
  EXPECT_EQ(foo_var_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(foo_var_info.file_origin, &src);

  const auto &ref_map(root_symbol.Value().LocalReferencesMapViewForTesting());

  // Expect one type reference to "pp::cc::dd".
  ASSIGN_MUST_FIND(pp_type_refs, ref_map, "pp");
  ASSIGN_MUST_HAVE_UNIQUE(pp_type_ref, pp_type_refs);
  const ReferenceComponent &pp_type_ref_comp(pp_type_ref->components->Value());
  EXPECT_EQ(pp_type_ref_comp.identifier, "pp");
  EXPECT_EQ(pp_type_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(pp_type_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(pp_type_ref_comp.resolved_symbol, nullptr);

  ASSIGN_MUST_HAVE_UNIQUE(cc_type_ref, pp_type_ref->components->Children());
  const ReferenceComponent &cc_type_ref_comp(cc_type_ref.Value());
  EXPECT_EQ(cc_type_ref_comp.identifier, "cc");
  EXPECT_EQ(cc_type_ref_comp.ref_type, ReferenceType::kDirectMember);
  EXPECT_EQ(cc_type_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(cc_type_ref_comp.resolved_symbol, nullptr);

  ASSIGN_MUST_HAVE_UNIQUE(dd_type_ref, cc_type_ref.Children());
  const ReferenceComponent &dd_type_ref_comp(dd_type_ref.Value());
  EXPECT_EQ(dd_type_ref_comp.identifier, "dd");
  EXPECT_EQ(dd_type_ref_comp.ref_type, ReferenceType::kDirectMember);
  EXPECT_EQ(dd_type_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(dd_type_ref_comp.resolved_symbol, nullptr);

  // Expect one type reference to "number".
  ASSIGN_MUST_FIND(number_refs, ref_map, "number");
  ASSIGN_MUST_HAVE_UNIQUE(number_ref, number_refs);
  const ReferenceComponent &number_ref_comp(number_ref->components->Value());
  EXPECT_EQ(number_ref_comp.identifier, "number");
  EXPECT_EQ(number_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(number_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(number_ref_comp.resolved_symbol, nullptr);

  EXPECT_EQ(foo_var_info.declared_type.user_defined_type,
            number_ref->components.get());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    ASSIGN_MUST_HAVE_UNIQUE(err, resolve_diagnostics);
    EXPECT_EQ(err.code(), absl::StatusCode::kNotFound);

    EXPECT_EQ(pp_type_ref_comp.resolved_symbol, &package_pp);
    EXPECT_EQ(cc_type_ref_comp.resolved_symbol, nullptr);  // chain fails here
    EXPECT_EQ(dd_type_ref_comp.resolved_symbol, nullptr);
    EXPECT_EQ(number_ref_comp.resolved_symbol, &number_typedef);
  }
}

TEST(BuildSymbolTableTest, TypedefOfClassTypeParameter) {
  TestVerilogSourceFile src("typedef.sv",
                            "class cc #(parameter type T = int);\n"
                            "  typedef T number;\n"
                            "  number foo;\n"
                            "endclass\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(cc_class, root_symbol, "cc");
  EXPECT_EQ(cc_class_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(cc_class_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(t_type_param, cc_class, "T");
  EXPECT_EQ(t_type_param_info.metatype, SymbolMetaType::kParameter);
  EXPECT_EQ(t_type_param_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(number_typedef, cc_class, "number");
  EXPECT_EQ(number_typedef_info.metatype, SymbolMetaType::kTypeAlias);
  EXPECT_EQ(number_typedef_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(foo_var, cc_class, "foo");
  EXPECT_EQ(foo_var_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(foo_var_info.file_origin, &src);

  // Expect one type reference to "number", and one to "T".
  const auto &ref_map(cc_class_info.LocalReferencesMapViewForTesting());

  ASSIGN_MUST_FIND(t_type_refs, ref_map, "T");
  ASSIGN_MUST_HAVE_UNIQUE(t_type_ref, t_type_refs);
  const ReferenceComponent &t_type_ref_comp(t_type_ref->components->Value());
  EXPECT_EQ(t_type_ref_comp.identifier, "T");
  EXPECT_EQ(t_type_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(t_type_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(t_type_ref_comp.resolved_symbol, nullptr);

  ASSIGN_MUST_FIND(number_refs, ref_map, "number");
  ASSIGN_MUST_HAVE_UNIQUE(number_ref, number_refs);
  const ReferenceComponent &number_ref_comp(number_ref->components->Value());
  EXPECT_EQ(number_ref_comp.identifier, "number");
  EXPECT_EQ(number_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(number_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(number_ref_comp.resolved_symbol, nullptr);

  EXPECT_EQ(foo_var_info.declared_type.user_defined_type,
            number_ref->components.get());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // "number" is type-aliased to "T"
    EXPECT_EQ(t_type_ref_comp.resolved_symbol, &t_type_param);
    EXPECT_EQ(number_ref_comp.resolved_symbol, &number_typedef);
  }
}

TEST(BuildSymbolTableTest, TypedefOfParameterizedClassPositionalParams) {
  TestVerilogSourceFile src("typedef.sv",
                            "package pp;\n"
                            "  class cc #(parameter type T = int);\n"
                            "  endclass\n"
                            "endpackage\n"
                            "typedef pp::cc#(pp::cc#(int)) number;\n"
                            "number foo;\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(pp_package, root_symbol, "pp");
  EXPECT_EQ(pp_package_info.metatype, SymbolMetaType::kPackage);
  EXPECT_EQ(pp_package_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(cc_class, pp_package, "cc");
  EXPECT_EQ(cc_class_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(cc_class_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(t_type_param, cc_class, "T");
  EXPECT_EQ(t_type_param_info.metatype, SymbolMetaType::kParameter);
  EXPECT_EQ(t_type_param_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(number_typedef, root_symbol, "number");
  EXPECT_EQ(number_typedef_info.metatype, SymbolMetaType::kTypeAlias);
  EXPECT_EQ(number_typedef_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(foo_var, root_symbol, "foo");
  EXPECT_EQ(foo_var_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(foo_var_info.file_origin, &src);

  const auto &ref_map(root_symbol.Value().LocalReferencesMapViewForTesting());
  // Expect one type reference to "number".
  ASSIGN_MUST_FIND(number_refs, ref_map, "number");
  ASSIGN_MUST_HAVE_UNIQUE(number_ref, number_refs);
  const ReferenceComponent &number_ref_comp(number_ref->components->Value());
  EXPECT_EQ(number_ref_comp.identifier, "number");
  EXPECT_EQ(number_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(number_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(number_ref_comp.resolved_symbol, nullptr);

  // Expect two type references to "pp::cc".
  ASSIGN_MUST_FIND(pp_refs, ref_map, "pp");
  EXPECT_EQ(pp_refs.size(), 2);
  for (const auto &pp_ref_iter : pp_refs) {
    const ReferenceComponent &pp_ref_comp(pp_ref_iter->components->Value());
    EXPECT_EQ(pp_ref_comp.identifier, "pp");
    EXPECT_EQ(pp_ref_comp.ref_type, ReferenceType::kUnqualified);
    EXPECT_EQ(pp_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
    EXPECT_EQ(pp_ref_comp.resolved_symbol, nullptr);

    ASSIGN_MUST_HAVE_UNIQUE(cc_ref, pp_ref_iter->components->Children());
    const ReferenceComponent &cc_ref_comp(cc_ref.Value());
    EXPECT_EQ(cc_ref_comp.identifier, "cc");
    EXPECT_EQ(cc_ref_comp.ref_type, ReferenceType::kDirectMember);
    EXPECT_EQ(cc_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
    EXPECT_EQ(cc_ref_comp.resolved_symbol, nullptr);
  }

  EXPECT_EQ(foo_var_info.declared_type.user_defined_type,
            number_ref->components.get());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // Resolve "pp::cc" type references
    for (const auto &pp_ref_iter : pp_refs) {
      const ReferenceComponent &pp_ref_comp(pp_ref_iter->components->Value());
      EXPECT_EQ(pp_ref_comp.resolved_symbol, &pp_package);

      ASSIGN_MUST_HAVE_UNIQUE(cc_ref, pp_ref_iter->components->Children());
      const ReferenceComponent &cc_ref_comp(cc_ref.Value());
      EXPECT_EQ(cc_ref_comp.resolved_symbol, &cc_class);
    }
    // Resolve "number" type reference.
    EXPECT_EQ(number_ref_comp.resolved_symbol, &number_typedef);
  }
}

TEST(BuildSymbolTableTest, TypedefOfParameterizedClassNamedParams) {
  TestVerilogSourceFile src("typedef.sv",
                            "package pp;\n"
                            "  class cc #(parameter type T = int);\n"
                            "  endclass\n"
                            "endpackage\n"
                            "typedef pp::cc#(.T(pp::cc#(.T(int)))) number;\n"
                            "number foo;\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(pp_package, root_symbol, "pp");
  EXPECT_EQ(pp_package_info.metatype, SymbolMetaType::kPackage);
  EXPECT_EQ(pp_package_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(cc_class, pp_package, "cc");
  EXPECT_EQ(cc_class_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(cc_class_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(t_type_param, cc_class, "T");
  EXPECT_EQ(t_type_param_info.metatype, SymbolMetaType::kParameter);
  EXPECT_EQ(t_type_param_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(number_typedef, root_symbol, "number");
  EXPECT_EQ(number_typedef_info.metatype, SymbolMetaType::kTypeAlias);
  EXPECT_EQ(number_typedef_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(foo_var, root_symbol, "foo");
  EXPECT_EQ(foo_var_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(foo_var_info.file_origin, &src);

  const auto &ref_map(root_symbol.Value().LocalReferencesMapViewForTesting());
  // Expect one type reference to "number".
  ASSIGN_MUST_FIND(number_refs, ref_map, "number");
  ASSIGN_MUST_HAVE_UNIQUE(number_ref, number_refs);
  const ReferenceComponent &number_ref_comp(number_ref->components->Value());
  EXPECT_EQ(number_ref_comp.identifier, "number");
  EXPECT_EQ(number_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(number_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(number_ref_comp.resolved_symbol, nullptr);

  // Expect two type references to "pp::cc#(.T(...))".
  ASSIGN_MUST_FIND(pp_refs, ref_map, "pp");
  EXPECT_EQ(pp_refs.size(), 2);
  for (const auto &pp_ref_iter : pp_refs) {
    const ReferenceComponent &pp_ref_comp(pp_ref_iter->components->Value());
    EXPECT_EQ(pp_ref_comp.identifier, "pp");
    EXPECT_EQ(pp_ref_comp.ref_type, ReferenceType::kUnqualified);
    EXPECT_EQ(pp_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
    EXPECT_EQ(pp_ref_comp.resolved_symbol, nullptr);

    ASSIGN_MUST_HAVE_UNIQUE(cc_ref, pp_ref_iter->components->Children());
    const ReferenceComponent &cc_ref_comp(cc_ref.Value());
    EXPECT_EQ(cc_ref_comp.identifier, "cc");
    EXPECT_EQ(cc_ref_comp.ref_type, ReferenceType::kDirectMember);
    EXPECT_EQ(cc_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
    EXPECT_EQ(cc_ref_comp.resolved_symbol, nullptr);

    ASSIGN_MUST_HAVE_UNIQUE(t_param_ref, cc_ref.Children());
    const ReferenceComponent &t_param_ref_comp(t_param_ref.Value());
    EXPECT_EQ(t_param_ref_comp.identifier, "T");
    EXPECT_EQ(t_param_ref_comp.ref_type, ReferenceType::kDirectMember);
    EXPECT_EQ(t_param_ref_comp.required_metatype, SymbolMetaType::kParameter);
    EXPECT_EQ(t_param_ref_comp.resolved_symbol, nullptr);
  }

  EXPECT_EQ(foo_var_info.declared_type.user_defined_type,
            number_ref->components.get());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // Resolve "pp::cc#(.T(...))" type references
    for (const auto &pp_ref_iter : pp_refs) {
      const ReferenceComponent &pp_ref_comp(pp_ref_iter->components->Value());
      EXPECT_EQ(pp_ref_comp.resolved_symbol, &pp_package);

      ASSIGN_MUST_HAVE_UNIQUE(cc_ref, pp_ref_iter->components->Children());
      const ReferenceComponent &cc_ref_comp(cc_ref.Value());
      EXPECT_EQ(cc_ref_comp.resolved_symbol, &cc_class);

      ASSIGN_MUST_HAVE_UNIQUE(t_param_ref, cc_ref.Children());
      const ReferenceComponent &t_param_ref_comp(t_param_ref.Value());
      EXPECT_EQ(t_param_ref_comp.resolved_symbol, &t_type_param);
    }
    // Resolve "number" type reference.
    EXPECT_EQ(number_ref_comp.resolved_symbol, &number_typedef);
  }
}

TEST(BuildSymbolTableTest, InvalidMemberLookupOfAliasedType) {
  TestVerilogSourceFile src("typedef.sv",
                            "typedef int number;\n"
                            "typedef number::count bar;\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(number_typedef, root_symbol, "number");
  EXPECT_EQ(number_typedef_info.metatype, SymbolMetaType::kTypeAlias);
  EXPECT_EQ(number_typedef_info.file_origin, &src);
  EXPECT_EQ(number_typedef_info.declared_type.user_defined_type, nullptr);
  EXPECT_EQ(verible::StringSpanOfSymbol(
                *number_typedef_info.declared_type.syntax_origin),
            "int");

  // Expect one type reference to "number".
  const auto &get_count_ref_map(
      root_symbol.Value().LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND(number_refs, get_count_ref_map, "number");
  ASSIGN_MUST_HAVE_UNIQUE(number_ref, number_refs);
  const ReferenceComponent &number_ref_comp(number_ref->components->Value());
  EXPECT_EQ(number_ref_comp.identifier, "number");
  EXPECT_EQ(number_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(number_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(number_ref_comp.resolved_symbol, nullptr);

  ASSIGN_MUST_HAVE_UNIQUE(number_count_ref, number_ref->components->Children());
  const ReferenceComponent &number_count_ref_comp(number_count_ref.Value());
  EXPECT_EQ(number_count_ref_comp.identifier, "count");
  EXPECT_EQ(number_count_ref_comp.ref_type, ReferenceType::kDirectMember);
  EXPECT_EQ(number_count_ref_comp.required_metatype,
            SymbolMetaType::kUnspecified);
  EXPECT_EQ(number_count_ref_comp.resolved_symbol, nullptr);

  // Expect one type reference to "number".
  MUST_ASSIGN_LOOKUP_SYMBOL(bar, root_symbol, "bar");
  EXPECT_EQ(bar_info.metatype, SymbolMetaType::kTypeAlias);
  EXPECT_EQ(bar_info.file_origin, &src);
  // type of "bar" is "number::count"
  EXPECT_EQ(bar_info.declared_type.user_defined_type, &number_count_ref);

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    ASSIGN_MUST_HAVE_UNIQUE(err, resolve_diagnostics);
    EXPECT_EQ(err.code(), absl::StatusCode::kInvalidArgument);
    EXPECT_THAT(err.message(), HasSubstr("Canonical type of "));
    EXPECT_THAT(err.message(), HasSubstr("does not have any members"));

    // Resolving "number::count" should fail.
    EXPECT_EQ(number_ref_comp.resolved_symbol, &number_typedef);
    EXPECT_EQ(number_count_ref_comp.resolved_symbol, nullptr);  // failed
  }
}

TEST(BuildSymbolTableTest, InvalidMemberLookupOfTypedefPrimitive) {
  TestVerilogSourceFile src("typedef.sv",
                            "typedef int number;\n"
                            "function int get_count(number foo);\n"
                            "  return foo.count;\n"  // invalid member
                            "endfunction\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(number_typedef, root_symbol, "number");
  EXPECT_EQ(number_typedef_info.metatype, SymbolMetaType::kTypeAlias);
  EXPECT_EQ(number_typedef_info.file_origin, &src);
  EXPECT_EQ(number_typedef_info.declared_type.user_defined_type, nullptr);
  EXPECT_EQ(verible::StringSpanOfSymbol(
                *number_typedef_info.declared_type.syntax_origin),
            "int");

  MUST_ASSIGN_LOOKUP_SYMBOL(get_count, root_symbol, "get_count");
  EXPECT_EQ(get_count_info.metatype, SymbolMetaType::kFunction);
  EXPECT_EQ(get_count_info.file_origin, &src);
  EXPECT_EQ(get_count_info.declared_type.user_defined_type, nullptr);  // int

  MUST_ASSIGN_LOOKUP_SYMBOL(foo_var, get_count, "foo");
  EXPECT_EQ(foo_var_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(foo_var_info.file_origin, &src);

  // Expect one type reference to "number".
  const auto &get_count_ref_map(
      get_count_info.LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND(number_refs, get_count_ref_map, "number");
  ASSIGN_MUST_HAVE_UNIQUE(number_ref, number_refs);
  const ReferenceComponent &number_ref_comp(number_ref->components->Value());
  EXPECT_EQ(number_ref_comp.identifier, "number");
  EXPECT_EQ(number_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(number_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(number_ref_comp.resolved_symbol, nullptr);

  ASSIGN_MUST_FIND(foo_refs, get_count_ref_map, "foo");
  ASSIGN_MUST_HAVE_UNIQUE(foo_ref, foo_refs);
  const ReferenceComponent &foo_ref_comp(foo_ref->components->Value());
  EXPECT_EQ(foo_ref_comp.identifier, "foo");
  EXPECT_EQ(foo_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(foo_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(foo_ref_comp.resolved_symbol, nullptr);

  ASSIGN_MUST_HAVE_UNIQUE(foo_count_ref, foo_ref->components->Children());
  const ReferenceComponent &foo_count_ref_comp(foo_count_ref.Value());
  EXPECT_EQ(foo_count_ref_comp.identifier, "count");
  EXPECT_EQ(foo_count_ref_comp.ref_type, ReferenceType::kMemberOfTypeOfParent);
  EXPECT_EQ(foo_count_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(foo_count_ref_comp.resolved_symbol, nullptr);

  // type of "foo" is "number"
  EXPECT_EQ(foo_var_info.declared_type.user_defined_type,
            number_ref->components.get());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    ASSIGN_MUST_HAVE_UNIQUE(err, resolve_diagnostics);
    EXPECT_EQ(err.code(), absl::StatusCode::kInvalidArgument);
    EXPECT_THAT(err.message(), HasSubstr("Canonical type of "));
    EXPECT_THAT(err.message(), HasSubstr("does not have any members"));

    // Resolve "foo.count" to "cc::count" through typedef "number"
    EXPECT_EQ(foo_ref_comp.resolved_symbol, &foo_var);
    EXPECT_EQ(foo_count_ref_comp.resolved_symbol, nullptr);  // failed
    EXPECT_EQ(number_ref_comp.resolved_symbol, &number_typedef);
  }
}

TEST(BuildSymbolTableTest, AccessClassMemberThroughTypedef) {
  TestVerilogSourceFile src("typedef.sv",
                            "class cc;\n"
                            "  int count;\n"
                            "endclass\n"
                            "typedef cc number;\n"
                            "function int get_count(number foo);\n"
                            "  return foo.count;\n"
                            "endfunction\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(cc_class, root_symbol, "cc");
  EXPECT_EQ(cc_class_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(cc_class_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(int_count, cc_class, "count");
  EXPECT_EQ(int_count_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(int_count_info.file_origin, &src);
  EXPECT_EQ(int_count_info.declared_type.user_defined_type, nullptr);  // int

  MUST_ASSIGN_LOOKUP_SYMBOL(number_typedef, root_symbol, "number");
  EXPECT_EQ(number_typedef_info.metatype, SymbolMetaType::kTypeAlias);
  EXPECT_EQ(number_typedef_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(get_count, root_symbol, "get_count");
  EXPECT_EQ(get_count_info.metatype, SymbolMetaType::kFunction);
  EXPECT_EQ(get_count_info.file_origin, &src);
  EXPECT_EQ(get_count_info.declared_type.user_defined_type, nullptr);  // int

  MUST_ASSIGN_LOOKUP_SYMBOL(foo_var, get_count, "foo");
  EXPECT_EQ(foo_var_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(foo_var_info.file_origin, &src);

  // Expect one type reference to "cc".
  const auto &root_ref_map(
      root_symbol.Value().LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND(cc_type_refs, root_ref_map, "cc");
  ASSIGN_MUST_HAVE_UNIQUE(cc_type_ref, cc_type_refs);
  const ReferenceComponent &cc_type_ref_comp(cc_type_ref->components->Value());
  EXPECT_EQ(cc_type_ref_comp.identifier, "cc");
  EXPECT_EQ(cc_type_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(cc_type_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(cc_type_ref_comp.resolved_symbol, nullptr);

  EXPECT_EQ(number_typedef_info.declared_type.user_defined_type,
            cc_type_ref->LastTypeComponent());

  // Expect one type reference to "number".
  const auto &get_count_ref_map(
      get_count_info.LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND(number_refs, get_count_ref_map, "number");
  ASSIGN_MUST_HAVE_UNIQUE(number_ref, number_refs);
  const ReferenceComponent &number_ref_comp(number_ref->components->Value());
  EXPECT_EQ(number_ref_comp.identifier, "number");
  EXPECT_EQ(number_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(number_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(number_ref_comp.resolved_symbol, nullptr);

  ASSIGN_MUST_FIND(foo_refs, get_count_ref_map, "foo");
  ASSIGN_MUST_HAVE_UNIQUE(foo_ref, foo_refs);
  const ReferenceComponent &foo_ref_comp(foo_ref->components->Value());
  EXPECT_EQ(foo_ref_comp.identifier, "foo");
  EXPECT_EQ(foo_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(foo_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(foo_ref_comp.resolved_symbol, nullptr);

  ASSIGN_MUST_HAVE_UNIQUE(foo_count_ref, foo_ref->components->Children());
  const ReferenceComponent &foo_count_ref_comp(foo_count_ref.Value());
  EXPECT_EQ(foo_count_ref_comp.identifier, "count");
  EXPECT_EQ(foo_count_ref_comp.ref_type, ReferenceType::kMemberOfTypeOfParent);
  EXPECT_EQ(foo_count_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(foo_count_ref_comp.resolved_symbol, nullptr);

  // type of "foo" is "number"
  EXPECT_EQ(foo_var_info.declared_type.user_defined_type,
            number_ref->components.get());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // Resolve "foo.count" to "cc::count" through typedef "number"
    EXPECT_EQ(cc_type_ref_comp.resolved_symbol, &cc_class);
    EXPECT_EQ(foo_ref_comp.resolved_symbol, &foo_var);
    EXPECT_EQ(foo_count_ref_comp.resolved_symbol, &int_count);
    EXPECT_EQ(number_ref_comp.resolved_symbol, &number_typedef);
  }
}

TEST(BuildSymbolTableTest, AccessStructMemberThroughTypedef) {
  TestVerilogSourceFile src("typedef.sv",
                            "typedef struct {\n"
                            "  int count;\n"
                            "} number;\n"
                            "function int get_count(number foo);\n"
                            "  return foo.count;\n"
                            "endfunction\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  // Find the symbol that is a struct (anon).
  const auto found = std::find_if(root_symbol.Children().begin(),
                                  root_symbol.Children().end(), IsStruct);
  ASSERT_NE(found, root_symbol.Children().end());
  const SymbolTableNode &anon_struct(found->second);
  const SymbolInfo &anon_struct_info(anon_struct.Value());
  EXPECT_EQ(anon_struct_info.metatype, SymbolMetaType::kStruct);
  EXPECT_TRUE(anon_struct_info.local_references_to_bind.empty());

  MUST_ASSIGN_LOOKUP_SYMBOL(int_count, anon_struct, "count");
  EXPECT_EQ(int_count_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(int_count_info.file_origin, &src);
  EXPECT_EQ(int_count_info.declared_type.user_defined_type, nullptr);  // int

  MUST_ASSIGN_LOOKUP_SYMBOL(number_typedef, root_symbol, "number");
  EXPECT_EQ(number_typedef_info.metatype, SymbolMetaType::kTypeAlias);
  EXPECT_EQ(number_typedef_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(get_count, root_symbol, "get_count");
  EXPECT_EQ(get_count_info.metatype, SymbolMetaType::kFunction);
  EXPECT_EQ(get_count_info.file_origin, &src);
  EXPECT_EQ(get_count_info.declared_type.user_defined_type, nullptr);  // int

  MUST_ASSIGN_LOOKUP_SYMBOL(foo_var, get_count, "foo");
  EXPECT_EQ(foo_var_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(foo_var_info.file_origin, &src);

  // typedef struct is already resolved.
  ASSERT_NE(number_typedef_info.declared_type.user_defined_type, nullptr);
  EXPECT_EQ(number_typedef_info.declared_type.user_defined_type->Value()
                .resolved_symbol,
            &anon_struct);

  // Expect one type reference to "number".
  const auto &get_count_ref_map(
      get_count_info.LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND(number_refs, get_count_ref_map, "number");
  ASSIGN_MUST_HAVE_UNIQUE(number_ref, number_refs);
  const ReferenceComponent &number_ref_comp(number_ref->components->Value());
  EXPECT_EQ(number_ref_comp.identifier, "number");
  EXPECT_EQ(number_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(number_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(number_ref_comp.resolved_symbol, nullptr);

  ASSIGN_MUST_FIND(foo_refs, get_count_ref_map, "foo");
  ASSIGN_MUST_HAVE_UNIQUE(foo_ref, foo_refs);
  const ReferenceComponent &foo_ref_comp(foo_ref->components->Value());
  EXPECT_EQ(foo_ref_comp.identifier, "foo");
  EXPECT_EQ(foo_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(foo_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(foo_ref_comp.resolved_symbol, nullptr);

  ASSIGN_MUST_HAVE_UNIQUE(foo_count_ref, foo_ref->components->Children());
  const ReferenceComponent &foo_count_ref_comp(foo_count_ref.Value());
  EXPECT_EQ(foo_count_ref_comp.identifier, "count");
  EXPECT_EQ(foo_count_ref_comp.ref_type, ReferenceType::kMemberOfTypeOfParent);
  EXPECT_EQ(foo_count_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(foo_count_ref_comp.resolved_symbol, nullptr);

  // type of "foo" is "number"
  EXPECT_EQ(foo_var_info.declared_type.user_defined_type,
            number_ref->components.get());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // Resolve "foo.count" to "cc::count" through typedef "number"
    EXPECT_EQ(foo_ref_comp.resolved_symbol, &foo_var);
    EXPECT_EQ(foo_count_ref_comp.resolved_symbol, &int_count);
    EXPECT_EQ(number_ref_comp.resolved_symbol, &number_typedef);
  }
}
TEST(BuildSymbolTableTest, InheritBaseClassThroughTypedef) {
  TestVerilogSourceFile src("typedef.sv",
                            "class base;\n"
                            "  typedef int number;\n"
                            "endclass\n"
                            "typedef base base_alias;\n"
                            "class derived extends base_alias;\n"
                            "  number count;\n"
                            "endclass\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(base_class, root_symbol, "base");
  EXPECT_EQ(base_class_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(base_class_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(number_typedef, base_class, "number");
  EXPECT_EQ(number_typedef_info.metatype, SymbolMetaType::kTypeAlias);
  EXPECT_EQ(number_typedef_info.file_origin, &src);
  EXPECT_EQ(number_typedef_info.declared_type.user_defined_type,
            nullptr);  // int

  MUST_ASSIGN_LOOKUP_SYMBOL(base_alias, root_symbol, "base_alias");
  EXPECT_EQ(base_alias_info.metatype, SymbolMetaType::kTypeAlias);
  EXPECT_EQ(base_alias_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(derived_class, root_symbol, "derived");
  EXPECT_EQ(derived_class_info.metatype, SymbolMetaType::kClass);
  EXPECT_EQ(derived_class_info.file_origin, &src);

  MUST_ASSIGN_LOOKUP_SYMBOL(count, derived_class, "count");
  EXPECT_EQ(count_info.metatype, SymbolMetaType::kDataNetVariableInstance);
  EXPECT_EQ(count_info.file_origin, &src);

  const auto &root_ref_map(
      root_symbol.Value().LocalReferencesMapViewForTesting());

  // Expect one reference to "base"
  ASSIGN_MUST_FIND(base_type_refs, root_ref_map, "base");
  ASSIGN_MUST_HAVE_UNIQUE(base_type_ref, base_type_refs);
  const ReferenceComponent &base_type_ref_comp(
      base_type_ref->components->Value());
  EXPECT_EQ(base_type_ref_comp.identifier, "base");
  EXPECT_EQ(base_type_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(base_type_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(base_type_ref_comp.resolved_symbol, nullptr);

  EXPECT_EQ(base_alias_info.declared_type.user_defined_type,
            base_type_ref->components.get());

  // Expect one reference to "base_alias"
  ASSIGN_MUST_FIND(base_alias_refs, root_ref_map, "base_alias");
  ASSIGN_MUST_HAVE_UNIQUE(base_alias_ref, base_alias_refs);
  const ReferenceComponent &base_alias_ref_comp(
      base_alias_ref->components->Value());
  EXPECT_EQ(base_alias_ref_comp.identifier, "base_alias");
  EXPECT_EQ(base_alias_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(base_alias_ref_comp.required_metatype, SymbolMetaType::kClass);
  EXPECT_EQ(base_alias_ref_comp.resolved_symbol, nullptr);

  EXPECT_EQ(derived_class_info.parent_type.user_defined_type,
            base_alias_ref->components.get());

  const auto &derived_ref_map(
      derived_class_info.LocalReferencesMapViewForTesting());

  // Expect one type reference to "number".
  ASSIGN_MUST_FIND(number_refs, derived_ref_map, "number");
  ASSIGN_MUST_HAVE_UNIQUE(number_ref, number_refs);
  const ReferenceComponent &number_ref_comp(number_ref->components->Value());
  EXPECT_EQ(number_ref_comp.identifier, "number");
  EXPECT_EQ(number_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(number_ref_comp.required_metatype, SymbolMetaType::kUnspecified);
  EXPECT_EQ(number_ref_comp.resolved_symbol, nullptr);

  EXPECT_EQ(count_info.declared_type.user_defined_type,
            number_ref->components.get());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    EXPECT_EQ(base_type_ref_comp.resolved_symbol, &base_class);
    EXPECT_EQ(base_alias_ref_comp.resolved_symbol, &base_alias);
    // "number" is resolved to "base::number"
    EXPECT_EQ(number_ref_comp.resolved_symbol, &number_typedef);
  }
}

static bool SourceFileLess(const TestVerilogSourceFile *left,
                           const TestVerilogSourceFile *right) {
  return left->ReferencedPath() < right->ReferencedPath();
}

static void SortSourceFiles(
    std::vector<const TestVerilogSourceFile *> *sources) {
  std::sort(sources->begin(), sources->end(), SourceFileLess);
}

static bool PermuteSourceFiles(
    std::vector<const TestVerilogSourceFile *> *sources) {
  return std::next_permutation(sources->begin(), sources->end(),
                               SourceFileLess);
}

TEST(BuildSymbolTableTest, MultiFileModuleInstance) {
  // Linear dependency chain between 3 files.
  TestVerilogSourceFile pp_src("pp.sv",
                               "module pp;\n"
                               "endmodule\n");
  TestVerilogSourceFile qq_src("qq.sv",
                               "module qq;\n"
                               "  pp pp_inst();\n"  // instance
                               "endmodule\n");
  TestVerilogSourceFile ss_src("ss.sv",
                               "module ss;\n"
                               "  qq qq_inst();\n"  // instance
                               "endmodule\n");
  {
    const auto status = pp_src.Parse();
    ASSERT_TRUE(status.ok()) << status.message();
  }
  {
    const auto status = qq_src.Parse();
    ASSERT_TRUE(status.ok()) << status.message();
  }
  {
    const auto status = ss_src.Parse();
    ASSERT_TRUE(status.ok()) << status.message();
  }

  // All permutations of the following file ordering should end up with the
  // same results.
  std::vector<const TestVerilogSourceFile *> ordering(
      {&pp_src, &qq_src, &ss_src});
  // start with the lexicographically "lowest" permutation
  SortSourceFiles(&ordering);
  int count = 0;
  do {
    SymbolTable symbol_table(nullptr);
    const SymbolTableNode &root_symbol(symbol_table.Root());

    for (const auto *src : ordering) {
      const auto build_diagnostics = BuildSymbolTable(*src, &symbol_table);
      EXPECT_EMPTY_STATUSES(build_diagnostics);
    }

    // Goal: resolve the reference of "pp" to this definition node.
    MUST_ASSIGN_LOOKUP_SYMBOL(pp, root_symbol, "pp");

    // Inspect inside the "qq" module definition.
    MUST_ASSIGN_LOOKUP_SYMBOL(qq, root_symbol, "qq");

    MUST_ASSIGN_LOOKUP_SYMBOL(ss, root_symbol, "ss");

    // "pp_inst" is an instance of type "pp"
    MUST_ASSIGN_LOOKUP_SYMBOL(pp_inst, qq, "pp_inst");

    // "qq_inst" is an instance of type "qq"
    MUST_ASSIGN_LOOKUP_SYMBOL(qq_inst, ss, "qq_inst");

    EXPECT_EQ(pp_info.file_origin, &pp_src);
    EXPECT_EQ(qq_info.file_origin, &qq_src);
    EXPECT_EQ(ss_info.file_origin, &ss_src);
    {
      ASSERT_EQ(qq_info.local_references_to_bind.size(), 2);
      const auto ref_map(qq_info.LocalReferencesMapViewForTesting());
      {
        ASSIGN_MUST_FIND_EXACTLY_ONE_REF(pp_type, ref_map, "pp");
        const ReferenceComponentNode *ref_node = pp_type->LastTypeComponent();
        ASSERT_NE(ref_node, nullptr);
        const ReferenceComponent &ref(ref_node->Value());
        EXPECT_EQ(ref.identifier, "pp");
        EXPECT_TRUE(verible::IsSubRange(ref.identifier,
                                        qq_src.GetTextStructure()->Contents()));
        EXPECT_EQ(ref.ref_type, ReferenceType::kUnqualified);
        EXPECT_EQ(ref.required_metatype, SymbolMetaType::kUnspecified);
        EXPECT_EQ(ref.resolved_symbol, nullptr);
      }
      {  // self-reference to "pp_inst" instance
        ASSIGN_MUST_FIND_EXACTLY_ONE_REF(pp_inst_self_ref, ref_map, "pp_inst");
        EXPECT_TRUE(is_leaf(*pp_inst_self_ref->components));  // no named ports
        // self-reference is already bound.
        EXPECT_EQ(pp_inst_self_ref->components->Value().resolved_symbol,
                  &pp_inst);
      }
    }
    {
      ASSERT_EQ(ss_info.local_references_to_bind.size(), 2);
      const auto ref_map(ss_info.LocalReferencesMapViewForTesting());
      {
        ASSIGN_MUST_FIND_EXACTLY_ONE_REF(qq_type, ref_map, "qq");
        const ReferenceComponentNode *ref_node = qq_type->LastTypeComponent();
        ASSERT_NE(ref_node, nullptr);
        const ReferenceComponent &ref(ref_node->Value());
        EXPECT_EQ(ref.identifier, "qq");
        EXPECT_TRUE(verible::IsSubRange(ref.identifier,
                                        ss_src.GetTextStructure()->Contents()));
        EXPECT_EQ(ref.ref_type, ReferenceType::kUnqualified);
        EXPECT_EQ(ref.required_metatype, SymbolMetaType::kUnspecified);
        EXPECT_EQ(ref.resolved_symbol, nullptr);
      }
      {  // self-reference to "qq_inst" instance
        ASSIGN_MUST_FIND_EXACTLY_ONE_REF(qq_inst_self_ref, ref_map, "qq_inst");
        EXPECT_TRUE(is_leaf(*qq_inst_self_ref->components));  // no named ports
        // self-reference is already bound.
        EXPECT_EQ(qq_inst_self_ref->components->Value().resolved_symbol,
                  &qq_inst);
      }
    }

    {  // Verify pp_inst's type info
      EXPECT_TRUE(pp_inst_info.local_references_to_bind.empty());
      EXPECT_NE(pp_inst_info.declared_type.user_defined_type, nullptr);
      const ReferenceComponent &pp_type(
          pp_inst_info.declared_type.user_defined_type->Value());
      EXPECT_EQ(pp_type.identifier, "pp");
      EXPECT_EQ(pp_type.resolved_symbol, nullptr);  // nothing resolved yet
      EXPECT_EQ(pp_type.ref_type, ReferenceType::kUnqualified);
      EXPECT_EQ(pp_type.required_metatype, SymbolMetaType::kUnspecified);
      EXPECT_EQ(pp_inst_info.file_origin, &qq_src);
    }

    {  // Verify qq_inst's type info
      EXPECT_TRUE(qq_inst_info.local_references_to_bind.empty());
      EXPECT_NE(qq_inst_info.declared_type.user_defined_type, nullptr);
      const ReferenceComponent &qq_type(
          qq_inst_info.declared_type.user_defined_type->Value());
      EXPECT_EQ(qq_type.identifier, "qq");
      EXPECT_EQ(qq_type.resolved_symbol, nullptr);  // nothing resolved yet
      EXPECT_EQ(qq_type.ref_type, ReferenceType::kUnqualified);
      EXPECT_EQ(qq_type.required_metatype, SymbolMetaType::kUnspecified);
      EXPECT_EQ(qq_inst_info.file_origin, &ss_src);
    }

    // Resolve symbols.
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // Verify that typeof(pp_inst) successfully resolved to module pp.
    EXPECT_EQ(
        pp_inst_info.declared_type.user_defined_type->Value().resolved_symbol,
        &pp);
    // Verify that typeof(qq_inst) successfully resolved to module qq.
    EXPECT_EQ(
        qq_inst_info.declared_type.user_defined_type->Value().resolved_symbol,
        &qq);
    ++count;
  } while (PermuteSourceFiles(&ordering));
  EXPECT_EQ(count, 6);  // make sure we covered all permutations
}

TEST(BuildSymbolTableTest, ModuleInstancesFromProjectOneFileAtATime) {
  const auto tempdir = ::testing::TempDir();
  const std::string sources_dir = JoinPath(tempdir, __FUNCTION__);
  ASSERT_TRUE(CreateDir(sources_dir).ok());

  VerilogProject project(sources_dir, {/* no include path */});

  // Linear dependency chain between 3 files.  Order arbitrarily chosen.
  constexpr std::string_view  //
      text1(
          "module ss;\n"
          "  qq qq_inst();\n"  // instance
          "endmodule\n"),
      text2(
          "module pp;\n"
          "endmodule\n"),
      text3(
          "module qq;\n"
          "  pp pp_inst();\n"  // instance
          "endmodule\n");
  // Write to temporary files.
  const ScopedTestFile file1(sources_dir, text1);
  const ScopedTestFile file2(sources_dir, text2);
  const ScopedTestFile file3(sources_dir, text3);

  // Register files as part of project.
  for (const auto *file : {&file1, &file2, &file3}) {
    const auto status_or_file =
        project.OpenTranslationUnit(Basename(file->filename()));
    ASSERT_TRUE(status_or_file.ok());
  }

  SymbolTable symbol_table(&project);
  EXPECT_EQ(symbol_table.Project(), &project);

  // Caller decides order of processing files, which doesn't matter for this
  // example.
  std::vector<absl::Status> build_diagnostics;
  for (const auto *file : {&file3, &file2, &file1}) {
    symbol_table.BuildSingleTranslationUnit(Basename(file->filename()),
                                            &build_diagnostics);
    EXPECT_EMPTY_STATUSES(build_diagnostics);
  }

  const SymbolTableNode &root_symbol(symbol_table.Root());

  // Goal: resolve the reference of "pp" to this definition node.
  MUST_ASSIGN_LOOKUP_SYMBOL(pp, root_symbol, "pp");

  // Inspect inside the "qq" module definition.
  MUST_ASSIGN_LOOKUP_SYMBOL(qq, root_symbol, "qq");

  MUST_ASSIGN_LOOKUP_SYMBOL(ss, root_symbol, "ss");

  // "pp_inst" is an instance of type "pp"
  MUST_ASSIGN_LOOKUP_SYMBOL(pp_inst, qq, "pp_inst");

  // "qq_inst" is an instance of type "qq"
  MUST_ASSIGN_LOOKUP_SYMBOL(qq_inst, ss, "qq_inst");

  {
    ASSERT_EQ(qq_info.local_references_to_bind.size(), 2);
    const auto ref_map(qq_info.LocalReferencesMapViewForTesting());
    {
      ASSIGN_MUST_FIND_EXACTLY_ONE_REF(pp_type, ref_map, "pp");
      const ReferenceComponentNode *ref_node = pp_type->LastTypeComponent();
      ASSERT_NE(ref_node, nullptr);
      const ReferenceComponent &ref(ref_node->Value());
      EXPECT_EQ(ref.identifier, "pp");
      EXPECT_EQ(ref.ref_type, ReferenceType::kUnqualified);
      EXPECT_EQ(ref.required_metatype, SymbolMetaType::kUnspecified);
      EXPECT_EQ(ref.resolved_symbol, nullptr);
    }
    {  // self-reference to "pp_inst" instance
      ASSIGN_MUST_FIND_EXACTLY_ONE_REF(pp_inst_self_ref, ref_map, "pp_inst");
      EXPECT_TRUE(is_leaf(*pp_inst_self_ref->components));  // no named ports
      // self-reference is already bound.
      EXPECT_EQ(pp_inst_self_ref->components->Value().resolved_symbol,
                &pp_inst);
    }
  }
  {
    ASSERT_EQ(ss_info.local_references_to_bind.size(), 2);
    const auto ref_map(ss_info.LocalReferencesMapViewForTesting());
    {
      ASSIGN_MUST_FIND_EXACTLY_ONE_REF(qq_type, ref_map, "qq");
      const ReferenceComponentNode *ref_node = qq_type->LastTypeComponent();
      ASSERT_NE(ref_node, nullptr);
      const ReferenceComponent &ref(ref_node->Value());
      EXPECT_EQ(ref.identifier, "qq");
      EXPECT_EQ(ref.ref_type, ReferenceType::kUnqualified);
      EXPECT_EQ(ref.required_metatype, SymbolMetaType::kUnspecified);
      EXPECT_EQ(ref.resolved_symbol, nullptr);
    }
    {  // self-reference to "qq_inst" instance
      ASSIGN_MUST_FIND_EXACTLY_ONE_REF(qq_inst_self_ref, ref_map, "qq_inst");
      EXPECT_TRUE(is_leaf(*qq_inst_self_ref->components));  // no named ports
      // self-reference is already bound.
      EXPECT_EQ(qq_inst_self_ref->components->Value().resolved_symbol,
                &qq_inst);
    }
  }

  {  // Verify pp_inst's type info
    EXPECT_TRUE(pp_inst_info.local_references_to_bind.empty());
    EXPECT_NE(pp_inst_info.declared_type.user_defined_type, nullptr);
    const ReferenceComponent &pp_type(
        pp_inst_info.declared_type.user_defined_type->Value());
    EXPECT_EQ(pp_type.identifier, "pp");
    EXPECT_EQ(pp_type.resolved_symbol, nullptr);  // nothing resolved yet
    EXPECT_EQ(pp_type.ref_type, ReferenceType::kUnqualified);
    EXPECT_EQ(pp_type.required_metatype, SymbolMetaType::kUnspecified);
  }

  {  // Verify qq_inst's type info
    EXPECT_TRUE(qq_inst_info.local_references_to_bind.empty());
    EXPECT_NE(qq_inst_info.declared_type.user_defined_type, nullptr);
    const ReferenceComponent &qq_type(
        qq_inst_info.declared_type.user_defined_type->Value());
    EXPECT_EQ(qq_type.identifier, "qq");
    EXPECT_EQ(qq_type.resolved_symbol, nullptr);  // nothing resolved yet
    EXPECT_EQ(qq_type.ref_type, ReferenceType::kUnqualified);
    EXPECT_EQ(qq_type.required_metatype, SymbolMetaType::kUnspecified);
  }

  // Resolve symbols.
  std::vector<absl::Status> resolve_diagnostics;
  symbol_table.Resolve(&resolve_diagnostics);
  EXPECT_EMPTY_STATUSES(resolve_diagnostics);

  // Verify that typeof(pp_inst) successfully resolved to module pp.
  EXPECT_EQ(
      pp_inst_info.declared_type.user_defined_type->Value().resolved_symbol,
      &pp);
  // Verify that typeof(qq_inst) successfully resolved to module qq.
  EXPECT_EQ(
      qq_inst_info.declared_type.user_defined_type->Value().resolved_symbol,
      &qq);
}

TEST(BuildSymbolTableTest, ModuleInstancesFromProjectMissingFile) {
  const auto tempdir = ::testing::TempDir();
  const std::string sources_dir = JoinPath(tempdir, __FUNCTION__);
  VerilogProject project(sources_dir, {/* no include path */});

  SymbolTable symbol_table(&project);
  EXPECT_EQ(symbol_table.Project(), &project);

  std::vector<absl::Status> build_diagnostics;
  symbol_table.BuildSingleTranslationUnit("file/not/found.txt",
                                          &build_diagnostics);
  ASSERT_FALSE(build_diagnostics.empty());
  EXPECT_THAT(build_diagnostics.front().code(), absl::StatusCode::kNotFound)
      << build_diagnostics.front();
}

TEST(BuildSymbolTableTest, ModuleInstancesFromProjectFilesGood) {
  const auto tempdir = ::testing::TempDir();
  const std::string sources_dir = JoinPath(tempdir, __FUNCTION__);
  ASSERT_TRUE(CreateDir(sources_dir).ok());

  VerilogProject project(sources_dir, {/* no include path */});

  // Linear dependency chain between 3 files.  Order arbitrarily chosen.
  constexpr std::string_view  //
      text1(
          "module ss;\n"
          "  qq qq_inst();\n"  // instance
          "endmodule\n"),
      text2(
          "module pp;\n"
          "endmodule\n"),
      text3(
          "module qq;\n"
          "  pp pp_inst();\n"  // instance
          "endmodule\n");
  // Write to temporary files.
  const ScopedTestFile file1(sources_dir, text1);
  const ScopedTestFile file2(sources_dir, text2);
  const ScopedTestFile file3(sources_dir, text3);

  // Register files as part of project.
  for (const auto *file : {&file1, &file2, &file3}) {
    const auto status_or_file =
        project.OpenTranslationUnit(Basename(file->filename()));
    ASSERT_TRUE(status_or_file.ok());
  }

  SymbolTable symbol_table(&project);
  EXPECT_EQ(symbol_table.Project(), &project);

  std::vector<absl::Status> build_diagnostics;
  symbol_table.Build(&build_diagnostics);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  const SymbolTableNode &root_symbol(symbol_table.Root());

  // Goal: resolve the reference of "pp" to this definition node.
  MUST_ASSIGN_LOOKUP_SYMBOL(pp, root_symbol, "pp");

  // Inspect inside the "qq" module definition.
  MUST_ASSIGN_LOOKUP_SYMBOL(qq, root_symbol, "qq");

  MUST_ASSIGN_LOOKUP_SYMBOL(ss, root_symbol, "ss");

  // "pp_inst" is an instance of type "pp"
  MUST_ASSIGN_LOOKUP_SYMBOL(pp_inst, qq, "pp_inst");

  // "qq_inst" is an instance of type "qq"
  MUST_ASSIGN_LOOKUP_SYMBOL(qq_inst, ss, "qq_inst");

  {
    ASSERT_EQ(qq_info.local_references_to_bind.size(), 2);
    const auto ref_map(qq_info.LocalReferencesMapViewForTesting());
    {
      ASSIGN_MUST_FIND_EXACTLY_ONE_REF(pp_type, ref_map, "pp");
      const ReferenceComponentNode *ref_node = pp_type->LastTypeComponent();
      ASSERT_NE(ref_node, nullptr);
      const ReferenceComponent &ref(ref_node->Value());
      EXPECT_EQ(ref.identifier, "pp");
      EXPECT_EQ(ref.ref_type, ReferenceType::kUnqualified);
      EXPECT_EQ(ref.required_metatype, SymbolMetaType::kUnspecified);
      EXPECT_EQ(ref.resolved_symbol, nullptr);
    }
    {  // self-reference to "pp_inst" instance
      ASSIGN_MUST_FIND_EXACTLY_ONE_REF(pp_inst_self_ref, ref_map, "pp_inst");
      EXPECT_TRUE(is_leaf(*pp_inst_self_ref->components));  // no named ports
      // self-reference is already bound.
      EXPECT_EQ(pp_inst_self_ref->components->Value().resolved_symbol,
                &pp_inst);
    }
  }
  {
    ASSERT_EQ(ss_info.local_references_to_bind.size(), 2);
    const auto ref_map(ss_info.LocalReferencesMapViewForTesting());
    {
      ASSIGN_MUST_FIND_EXACTLY_ONE_REF(qq_type, ref_map, "qq");
      const ReferenceComponentNode *ref_node = qq_type->LastTypeComponent();
      ASSERT_NE(ref_node, nullptr);
      const ReferenceComponent &ref(ref_node->Value());
      EXPECT_EQ(ref.identifier, "qq");
      EXPECT_EQ(ref.ref_type, ReferenceType::kUnqualified);
      EXPECT_EQ(ref.required_metatype, SymbolMetaType::kUnspecified);
      EXPECT_EQ(ref.resolved_symbol, nullptr);
    }
    {  // self-reference to "qq_inst" instance
      ASSIGN_MUST_FIND_EXACTLY_ONE_REF(qq_inst_self_ref, ref_map, "qq_inst");
      EXPECT_TRUE(is_leaf(*qq_inst_self_ref->components));  // no named ports
      // self-reference is already bound.
      EXPECT_EQ(qq_inst_self_ref->components->Value().resolved_symbol,
                &qq_inst);
    }
  }

  {  // Verify pp_inst's type info
    EXPECT_TRUE(pp_inst_info.local_references_to_bind.empty());
    EXPECT_NE(pp_inst_info.declared_type.user_defined_type, nullptr);
    const ReferenceComponent &pp_type(
        pp_inst_info.declared_type.user_defined_type->Value());
    EXPECT_EQ(pp_type.identifier, "pp");
    EXPECT_EQ(pp_type.resolved_symbol, nullptr);  // nothing resolved yet
    EXPECT_EQ(pp_type.ref_type, ReferenceType::kUnqualified);
    EXPECT_EQ(pp_type.required_metatype, SymbolMetaType::kUnspecified);
  }

  {  // Verify qq_inst's type info
    EXPECT_TRUE(qq_inst_info.local_references_to_bind.empty());
    EXPECT_NE(qq_inst_info.declared_type.user_defined_type, nullptr);
    const ReferenceComponent &qq_type(
        qq_inst_info.declared_type.user_defined_type->Value());
    EXPECT_EQ(qq_type.identifier, "qq");
    EXPECT_EQ(qq_type.resolved_symbol, nullptr);  // nothing resolved yet
    EXPECT_EQ(qq_type.ref_type, ReferenceType::kUnqualified);
    EXPECT_EQ(qq_type.required_metatype, SymbolMetaType::kUnspecified);
  }

  // Resolve symbols.
  std::vector<absl::Status> resolve_diagnostics;
  symbol_table.Resolve(&resolve_diagnostics);
  EXPECT_EMPTY_STATUSES(resolve_diagnostics);

  // Verify that typeof(pp_inst) successfully resolved to module pp.
  EXPECT_EQ(
      pp_inst_info.declared_type.user_defined_type->Value().resolved_symbol,
      &pp);
  // Verify that typeof(qq_inst) successfully resolved to module qq.
  EXPECT_EQ(
      qq_inst_info.declared_type.user_defined_type->Value().resolved_symbol,
      &qq);
}

TEST(BuildSymbolTableTest, SingleFileModuleInstanceCyclicDependencies) {
  // Cyclic dependencies among three modules in one file.
  // Make sure this can still build and resolve without hanging,
  // even if this is semantically illegal.
  TestVerilogSourceFile src("cycle.sv",
                            "module pp;\n"
                            "  ss ss_inst();\n"  // instance
                            "endmodule\n"
                            "module qq;\n"
                            "  pp pp_inst();\n"  // instance
                            "endmodule\n"
                            "module ss;\n"
                            "  qq qq_inst();\n"  // instance
                            "endmodule\n");
  {
    const auto status = src.Parse();
    ASSERT_TRUE(status.ok()) << status.message();
  }

  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  // Goal: resolve the reference of "pp" to this definition node.
  MUST_ASSIGN_LOOKUP_SYMBOL(pp, root_symbol, "pp");

  // Inspect inside the "qq" module definition.
  MUST_ASSIGN_LOOKUP_SYMBOL(qq, root_symbol, "qq");

  MUST_ASSIGN_LOOKUP_SYMBOL(ss, root_symbol, "ss");

  // "ss_inst" is an instance of type "ss"
  MUST_ASSIGN_LOOKUP_SYMBOL(ss_inst, pp, "ss_inst");

  // "pp_inst" is an instance of type "pp"
  MUST_ASSIGN_LOOKUP_SYMBOL(pp_inst, qq, "pp_inst");

  // "qq_inst" is an instance of type "qq"
  MUST_ASSIGN_LOOKUP_SYMBOL(qq_inst, ss, "qq_inst");

  EXPECT_EQ(pp_info.file_origin, &src);
  EXPECT_EQ(qq_info.file_origin, &src);
  EXPECT_EQ(ss_info.file_origin, &src);
  {
    ASSERT_EQ(pp_info.local_references_to_bind.size(), 2);
    const auto ref_map(pp_info.LocalReferencesMapViewForTesting());
    {
      ASSIGN_MUST_FIND_EXACTLY_ONE_REF(ss_type, ref_map, "ss");
      const ReferenceComponentNode *ref_node = ss_type->LastTypeComponent();
      ASSERT_NE(ref_node, nullptr);
      const ReferenceComponent &ref(ref_node->Value());
      EXPECT_EQ(ref.identifier, "ss");
      EXPECT_TRUE(verible::IsSubRange(ref.identifier,
                                      src.GetTextStructure()->Contents()));
      EXPECT_EQ(ref.ref_type, ReferenceType::kUnqualified);
      EXPECT_EQ(ref.required_metatype, SymbolMetaType::kUnspecified);
      EXPECT_EQ(ref.resolved_symbol, nullptr);
    }
    {  // self-reference to "ss_inst" instance
      ASSIGN_MUST_FIND_EXACTLY_ONE_REF(ss_inst_self_ref, ref_map, "ss_inst");
      EXPECT_TRUE(is_leaf(*ss_inst_self_ref->components));  // no named ports
      // self-reference is already bound.
      EXPECT_EQ(ss_inst_self_ref->components->Value().resolved_symbol,
                &ss_inst);
    }
  }
  {
    ASSERT_EQ(qq_info.local_references_to_bind.size(), 2);
    const auto ref_map(qq_info.LocalReferencesMapViewForTesting());
    {
      ASSIGN_MUST_FIND_EXACTLY_ONE_REF(pp_type, ref_map, "pp");
      const ReferenceComponentNode *ref_node = pp_type->LastTypeComponent();
      ASSERT_NE(ref_node, nullptr);
      const ReferenceComponent &ref(ref_node->Value());
      EXPECT_EQ(ref.identifier, "pp");
      EXPECT_TRUE(verible::IsSubRange(ref.identifier,
                                      src.GetTextStructure()->Contents()));
      EXPECT_EQ(ref.ref_type, ReferenceType::kUnqualified);
      EXPECT_EQ(ref.required_metatype, SymbolMetaType::kUnspecified);
      EXPECT_EQ(ref.resolved_symbol, nullptr);
    }
    {  // self-reference to "pp_inst" instance
      ASSIGN_MUST_FIND_EXACTLY_ONE_REF(pp_inst_self_ref, ref_map, "pp_inst");
      EXPECT_TRUE(is_leaf(*pp_inst_self_ref->components));  // no named ports
      // self-reference is already bound.
      EXPECT_EQ(pp_inst_self_ref->components->Value().resolved_symbol,
                &pp_inst);
    }
  }
  {
    ASSERT_EQ(ss_info.local_references_to_bind.size(), 2);
    const auto ref_map(ss_info.LocalReferencesMapViewForTesting());
    {
      ASSIGN_MUST_FIND_EXACTLY_ONE_REF(qq_type, ref_map, "qq");
      const ReferenceComponentNode *ref_node = qq_type->LastTypeComponent();
      ASSERT_NE(ref_node, nullptr);
      const ReferenceComponent &ref(ref_node->Value());
      EXPECT_EQ(ref.identifier, "qq");
      EXPECT_TRUE(verible::IsSubRange(ref.identifier,
                                      src.GetTextStructure()->Contents()));
      EXPECT_EQ(ref.ref_type, ReferenceType::kUnqualified);
      EXPECT_EQ(ref.required_metatype, SymbolMetaType::kUnspecified);
      EXPECT_EQ(ref.resolved_symbol, nullptr);
    }
    {  // self-reference to "qq_inst" instance
      ASSIGN_MUST_FIND_EXACTLY_ONE_REF(qq_inst_self_ref, ref_map, "qq_inst");
      EXPECT_TRUE(is_leaf(*qq_inst_self_ref->components));  // no named ports
      // self-reference is already bound.
      EXPECT_EQ(qq_inst_self_ref->components->Value().resolved_symbol,
                &qq_inst);
    }
  }

  {  // Verify ss_inst's type info
    EXPECT_TRUE(ss_inst_info.local_references_to_bind.empty());
    EXPECT_NE(ss_inst_info.declared_type.user_defined_type, nullptr);
    const ReferenceComponent &ss_type(
        ss_inst_info.declared_type.user_defined_type->Value());
    EXPECT_EQ(ss_type.identifier, "ss");
    EXPECT_EQ(ss_type.resolved_symbol, nullptr);  // nothing resolved yet
    EXPECT_EQ(ss_type.ref_type, ReferenceType::kUnqualified);
    EXPECT_EQ(ss_type.required_metatype, SymbolMetaType::kUnspecified);
    EXPECT_EQ(ss_inst_info.file_origin, &src);
  }

  {  // Verify pp_inst's type info
    EXPECT_TRUE(pp_inst_info.local_references_to_bind.empty());
    EXPECT_NE(pp_inst_info.declared_type.user_defined_type, nullptr);
    const ReferenceComponent &pp_type(
        pp_inst_info.declared_type.user_defined_type->Value());
    EXPECT_EQ(pp_type.identifier, "pp");
    EXPECT_EQ(pp_type.resolved_symbol, nullptr);  // nothing resolved yet
    EXPECT_EQ(pp_type.ref_type, ReferenceType::kUnqualified);
    EXPECT_EQ(pp_type.required_metatype, SymbolMetaType::kUnspecified);
    EXPECT_EQ(pp_inst_info.file_origin, &src);
  }

  {  // Verify qq_inst's type info
    EXPECT_TRUE(qq_inst_info.local_references_to_bind.empty());
    EXPECT_NE(qq_inst_info.declared_type.user_defined_type, nullptr);
    const ReferenceComponent &qq_type(
        qq_inst_info.declared_type.user_defined_type->Value());
    EXPECT_EQ(qq_type.identifier, "qq");
    EXPECT_EQ(qq_type.resolved_symbol, nullptr);  // nothing resolved yet
    EXPECT_EQ(qq_type.ref_type, ReferenceType::kUnqualified);
    EXPECT_EQ(qq_type.required_metatype, SymbolMetaType::kUnspecified);
    EXPECT_EQ(qq_inst_info.file_origin, &src);
  }

  // Resolve symbols.
  std::vector<absl::Status> resolve_diagnostics;
  symbol_table.Resolve(&resolve_diagnostics);
  EXPECT_EMPTY_STATUSES(resolve_diagnostics);

  // Verify that typeof(ss_inst) successfully resolved to module ss.
  EXPECT_EQ(
      ss_inst_info.declared_type.user_defined_type->Value().resolved_symbol,
      &ss);
  // Verify that typeof(pp_inst) successfully resolved to module pp.
  EXPECT_EQ(
      pp_inst_info.declared_type.user_defined_type->Value().resolved_symbol,
      &pp);
  // Verify that typeof(qq_inst) successfully resolved to module qq.
  EXPECT_EQ(
      qq_inst_info.declared_type.user_defined_type->Value().resolved_symbol,
      &qq);
}

TEST(BuildSymbolTableTest, MultiFileModuleInstanceCyclicDependencies) {
  // Cyclic dependencies among three files.
  // Make sure this can still build and resolve without hanging,
  // even if this is semantically illegal.
  TestVerilogSourceFile pp_src("pp.sv",
                               "module pp;\n"
                               "  ss ss_inst();\n"  // instance
                               "endmodule\n");
  TestVerilogSourceFile qq_src("qq.sv",
                               "module qq;\n"
                               "  pp pp_inst();\n"  // instance
                               "endmodule\n");
  TestVerilogSourceFile ss_src("ss.sv",
                               "module ss;\n"
                               "  qq qq_inst();\n"  // instance
                               "endmodule\n");
  {
    const auto status = pp_src.Parse();
    ASSERT_TRUE(status.ok()) << status.message();
  }
  {
    const auto status = qq_src.Parse();
    ASSERT_TRUE(status.ok()) << status.message();
  }
  {
    const auto status = ss_src.Parse();
    ASSERT_TRUE(status.ok()) << status.message();
  }

  // All permutations of the following file ordering should end up with the
  // same results.
  std::vector<const TestVerilogSourceFile *> ordering(
      {&pp_src, &qq_src, &ss_src});
  // start with the lexicographically "lowest" permutation
  SortSourceFiles(&ordering);
  int count = 0;
  do {
    SymbolTable symbol_table(nullptr);
    const SymbolTableNode &root_symbol(symbol_table.Root());

    for (const auto *src : ordering) {
      const auto build_diagnostics = BuildSymbolTable(*src, &symbol_table);
      EXPECT_EMPTY_STATUSES(build_diagnostics);
    }

    // Goal: resolve the reference of "pp" to this definition node.
    MUST_ASSIGN_LOOKUP_SYMBOL(pp, root_symbol, "pp");

    // Inspect inside the "qq" module definition.
    MUST_ASSIGN_LOOKUP_SYMBOL(qq, root_symbol, "qq");

    MUST_ASSIGN_LOOKUP_SYMBOL(ss, root_symbol, "ss");

    // "ss_inst" is an instance of type "ss"
    MUST_ASSIGN_LOOKUP_SYMBOL(ss_inst, pp, "ss_inst");

    // "pp_inst" is an instance of type "pp"
    MUST_ASSIGN_LOOKUP_SYMBOL(pp_inst, qq, "pp_inst");

    // "qq_inst" is an instance of type "qq"
    MUST_ASSIGN_LOOKUP_SYMBOL(qq_inst, ss, "qq_inst");

    EXPECT_EQ(pp_info.file_origin, &pp_src);
    EXPECT_EQ(qq_info.file_origin, &qq_src);
    EXPECT_EQ(ss_info.file_origin, &ss_src);
    {
      ASSERT_EQ(pp_info.local_references_to_bind.size(), 2);
      const auto ref_map(pp_info.LocalReferencesMapViewForTesting());
      {
        ASSIGN_MUST_FIND_EXACTLY_ONE_REF(ss_type, ref_map, "ss");
        const ReferenceComponentNode *ref_node = ss_type->LastTypeComponent();
        ASSERT_NE(ref_node, nullptr);
        const ReferenceComponent &ref(ref_node->Value());
        EXPECT_EQ(ref.identifier, "ss");
        EXPECT_TRUE(verible::IsSubRange(ref.identifier,
                                        pp_src.GetTextStructure()->Contents()));
        EXPECT_EQ(ref.ref_type, ReferenceType::kUnqualified);
        EXPECT_EQ(ref.required_metatype, SymbolMetaType::kUnspecified);
        EXPECT_EQ(ref.resolved_symbol, nullptr);
      }
      {  // self-reference to "ss_inst" instance
        ASSIGN_MUST_FIND_EXACTLY_ONE_REF(ss_inst_self_ref, ref_map, "ss_inst");
        EXPECT_TRUE(is_leaf(*ss_inst_self_ref->components));  // no named ports
        // self-reference is already bound.
        EXPECT_EQ(ss_inst_self_ref->components->Value().resolved_symbol,
                  &ss_inst);
      }
    }
    {
      ASSERT_EQ(qq_info.local_references_to_bind.size(), 2);
      const auto ref_map(qq_info.LocalReferencesMapViewForTesting());
      {
        ASSIGN_MUST_FIND_EXACTLY_ONE_REF(pp_type, ref_map, "pp");
        const ReferenceComponentNode *ref_node = pp_type->LastTypeComponent();
        ASSERT_NE(ref_node, nullptr);
        const ReferenceComponent &ref(ref_node->Value());
        EXPECT_EQ(ref.identifier, "pp");
        EXPECT_TRUE(verible::IsSubRange(ref.identifier,
                                        qq_src.GetTextStructure()->Contents()));
        EXPECT_EQ(ref.ref_type, ReferenceType::kUnqualified);
        EXPECT_EQ(ref.required_metatype, SymbolMetaType::kUnspecified);
        EXPECT_EQ(ref.resolved_symbol, nullptr);
      }
      {  // self-reference to "pp_inst" instance
        ASSIGN_MUST_FIND_EXACTLY_ONE_REF(pp_inst_self_ref, ref_map, "pp_inst");
        EXPECT_TRUE(is_leaf(*pp_inst_self_ref->components));  // no named ports
        // self-reference is already bound.
        EXPECT_EQ(pp_inst_self_ref->components->Value().resolved_symbol,
                  &pp_inst);
      }
    }
    {
      ASSERT_EQ(ss_info.local_references_to_bind.size(), 2);
      const auto ref_map(ss_info.LocalReferencesMapViewForTesting());
      {
        ASSIGN_MUST_FIND_EXACTLY_ONE_REF(qq_type, ref_map, "qq");
        const ReferenceComponentNode *ref_node = qq_type->LastTypeComponent();
        ASSERT_NE(ref_node, nullptr);
        const ReferenceComponent &ref(ref_node->Value());
        EXPECT_EQ(ref.identifier, "qq");
        EXPECT_TRUE(verible::IsSubRange(ref.identifier,
                                        ss_src.GetTextStructure()->Contents()));
        EXPECT_EQ(ref.ref_type, ReferenceType::kUnqualified);
        EXPECT_EQ(ref.required_metatype, SymbolMetaType::kUnspecified);
        EXPECT_EQ(ref.resolved_symbol, nullptr);
      }
      {  // self-reference to "qq_inst" instance
        ASSIGN_MUST_FIND_EXACTLY_ONE_REF(qq_inst_self_ref, ref_map, "qq_inst");
        EXPECT_TRUE(is_leaf(*qq_inst_self_ref->components));  // no named ports
        // self-reference is already bound.
        EXPECT_EQ(qq_inst_self_ref->components->Value().resolved_symbol,
                  &qq_inst);
      }
    }

    {  // Verify ss_inst's type info
      EXPECT_TRUE(ss_inst_info.local_references_to_bind.empty());
      EXPECT_NE(ss_inst_info.declared_type.user_defined_type, nullptr);
      const ReferenceComponent &ss_type(
          ss_inst_info.declared_type.user_defined_type->Value());
      EXPECT_EQ(ss_type.identifier, "ss");
      EXPECT_EQ(ss_type.resolved_symbol, nullptr);  // nothing resolved yet
      EXPECT_EQ(ss_type.ref_type, ReferenceType::kUnqualified);
      EXPECT_EQ(ss_type.required_metatype, SymbolMetaType::kUnspecified);
      EXPECT_EQ(ss_inst_info.file_origin, &pp_src);
    }

    {  // Verify pp_inst's type info
      EXPECT_TRUE(pp_inst_info.local_references_to_bind.empty());
      EXPECT_NE(pp_inst_info.declared_type.user_defined_type, nullptr);
      const ReferenceComponent &pp_type(
          pp_inst_info.declared_type.user_defined_type->Value());
      EXPECT_EQ(pp_type.identifier, "pp");
      EXPECT_EQ(pp_type.resolved_symbol, nullptr);  // nothing resolved yet
      EXPECT_EQ(pp_type.ref_type, ReferenceType::kUnqualified);
      EXPECT_EQ(pp_type.required_metatype, SymbolMetaType::kUnspecified);
      EXPECT_EQ(pp_inst_info.file_origin, &qq_src);
    }

    {  // Verify qq_inst's type info
      EXPECT_TRUE(qq_inst_info.local_references_to_bind.empty());
      EXPECT_NE(qq_inst_info.declared_type.user_defined_type, nullptr);
      const ReferenceComponent &qq_type(
          qq_inst_info.declared_type.user_defined_type->Value());
      EXPECT_EQ(qq_type.identifier, "qq");
      EXPECT_EQ(qq_type.resolved_symbol, nullptr);  // nothing resolved yet
      EXPECT_EQ(qq_type.ref_type, ReferenceType::kUnqualified);
      EXPECT_EQ(qq_type.required_metatype, SymbolMetaType::kUnspecified);
      EXPECT_EQ(qq_inst_info.file_origin, &ss_src);
    }

    // Resolve symbols.
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);

    // Verify that typeof(ss_inst) successfully resolved to module ss.
    EXPECT_EQ(
        ss_inst_info.declared_type.user_defined_type->Value().resolved_symbol,
        &ss);
    // Verify that typeof(pp_inst) successfully resolved to module pp.
    EXPECT_EQ(
        pp_inst_info.declared_type.user_defined_type->Value().resolved_symbol,
        &pp);
    // Verify that typeof(qq_inst) successfully resolved to module qq.
    EXPECT_EQ(
        qq_inst_info.declared_type.user_defined_type->Value().resolved_symbol,
        &qq);
    ++count;
  } while (PermuteSourceFiles(&ordering));
  EXPECT_EQ(count, 6);  // make sure we covered all permutations
}

TEST(BuildSymbolTableTest, IncludeModuleDefinition) {
  const auto tempdir = ::testing::TempDir();
  const std::string sources_dir = JoinPath(tempdir, __FUNCTION__);
  ASSERT_TRUE(CreateDir(sources_dir).ok());

  // Create files.
  ScopedTestFile IncludedFile(sources_dir,
                              "module pp;\n"
                              "endmodule\n",
                              "module.sv");
  ScopedTestFile pp_src(sources_dir, "`include \"module.sv\"\n", "pp.sv");

  VerilogProject project(sources_dir, {sources_dir});
  const auto file_or_status =
      project.OpenTranslationUnit(Basename(pp_src.filename()));
  ASSERT_TRUE(file_or_status.ok()) << file_or_status.status().message();

  SymbolTable symbol_table(&project);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  std::vector<absl::Status> build_diagnostics;
  symbol_table.Build(&build_diagnostics);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(pp, root_symbol, "pp");

  const VerilogSourceFile *included = project.LookupRegisteredFile("module.sv");
  ASSERT_NE(included, nullptr);
  EXPECT_EQ(pp_info.file_origin, included);

  // Resolve symbols.  Nothing to resolve.
  std::vector<absl::Status> resolve_diagnostics;
  symbol_table.Resolve(&resolve_diagnostics);
  EXPECT_EMPTY_STATUSES(resolve_diagnostics);
}

TEST(BuildSymbolTableTest, IncludeWithoutProject) {
  const auto tempdir = ::testing::TempDir();
  const std::string sources_dir = JoinPath(tempdir, __FUNCTION__);
  ASSERT_TRUE(CreateDir(sources_dir).ok());

  // Create files.
  ScopedTestFile IncludedFile(sources_dir,
                              "module pp;\n"
                              "endmodule\n",
                              "module.sv");
  TestVerilogSourceFile pp_src("pp.sv", "`include \"module.sv\"\n");

  SymbolTable symbol_table(nullptr);

  const auto build_diagnostics = BuildSymbolTable(pp_src, nullptr);
  // include files are ignored.
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  // Resolve symbols.  Nothing to resolve.
  std::vector<absl::Status> resolve_diagnostics;
  symbol_table.Resolve(&resolve_diagnostics);
  EXPECT_EMPTY_STATUSES(resolve_diagnostics);
}

TEST(BuildSymbolTableTest, IncludeFileNotFound) {
  const auto tempdir = ::testing::TempDir();
  const std::string sources_dir = JoinPath(tempdir, __FUNCTION__);
  ASSERT_TRUE(CreateDir(sources_dir).ok());

  // Create files.
  ScopedTestFile pp_src(sources_dir, "`include \"not-found.sv\"\n", "pp.sv");

  VerilogProject project(sources_dir, {sources_dir});
  const auto file_or_status =
      project.OpenTranslationUnit(Basename(pp_src.filename()));
  ASSERT_TRUE(file_or_status.ok()) << file_or_status.status().message();

  SymbolTable symbol_table(&project);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  std::vector<absl::Status> build_diagnostics;
  symbol_table.Build(&build_diagnostics);
  ASSERT_FALSE(build_diagnostics.empty());
  EXPECT_EQ(build_diagnostics.front().code(), absl::StatusCode::kNotFound);

  EXPECT_TRUE(root_symbol.Children().empty());

  // Resolve symbols.  Nothing to resolve.
  std::vector<absl::Status> resolve_diagnostics;
  symbol_table.Resolve(&resolve_diagnostics);
  EXPECT_EMPTY_STATUSES(resolve_diagnostics);
}

TEST(BuildSymbolTableTest, IncludeFileParseError) {
  const auto tempdir = ::testing::TempDir();
  const std::string sources_dir = JoinPath(tempdir, __FUNCTION__);
  ASSERT_TRUE(CreateDir(sources_dir).ok());

  // Create files.
  ScopedTestFile IncludedFile(sources_dir,
                              "module 333;\n"  // syntax error
                              "endmodule\n",
                              "module.sv");
  ScopedTestFile pp_src(sources_dir, "`include \"module.sv\"\n", "pp.sv");

  VerilogProject project(sources_dir, {sources_dir});
  const auto file_or_status =
      project.OpenTranslationUnit(Basename(pp_src.filename()));
  ASSERT_TRUE(file_or_status.ok()) << file_or_status.status().message();

  SymbolTable symbol_table(&project);

  std::vector<absl::Status> build_diagnostics;
  symbol_table.Build(&build_diagnostics);
  ASSERT_FALSE(build_diagnostics.empty());
  EXPECT_EQ(build_diagnostics.front().code(),
            absl::StatusCode::kInvalidArgument);

  // Resolve symbols.  Nothing to resolve.
  std::vector<absl::Status> resolve_diagnostics;
  symbol_table.Resolve(&resolve_diagnostics);
  EXPECT_EMPTY_STATUSES(resolve_diagnostics);
}

TEST(BuildSymbolTableTest, IncludeFileEmpty) {
  const auto tempdir = ::testing::TempDir();
  const std::string sources_dir = JoinPath(tempdir, __FUNCTION__);
  ASSERT_TRUE(CreateDir(sources_dir).ok());

  // Create files.
  ScopedTestFile IncludedFile(sources_dir,
                              "",  // empty
                              "empty.sv");
  ScopedTestFile pp_src(sources_dir, "`include \"empty.sv\"\n", "pp.sv");

  VerilogProject project(sources_dir, {sources_dir});
  const auto file_or_status =
      project.OpenTranslationUnit(Basename(pp_src.filename()));
  ASSERT_TRUE(file_or_status.ok()) << file_or_status.status().message();

  SymbolTable symbol_table(&project);

  std::vector<absl::Status> build_diagnostics;
  symbol_table.Build(&build_diagnostics);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  // Resolve symbols.  Nothing to resolve.
  std::vector<absl::Status> resolve_diagnostics;
  symbol_table.Resolve(&resolve_diagnostics);
  EXPECT_EMPTY_STATUSES(resolve_diagnostics);
}

TEST(BuildSymbolTableTest, IncludedTwiceFromOneFile) {
  const auto tempdir = ::testing::TempDir();
  const std::string sources_dir = JoinPath(tempdir, __FUNCTION__);
  ASSERT_TRUE(CreateDir(sources_dir).ok());

  // Create files.
  ScopedTestFile IncludedFile(sources_dir,
                              "// verilog_syntax: parse-as-module-body\n"
                              "wire ww;\n",
                              "wires.sv");
  ScopedTestFile pp_src(sources_dir,
                        "module pp;\n"
                        "`include \"wires.sv\"\n"
                        "endmodule\n"
                        "module qq;\n"
                        "`include \"wires.sv\"\n"
                        "endmodule\n",
                        "pp.sv");

  VerilogProject project(sources_dir, {sources_dir});
  const auto file_or_status =
      project.OpenTranslationUnit(Basename(pp_src.filename()));
  ASSERT_TRUE(file_or_status.ok()) << file_or_status.status().message();
  const VerilogSourceFile *pp_file = *file_or_status;
  ASSERT_NE(pp_file, nullptr);

  SymbolTable symbol_table(&project);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  std::vector<absl::Status> build_diagnostics;
  symbol_table.Build(&build_diagnostics);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(pp, root_symbol, "pp");
  MUST_ASSIGN_LOOKUP_SYMBOL(qq, root_symbol, "qq");
  MUST_ASSIGN_LOOKUP_SYMBOL(pp_ww, pp, "ww");
  MUST_ASSIGN_LOOKUP_SYMBOL(qq_ww, qq, "ww");

  const VerilogSourceFile *included = project.LookupRegisteredFile("wires.sv");
  ASSERT_NE(included, nullptr);
  EXPECT_EQ(pp_info.file_origin, pp_file);
  EXPECT_EQ(qq_info.file_origin, pp_file);
  EXPECT_EQ(pp_ww_info.file_origin, included);
  EXPECT_EQ(qq_ww_info.file_origin, included);

  // Resolve symbols.  Nothing to resolve.
  std::vector<absl::Status> resolve_diagnostics;
  symbol_table.Resolve(&resolve_diagnostics);
  EXPECT_EMPTY_STATUSES(resolve_diagnostics);
}

TEST(BuildSymbolTableTest, IncludedTwiceFromDifferentFiles) {
  const auto tempdir = ::testing::TempDir();
  const std::string sources_dir = JoinPath(tempdir, __FUNCTION__);
  ASSERT_TRUE(CreateDir(sources_dir).ok());

  // Create files.
  ScopedTestFile IncludedFile(sources_dir,
                              "// verilog_syntax: parse-as-module-body\n"
                              "wire ww;\n",
                              "wires.sv");
  ScopedTestFile pp_src(sources_dir,
                        "module pp;\n"
                        "`include \"wires.sv\"\n"
                        "endmodule\n",
                        "pp.sv");
  ScopedTestFile qq_src(sources_dir,
                        "module qq;\n"
                        "`include \"wires.sv\"\n"
                        "endmodule\n",
                        "qq.sv");

  VerilogProject project(sources_dir, {sources_dir});

  const auto pp_file_or_status =
      project.OpenTranslationUnit(Basename(pp_src.filename()));
  ASSERT_TRUE(pp_file_or_status.ok()) << pp_file_or_status.status().message();
  const VerilogSourceFile *pp_file = *pp_file_or_status;
  ASSERT_NE(pp_file, nullptr);

  const auto qq_file_or_status =
      project.OpenTranslationUnit(Basename(qq_src.filename()));
  ASSERT_TRUE(qq_file_or_status.ok()) << qq_file_or_status.status().message();
  const VerilogSourceFile *qq_file = *qq_file_or_status;
  ASSERT_NE(qq_file, nullptr);

  SymbolTable symbol_table(&project);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  std::vector<absl::Status> build_diagnostics;
  symbol_table.Build(&build_diagnostics);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(pp, root_symbol, "pp");
  MUST_ASSIGN_LOOKUP_SYMBOL(qq, root_symbol, "qq");
  MUST_ASSIGN_LOOKUP_SYMBOL(pp_ww, pp, "ww");
  MUST_ASSIGN_LOOKUP_SYMBOL(qq_ww, qq, "ww");

  const VerilogSourceFile *included = project.LookupRegisteredFile("wires.sv");
  ASSERT_NE(included, nullptr);
  EXPECT_EQ(pp_info.file_origin, pp_file);
  EXPECT_EQ(qq_info.file_origin, qq_file);
  EXPECT_EQ(pp_ww_info.file_origin, included);
  EXPECT_EQ(qq_ww_info.file_origin, included);

  // Resolve symbols.  Nothing to resolve.
  std::vector<absl::Status> resolve_diagnostics;
  symbol_table.Resolve(&resolve_diagnostics);
  EXPECT_EMPTY_STATUSES(resolve_diagnostics);
}

TEST(BuildSymbolTableTest, ModulePortDeclarationMultiline) {
  TestVerilogSourceFile src("foobar.sv",
                            "module a; endmodule\n"
                            "module m(mport);\n"
                            "  input mport;\n"
                            "  wire mport;\n"
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  EXPECT_EMPTY_STATUSES(build_diagnostics);
}

TEST(BuildSymbolTableTest, ModulePortDeclarationDirectionRedefinition) {
  TestVerilogSourceFile src(
      "foobar.sv",
      "module m(mport);\n"
      "  input mport;\n"
      "  output mport;\n"  // direction is already declared
      "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(module_node, root_symbol, "m");
  EXPECT_EQ(module_node_info.metatype, SymbolMetaType::kModule);
  EXPECT_EQ(module_node_info.file_origin, &src);
  EXPECT_EQ(module_node_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type

  ASSIGN_MUST_HAVE_UNIQUE(err_status, build_diagnostics);
  EXPECT_EQ(err_status.code(), absl::StatusCode::kAlreadyExists);
  EXPECT_THAT(err_status.message(),
              HasSubstr("\"mport\" is already defined in the $root::m scope"));

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, ModulePortDeclarationTypeRedefinition) {
  TestVerilogSourceFile src("foobar.sv",
                            "module a; endmodule\n"
                            "module m(mport);\n"
                            "  input mport;\n"
                            "  wire mport;\n"
                            "  logic mport;\n"
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(module_node, root_symbol, "m");
  EXPECT_EQ(module_node_info.metatype, SymbolMetaType::kModule);
  EXPECT_EQ(module_node_info.file_origin, &src);
  EXPECT_EQ(module_node_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type

  ASSIGN_MUST_HAVE_UNIQUE(err_status, build_diagnostics);
  EXPECT_EQ(err_status.code(), absl::StatusCode::kAlreadyExists);
  EXPECT_THAT(err_status.message(),
              HasSubstr("\"mport\" is already defined in the $root::m scope"));

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, ModulePortDeclarationTypeMultilineWithDimensions) {
  TestVerilogSourceFile src("foobar.sv",
                            "module m(mport);\n"
                            "  input [10:0] mport;\n"
                            "  reg [10:0] mport;\n"
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  EXPECT_EMPTY_STATUSES(build_diagnostics);
}

TEST(BuildSymbolTableTest,
     ModulePortDeclarationTypeMultilineWithMismatchingDimensions) {
  TestVerilogSourceFile src("foobar.sv",
                            "module m(mport);\n"
                            "  input [10:0] mport;\n"
                            "  reg [8:0] mport;\n"
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(module_node, root_symbol, "m");
  EXPECT_EQ(module_node_info.metatype, SymbolMetaType::kModule);
  EXPECT_EQ(module_node_info.file_origin, &src);
  EXPECT_EQ(module_node_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type

  ASSIGN_MUST_HAVE_UNIQUE(err_status, build_diagnostics);
  EXPECT_EQ(err_status.code(), absl::StatusCode::kAlreadyExists);
  EXPECT_THAT(err_status.message(),
              HasSubstr("\"mport\" is already defined in the $root::m scope"));

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest,
     ModulePortDeclarationTypeMultilineCorrectSignPlacements) {
  TestVerilogSourceFile src("foobar.sv",
                            "module m(a, b, c, d);\n"
                            "  input signed [10:0] a;\n"
                            "  output unsigned [10:0] b;\n"
                            "  input [10:0] c;\n"
                            "  output [10:0] d;\n"
                            "  wire [10:0] a;\n"
                            "  logic [10:0] b;\n"
                            "  logic unsigned [10:0] c;\n"
                            "  wire signed [10:0] d;\n"
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  EXPECT_EMPTY_STATUSES(build_diagnostics);
}

TEST(BuildSymbolTableTest,
     ModulePortDeclarationTypeMultilineWithMismatchingSigns) {
  TestVerilogSourceFile src("foobar.sv",
                            "module m(mport);\n"
                            "  input unsigned [10:0] mport;\n"
                            "  reg signed [10:0] mport;\n"
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(module_node, root_symbol, "m");
  EXPECT_EQ(module_node_info.metatype, SymbolMetaType::kModule);
  EXPECT_EQ(module_node_info.file_origin, &src);
  EXPECT_EQ(module_node_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type

  ASSIGN_MUST_HAVE_UNIQUE(err_status, build_diagnostics);
  EXPECT_EQ(err_status.code(), absl::StatusCode::kAlreadyExists);
  EXPECT_THAT(err_status.message(),
              HasSubstr("\"mport\" is already defined in the $root::m scope"));

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, ModulePortDeclarationTypeMultilineWithPortList) {
  TestVerilogSourceFile src("foobar.sv",
                            "module m(a, b, c);\n"
                            "  input a, b;\n"
                            "  output b, c;\n"
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(module_node, root_symbol, "m");
  EXPECT_EQ(module_node_info.metatype, SymbolMetaType::kModule);
  EXPECT_EQ(module_node_info.file_origin, &src);
  EXPECT_EQ(module_node_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type

  ASSIGN_MUST_HAVE_UNIQUE(err_status, build_diagnostics);
  EXPECT_EQ(err_status.code(), absl::StatusCode::kAlreadyExists);
  EXPECT_THAT(err_status.message(),
              HasSubstr("\"b\" is already defined in the $root::m scope"));

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, InterfaceDeclarationSingleEmpty) {
  TestVerilogSourceFile src("foobar_if.sv",
                            "interface foobar_if;\n"
                            "endinterface\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(interface_node, root_symbol, "foobar_if");
  EXPECT_EQ(interface_node_info.metatype, SymbolMetaType::kInterface);
  EXPECT_EQ(interface_node_info.file_origin, &src);
  EXPECT_EQ(interface_node_info.declared_type.syntax_origin,
            nullptr);  // there is no interface meta-type
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, InterfaceDeclarationLocalNetsVariables) {
  TestVerilogSourceFile src("foobar_if.sv",
                            "interface foobar_if;\n"
                            "  logic l1;\n"
                            "  logic l2;\n"
                            "endinterface\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(interface_node, root_symbol, "foobar_if");
  EXPECT_EQ(interface_node_info.metatype, SymbolMetaType::kInterface);
  EXPECT_EQ(interface_node_info.file_origin, &src);
  EXPECT_EQ(interface_node_info.declared_type.syntax_origin,
            nullptr);  // there is no interface meta-type
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  static constexpr std::string_view members[] = {"l1", "l2"};
  for (const auto &member : members) {
    MUST_ASSIGN_LOOKUP_SYMBOL(member_node, interface_node, member);
    EXPECT_EQ(member_node_info.metatype,
              SymbolMetaType::kDataNetVariableInstance);
    EXPECT_EQ(member_node_info.declared_type.user_defined_type,
              nullptr);  // types are primitive
  }

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, InterfaceDeclarationWithPorts) {
  TestVerilogSourceFile src("foobar_if.sv",
                            "interface foobar_if (\n"
                            "  input wire clk,\n"
                            "  input logic reset\n"
                            ");\n"
                            "  logic d;"
                            "  logic q;"
                            "  modport dff ("
                            "    input d,"
                            "    output q);"
                            "  modport dff_test ("
                            "    output d,"
                            "    input q);"
                            "endinterface\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  MUST_ASSIGN_LOOKUP_SYMBOL(interface_node, root_symbol, "foobar_if");
  EXPECT_EQ(interface_node_info.metatype, SymbolMetaType::kInterface);
  EXPECT_EQ(interface_node_info.file_origin, &src);
  EXPECT_EQ(interface_node_info.declared_type.syntax_origin,
            nullptr);  // there is no interface meta-type

  static constexpr std::string_view members[] = {"clk", "reset", "d", "q"};
  for (const auto &member : members) {
    MUST_ASSIGN_LOOKUP_SYMBOL(member_node, interface_node, member);
    EXPECT_EQ(member_node_info.metatype,
              SymbolMetaType::kDataNetVariableInstance);
    EXPECT_EQ(member_node_info.declared_type.user_defined_type,
              nullptr);  // types are primitive
  }

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, InterfaceDeclarationMultiple) {
  TestVerilogSourceFile src("foobar_if.sv",
                            "interface foobar1_if;\nendinterface\n"
                            "interface foobar2_if;\nendinterface\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_EMPTY_STATUSES(build_diagnostics);

  const std::string_view expected_interfaces[] = {"foobar1_if", "foobar2_if"};
  for (const auto &expected_interface : expected_interfaces) {
    MUST_ASSIGN_LOOKUP_SYMBOL(interface_node, root_symbol, expected_interface);
    EXPECT_EQ(interface_node_info.metatype, SymbolMetaType::kInterface);
    EXPECT_EQ(interface_node_info.file_origin, &src);
    EXPECT_EQ(interface_node_info.declared_type.syntax_origin,
              nullptr);  // there is no interface meta-type
  }

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, InterfaceDeclarationDuplicate) {
  TestVerilogSourceFile src("foobar_if.sv",
                            "interface foobar_if;\nendinterface\n"
                            "interface foobar_if;\nendinterface\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(interface_node, root_symbol, "foobar_if");
  EXPECT_EQ(interface_node_info.metatype, SymbolMetaType::kInterface);
  EXPECT_EQ(interface_node_info.file_origin, &src);
  EXPECT_EQ(interface_node_info.declared_type.syntax_origin,
            nullptr);  // there is no interface meta-type

  ASSIGN_MUST_HAVE_UNIQUE(err, build_diagnostics);
  EXPECT_EQ(err.code(), absl::StatusCode::kAlreadyExists);
  EXPECT_THAT(err.message(),
              HasSubstr("\"foobar_if\" is already defined in the $root scope"));

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

TEST(BuildSymbolTableTest, InterfaceDeclarationDuplicateSeparateFiles) {
  TestVerilogSourceFile src("foobar_if.sv",
                            "interface foobar_if;\nendinterface\n");
  TestVerilogSourceFile src2("foobar_if-2.sv",
                             "interface foobar_if;\nendinterface\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  const auto status2 = src2.Parse();
  ASSERT_TRUE(status2.ok()) << status2.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode &root_symbol(symbol_table.Root());

  const auto build_diagnostics1 = BuildSymbolTable(src, &symbol_table);
  const auto build_diagnostics = BuildSymbolTable(src2, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(interface_node, root_symbol, "foobar_if");
  EXPECT_EQ(interface_node_info.metatype, SymbolMetaType::kInterface);
  EXPECT_EQ(interface_node_info.file_origin, &src);
  EXPECT_EQ(interface_node_info.declared_type.syntax_origin,
            nullptr);  // there is no interface meta-type

  ASSIGN_MUST_HAVE_UNIQUE(err, build_diagnostics);
  EXPECT_EQ(err.code(), absl::StatusCode::kAlreadyExists);
  EXPECT_THAT(err.message(),
              HasSubstr("\"foobar_if\" is already defined in the $root scope"));

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_EMPTY_STATUSES(resolve_diagnostics);
  }
}

struct FileListTestCase {
  std::string_view contents;
  std::vector<std::string_view> expected_files;
};

TEST(ParseSourceFileListFromFileTest, FileNotFound) {
  FileList file_list;
  const auto status(AppendFileListFromFile("/no/such/file.txt", &file_list));
  EXPECT_FALSE(status.ok());
}

TEST(ParseSourceFileListFromFileTest, VariousValidFiles) {
  const FileListTestCase kTestCases[] = {
      {"", {}},                // empty
      {"\n\n", {}},            // blank lines
      {"foo.sv", {"foo.sv"}},  // missing terminating newline, but still works
      {"foo.sv\n", {"foo.sv"}},
      {"file name contains space.sv\n", {"file name contains space.sv"}},
      {"foo/bar.sv\n", {"foo/bar.sv"}},  // with path separator
      {" foo.sv\n", {"foo.sv"}},         // remove leading whitespace
      {"foo.sv \n", {"foo.sv"}},         // remove trailing whitespace
      {"#foo.sv\n", {}},                 // commented out
      {"# foo.sv\n", {}},                // commented out
      {"foo.sv\nbar/bar.sv\n", {"foo.sv", "bar/bar.sv"}},
      {"/foo/bar.sv\n"
       "### ignore this one\n"
       "bar/baz.txt\n",
       {"/foo/bar.sv", "bar/baz.txt"}},
  };
  for (const auto &test : kTestCases) {
    const ScopedTestFile test_file(testing::TempDir(), test.contents);
    FileList file_list;
    const auto status(AppendFileListFromFile(test_file.filename(), &file_list));
    ASSERT_TRUE(status.ok()) << status;
    EXPECT_THAT(file_list.file_paths, ElementsAreArray(test.expected_files))
        << "input: " << test.contents;
  }
}

}  // namespace
}  // namespace verilog
