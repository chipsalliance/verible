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

#include "verilog/analysis/symbol_table.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <sstream>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/base/attributes.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "common/text/tree_utils.h"
#include "common/util/file_util.h"
#include "common/util/logging.h"
#include "common/util/range.h"
#include "verilog/analysis/verilog_project.h"

namespace verilog {

// Directly test some SymbolTable internals.
class SymbolTable::Tester : public SymbolTable {
 public:
  explicit Tester(VerilogProject* project) : SymbolTable(project) {}

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
  const SymbolTableNode& node;
};

static std::ostream& operator<<(std::ostream& stream,
                                const ScopePathPrinter& p) {
  return SymbolTableNodeFullPath(stream, p.node);
}

// Assert that map/set element exists at 'key', and assigns it to 'dest'
// 'key' must be printable for failure diagnostic message.
// Defined as a macro for meaningful line numbers on failure.
#define ASSIGN_MUST_FIND(dest, map, key)                 \
  const auto found_##dest(map.find(key)); /* iterator */ \
  ASSERT_NE(found_##dest, map.end())                     \
      << "No element at \"" << key << "\" in " #map;     \
  const auto& dest ABSL_ATTRIBUTE_UNUSED(                \
      found_##dest->second); /* mapped_type */

// Assert that container is not empty, and reference its first element.
// Works on any container type with .begin().
// Defined as a macro for meaningful line numbers on failure.
#define ASSIGN_MUST_HAVE_FIRST_ELEMENT(dest, container) \
  ASSERT_FALSE(container.empty());                      \
  const auto& dest(*container.begin());

// Assert that container has exactly one-element, and reference it.
// Works on any container with .size() and .begin().
// Defined as a macro for meaningful line numbers on failure.
#define ASSIGN_MUST_HAVE_UNIQUE(dest, container) \
  ASSERT_EQ(container.size(), 1);                \
  const auto& dest(*container.begin());

// Shorthand for asserting that a symbol table lookup from
// (const SymbolTableNode& scope) using (absl::string_view key) must succeed,
// and is captured as (const SymbolTableNode& dest).
// Most of the time, the tester is not interested in the found_* iterator.
// This also defines 'dest##_info' as the SymbolInfo value attached to the
// 'dest' SymbolTableNode. Defined as a macro so that failure gives meaningful
// line numbers, and this allows ASSERT_NE to early exit.
#define MUST_ASSIGN_LOOKUP_SYMBOL(dest, scope, key)                       \
  const auto found_##dest = scope.Find(key);                              \
  ASSERT_NE(found_##dest, scope.end())                                    \
      << "No symbol at \"" << key << "\" in " << ScopePathPrinter{scope}; \
  EXPECT_EQ(found_##dest->first, key);                                    \
  const SymbolTableNode& dest(found_##dest->second);                      \
  const SymbolInfo& dest##_info ABSL_ATTRIBUTE_UNUSED(dest.Value())

// For SymbolInfo::references_map_view_type only: Assert that there is exactly
// one element at 'key' in 'map' and assign it to 'dest' (DependentReferences).
// 'map' should come from SymbolInfo::LocalReferencesMapViewForTesting().
#define ASSIGN_MUST_FIND_EXACTLY_ONE_REF(dest, map, key)                 \
  ASSIGN_MUST_FIND(dest##_candidates, map, key); /* set of candidates */ \
  ASSIGN_MUST_HAVE_UNIQUE(dest, dest##_candidates);

TEST(SymbolTypePrintTest, Print) {
  std::ostringstream stream;
  stream << SymbolType::kClass;
  EXPECT_EQ(stream.str(), "class");
}

TEST(SymbolTableNodeFullPathTest, Print) {
  typedef SymbolTableNode::key_value_type KV;
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

TEST(ReferenceNodeFullPathTest, Print) {
  typedef ReferenceComponentNode Node;
  typedef ReferenceComponent Data;
  const Node root(
      Data{.identifier = "xx", .ref_type = ReferenceType::kUnqualified},
      Node(Data{.identifier = "yy", .ref_type = ReferenceType::kDirectMember},
           Node(Data{.identifier = "zz",
                     .ref_type = ReferenceType::kMemberOfTypeOfParent})));
  {
    std::ostringstream stream;
    ReferenceNodeFullPath(stream, root);
    EXPECT_EQ(stream.str(), "@xx");
  }
  {
    std::ostringstream stream;
    ReferenceNodeFullPath(stream, root.Children().front());
    EXPECT_EQ(stream.str(), "@xx::yy");
  }
  {
    std::ostringstream stream;
    ReferenceNodeFullPath(stream, root.Children().front().Children().front());
    EXPECT_EQ(stream.str(), "@xx::yy.zz");
  }
}

TEST(DependentReferencesTest, PrintEmpty) {
  DependentReferences dep_refs;
  std::ostringstream stream;
  stream << dep_refs;
  EXPECT_EQ(stream.str(), "(empty-ref)");
}

TEST(DependentReferencesTest, PrintOnlyRootNodeUnresolved) {
  const DependentReferences dep_refs{
      .components = absl::make_unique<ReferenceComponentNode>(
          ReferenceComponent{.identifier = "foo",
                             .ref_type = ReferenceType::kUnqualified,
                             .resolved_symbol = nullptr})};
  std::ostringstream stream;
  stream << dep_refs;
  EXPECT_EQ(stream.str(), "{ (@foo -> <unresolved>) }");
}

TEST(DependentReferencesTest, PrintNonRootResolved) {
  // Synthesize a symbol table.
  typedef SymbolTableNode::key_value_type KV;
  SymbolTableNode root(
      SymbolInfo{.type = SymbolType::kRoot},
      KV{"p_pkg",
         SymbolTableNode(SymbolInfo{.type = SymbolType::kPackage},
                         KV{"c_class", SymbolTableNode(SymbolInfo{
                                           .type = SymbolType::kClass})})});

  // Bookmark symbol table nodes.
  MUST_ASSIGN_LOOKUP_SYMBOL(p_pkg, root, "p_pkg");
  MUST_ASSIGN_LOOKUP_SYMBOL(c_class, p_pkg, "c_class");

  // Construct references already resolved to above nodes.
  const DependentReferences dep_refs{
      .components = absl::make_unique<ReferenceComponentNode>(
          ReferenceComponent{.identifier = "p_pkg",
                             .ref_type = ReferenceType::kUnqualified,
                             .resolved_symbol = &p_pkg},
          ReferenceComponentNode(
              ReferenceComponent{.identifier = "c_class",
                                 .ref_type = ReferenceType::kDirectMember,
                                 .resolved_symbol = &c_class}))};

  // Print and compare.
  std::ostringstream stream;
  stream << dep_refs;
  EXPECT_EQ(stream.str(),
            R"({ (@p_pkg -> $root::p_pkg)
  { (::c_class -> $root::p_pkg::c_class) }
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
  ASSERT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

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
      { (@qq -> $root::tt::qq) }
      )
    qq: { (refs: ) }
  }
})");
  }

  {  // Resolve symbols.
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_TRUE(resolve_diagnostics.empty());
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
      { (@qq -> $root::tt::qq) }
      )
    qq: { (refs: ) }
  }
})");
  }
}

TEST(BuildSymbolTableTest, IntegrityCheckResolvedSymbol) {
  const auto test_func = []() {
    SymbolTable::Tester symbol_table_1(nullptr), symbol_table_2(nullptr);
    SymbolTableNode& root1(symbol_table_1.MutableRoot());
    SymbolTableNode& root2(symbol_table_2.MutableRoot());
    // Deliberately point from one symbol table to the other.
    // To avoid an use-after-free AddressSanitizer error,
    // mind the destruction ordering here:
    // symbol_table1 will outlive symbol_table_2, so give symbol_table_2 a
    // pointer to symbol_table_1.
    root2.Value().local_references_to_bind.push_back(DependentReferences{
        .components = absl::make_unique<ReferenceComponentNode>(
            ReferenceComponent{.identifier = "foo",
                               .ref_type = ReferenceType::kUnqualified,
                               .resolved_symbol = &root1})});
    // CheckIntegrity() will fail on destruction of symbol_table_2.
  };
  EXPECT_DEATH(test_func(),
               "Resolved symbols must point to a node in the same SymbolTable");
}

TEST(BuildSymbolTableTest, IntegrityCheckDeclaredType) {
  const auto test_func = []() {
    SymbolTable::Tester symbol_table_1(nullptr), symbol_table_2(nullptr);
    SymbolTableNode& root1(symbol_table_1.MutableRoot());
    SymbolTableNode& root2(symbol_table_2.MutableRoot());
    // Deliberately point from one symbol table to the other.
    // To avoid an use-after-free AddressSanitizer error,
    // mind the destruction ordering here:
    // symbol_table1 will outlive symbol_table_2, so give symbol_table_2 a
    // pointer to symbol_table_1.
    root1.Value().local_references_to_bind.push_back(DependentReferences{
        .components = absl::make_unique<ReferenceComponentNode>(
            ReferenceComponent{.identifier = "foo",
                               .ref_type = ReferenceType::kUnqualified,
                               .resolved_symbol = &root1})});
    root2.Value().declared_type.user_defined_type =
        root1.Value().local_references_to_bind.front().components.get();
    // CheckIntegrity() will fail on destruction of symbol_table_2.
  };
  EXPECT_DEATH(test_func(),
               "Resolved symbols must point to a node in the same SymbolTable");
}

TEST(BuildSymbolTableTest, InvalidSyntax) {
  constexpr absl::string_view invalid_codes[] = {
      "module;\nendmodule\n",
  };
  for (const auto& code : invalid_codes) {
    TestVerilogSourceFile src("foobar.sv", code);
    const auto status = src.Parse();
    EXPECT_FALSE(status.ok());
    SymbolTable symbol_table(nullptr);
    EXPECT_EQ(symbol_table.Project(), nullptr);

    {  // Attempt to build symbol table after parse failure.
      const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

      EXPECT_TRUE(symbol_table.Root().Children().empty());
      EXPECT_TRUE(build_diagnostics.empty())
          << "Unexpected diagnostic:\n"
          << build_diagnostics.front().message();
    }
    {  // Attempt to resolve empty symbol table and references.
      std::vector<absl::Status> resolve_diagnostics;
      symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
      EXPECT_TRUE(resolve_diagnostics.empty());
    }
  }
}

TEST(BuildSymbolTableTest, AvoidCrashFromFuzzer) {
  // All that matters is that these test cases do not trigger crashes.
  constexpr absl::string_view codes[] = {
      // some of these test cases come from fuzz testing
      // and may contain syntax errors
      "`e(C*C);\n",              // expect two distinct reference trees
      "`e(C::D * C.m + 12);\n",  // expect two reference trees
      "n#7;\n",
      "c#1;;=P;\n",
  };
  for (const auto& code : codes) {
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
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(module_node, root_symbol, "m");
  EXPECT_EQ(module_node_info.type, SymbolType::kModule);
  EXPECT_EQ(module_node_info.file_origin, &src);
  EXPECT_EQ(module_node_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_TRUE(resolve_diagnostics.empty());
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
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(module_node, root_symbol, "m");
  EXPECT_EQ(module_node_info.type, SymbolType::kModule);
  EXPECT_EQ(module_node_info.file_origin, &src);
  EXPECT_EQ(module_node_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  static constexpr absl::string_view members[] = {"w1", "w2", "l1", "l2"};
  for (const auto& member : members) {
    MUST_ASSIGN_LOOKUP_SYMBOL(member_node, module_node, member);
    EXPECT_EQ(member_node_info.type, SymbolType::kDataNetVariableInstance);
    EXPECT_EQ(member_node_info.declared_type.user_defined_type,
              nullptr);  // types are primitive
  }

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_TRUE(resolve_diagnostics.empty());
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
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(module_node, root_symbol, "m");
  EXPECT_EQ(module_node_info.type, SymbolType::kModule);
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
    EXPECT_TRUE(resolve_diagnostics.empty());
  }
}

TEST(BuildSymbolTableTest, ModuleDeclarationConditionalGenerateAnonymous) {
  constexpr absl::string_view source_variants[] = {
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
  for (const auto& code : source_variants) {
    TestVerilogSourceFile src("foobar.sv", code);
    const auto status = src.Parse();
    ASSERT_TRUE(status.ok()) << status.message();
    SymbolTable symbol_table(nullptr);
    const SymbolTableNode& root_symbol(symbol_table.Root());

    const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

    MUST_ASSIGN_LOOKUP_SYMBOL(module_node, root_symbol, "m");
    EXPECT_EQ(module_node_info.type, SymbolType::kModule);
    EXPECT_EQ(module_node_info.file_origin, &src);
    EXPECT_EQ(module_node_info.declared_type.syntax_origin,
              nullptr);  // there is no module meta-type
    EXPECT_TRUE(build_diagnostics.empty())
        << "Unexpected diagnostic:\n"
        << build_diagnostics.front().message();

    ASSERT_EQ(module_node.Children().size(), 3);
    auto iter = module_node.Children().begin();
    {
      const SymbolTableNode& gen_block(iter->second);  // anonymous "...-0"
      const SymbolInfo& gen_block_info(gen_block.Value());
      EXPECT_EQ(gen_block_info.type, SymbolType::kGenerate);
      MUST_ASSIGN_LOOKUP_SYMBOL(wire_x, gen_block, "x");
      EXPECT_EQ(wire_x_info.type, SymbolType::kDataNetVariableInstance);
      ++iter;
    }
    {
      const SymbolTableNode& gen_block(iter->second);  // anonymous "...-1"
      const SymbolInfo& gen_block_info(gen_block.Value());
      EXPECT_EQ(gen_block_info.type, SymbolType::kGenerate);
      MUST_ASSIGN_LOOKUP_SYMBOL(wire_y, gen_block, "y");
      EXPECT_EQ(wire_y_info.type, SymbolType::kDataNetVariableInstance);
      ++iter;
    }
    {
      const SymbolTableNode& gen_block(iter->second);  // anonymous "...-2"
      const SymbolInfo& gen_block_info(gen_block.Value());
      EXPECT_EQ(gen_block_info.type, SymbolType::kGenerate);
      MUST_ASSIGN_LOOKUP_SYMBOL(wire_z, gen_block, "z");
      EXPECT_EQ(wire_z_info.type, SymbolType::kDataNetVariableInstance);
      ++iter;
    }

    {
      std::vector<absl::Status> resolve_diagnostics;
      symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
      EXPECT_TRUE(resolve_diagnostics.empty());
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
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(module_node, root_symbol, "m");
  EXPECT_EQ(module_node_info.type, SymbolType::kModule);
  EXPECT_EQ(module_node_info.file_origin, &src);
  EXPECT_EQ(module_node_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  ASSERT_EQ(module_node.Children().size(), 3);
  {
    MUST_ASSIGN_LOOKUP_SYMBOL(gen_block, module_node, "aa");
    EXPECT_EQ(gen_block_info.type, SymbolType::kGenerate);
    MUST_ASSIGN_LOOKUP_SYMBOL(wire_z, gen_block, "z");
    EXPECT_EQ(wire_z_info.type, SymbolType::kDataNetVariableInstance);
  }
  {
    MUST_ASSIGN_LOOKUP_SYMBOL(gen_block, module_node, "bb");
    EXPECT_EQ(gen_block_info.type, SymbolType::kGenerate);
    MUST_ASSIGN_LOOKUP_SYMBOL(wire_y, gen_block, "y");
    EXPECT_EQ(wire_y_info.type, SymbolType::kDataNetVariableInstance);
  }
  {
    MUST_ASSIGN_LOOKUP_SYMBOL(gen_block, module_node, "cc");
    EXPECT_EQ(gen_block_info.type, SymbolType::kGenerate);
    MUST_ASSIGN_LOOKUP_SYMBOL(wire_x, gen_block, "x");
    EXPECT_EQ(wire_x_info.type, SymbolType::kDataNetVariableInstance);
  }

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_TRUE(resolve_diagnostics.empty());
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
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(module_node, root_symbol, "m");
  EXPECT_EQ(module_node_info.type, SymbolType::kModule);
  EXPECT_EQ(module_node_info.file_origin, &src);
  EXPECT_EQ(module_node_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  static constexpr absl::string_view members[] = {"clk", "q"};
  for (const auto& member : members) {
    MUST_ASSIGN_LOOKUP_SYMBOL(member_node, module_node, member);
    EXPECT_EQ(member_node_info.type, SymbolType::kDataNetVariableInstance);
    EXPECT_EQ(member_node_info.declared_type.user_defined_type,
              nullptr);  // types are primitive
  }

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_TRUE(resolve_diagnostics.empty());
  }
}

TEST(BuildSymbolTableTest, ModuleDeclarationMultiple) {
  TestVerilogSourceFile src("foobar.sv",
                            "module m1;\nendmodule\n"
                            "module m2;\nendmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  const absl::string_view expected_modules[] = {"m1", "m2"};
  for (const auto& expected_module : expected_modules) {
    MUST_ASSIGN_LOOKUP_SYMBOL(module_node, root_symbol, expected_module);
    EXPECT_EQ(module_node_info.type, SymbolType::kModule);
    EXPECT_EQ(module_node_info.file_origin, &src);
    EXPECT_EQ(module_node_info.declared_type.syntax_origin,
              nullptr);  // there is no module meta-type
    EXPECT_TRUE(build_diagnostics.empty())
        << "Unexpected diagnostic:\n"
        << build_diagnostics.front().message();
  }

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_TRUE(resolve_diagnostics.empty());
  }
}

TEST(BuildSymbolTableTest, ModuleDeclarationDuplicate) {
  TestVerilogSourceFile src("foobar.sv",
                            "module mm;\nendmodule\n"
                            "module mm;\nendmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(module_node, root_symbol, "mm");
  EXPECT_EQ(module_node_info.type, SymbolType::kModule);
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
    EXPECT_TRUE(resolve_diagnostics.empty());
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
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();
  MUST_ASSIGN_LOOKUP_SYMBOL(outer_module_node, root_symbol, "m_outer");
  {
    EXPECT_EQ(outer_module_node_info.type, SymbolType::kModule);
    EXPECT_EQ(outer_module_node_info.file_origin, &src);
    EXPECT_EQ(outer_module_node_info.declared_type.syntax_origin,
              nullptr);  // there is no module meta-type
  }
  {
    MUST_ASSIGN_LOOKUP_SYMBOL(inner_module_node, outer_module_node, "m_inner");
    EXPECT_EQ(inner_module_node_info.type, SymbolType::kModule);
    EXPECT_EQ(inner_module_node_info.file_origin, &src);
    EXPECT_EQ(inner_module_node_info.declared_type.syntax_origin,
              nullptr);  // there is no module meta-type
  }
  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_TRUE(resolve_diagnostics.empty());
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
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(module_node, root_symbol, "outer");
  EXPECT_EQ(module_node_info.type, SymbolType::kModule);

  ASSIGN_MUST_HAVE_UNIQUE(err, build_diagnostics);
  EXPECT_EQ(err.code(), absl::StatusCode::kAlreadyExists);
  EXPECT_THAT(err.message(),
              HasSubstr("\"mm\" is already defined in the $root::outer scope"));
  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_TRUE(resolve_diagnostics.empty());
  }
}

TEST(BuildSymbolTableTest, ModuleInstance) {
  // The following code variants should yield the same symbol table results:
  static constexpr absl::string_view source_variants[] = {
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
  for (const auto& code : source_variants) {
    VLOG(1) << "code:\n" << code;
    TestVerilogSourceFile src("foobar.sv", code);
    const auto status = src.Parse();
    ASSERT_TRUE(status.ok()) << status.message();
    SymbolTable symbol_table(nullptr);
    const SymbolTableNode& root_symbol(symbol_table.Root());

    const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

    EXPECT_TRUE(build_diagnostics.empty())
        << "Unexpected diagnostic:\n"
        << build_diagnostics.front().message();

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
        const ReferenceComponentNode* ref_node = pp_type->LastLeaf();
        ASSERT_NE(ref_node, nullptr);
        const ReferenceComponent& ref(ref_node->Value());
        EXPECT_EQ(ref.identifier, "pp");
        EXPECT_TRUE(verible::IsSubRange(ref.identifier,
                                        src.GetTextStructure()->Contents()));
        EXPECT_EQ(ref.ref_type, ReferenceType::kUnqualified);
        EXPECT_EQ(ref.resolved_symbol, nullptr);
      }
      {  // self-reference to "rr" instance
        ASSIGN_MUST_FIND_EXACTLY_ONE_REF(rr_self_ref, ref_map, "rr");
        EXPECT_TRUE(rr_self_ref->components->is_leaf());  // no named ports
        // self-reference is already bound.
        EXPECT_EQ(rr_self_ref->components->Value().resolved_symbol, &rr);
      }
    }

    EXPECT_TRUE(rr_info.local_references_to_bind.empty());
    EXPECT_NE(rr_info.declared_type.user_defined_type, nullptr);
    {
      const ReferenceComponent& pp_type(
          rr_info.declared_type.user_defined_type->Value());
      EXPECT_EQ(pp_type.identifier, "pp");
      EXPECT_EQ(pp_type.resolved_symbol, nullptr);  // nothing resolved yet
      EXPECT_EQ(pp_type.ref_type, ReferenceType::kUnqualified);
    }
    EXPECT_EQ(rr_info.file_origin, &src);

    // Resolve symbols.
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);

    EXPECT_TRUE(resolve_diagnostics.empty());
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
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  // Inspect inside the "qq" module definition.
  MUST_ASSIGN_LOOKUP_SYMBOL(qq, root_symbol, "qq");
  {
    EXPECT_EQ(qq_info.file_origin, &src);
    // There is only one reference to the "pp" module type.
    ASSERT_EQ(qq_info.local_references_to_bind.size(), 2);
    const auto ref_map(qq_info.LocalReferencesMapViewForTesting());
    ASSIGN_MUST_FIND_EXACTLY_ONE_REF(pp_type, ref_map, "pp");
    {  // verify that a reference to "pp" was established
      const ReferenceComponentNode* ref_node = pp_type->LastLeaf();
      ASSERT_NE(ref_node, nullptr);
      const ReferenceComponent& ref(ref_node->Value());
      EXPECT_EQ(ref.identifier, "pp");
      EXPECT_TRUE(verible::IsSubRange(ref.identifier,
                                      src.GetTextStructure()->Contents()));
      EXPECT_EQ(ref.ref_type, ReferenceType::kUnqualified);
      EXPECT_EQ(ref.resolved_symbol, nullptr);
    }
  }

  // "rr" is an instance of type "pp" (which is undefined)
  MUST_ASSIGN_LOOKUP_SYMBOL(rr, qq, "rr");
  EXPECT_TRUE(rr_info.local_references_to_bind.empty());
  EXPECT_NE(rr_info.declared_type.user_defined_type, nullptr);
  {
    const ReferenceComponent& pp_type(
        rr_info.declared_type.user_defined_type->Value());
    EXPECT_EQ(pp_type.identifier, "pp");
    EXPECT_EQ(pp_type.resolved_symbol, nullptr);  // nothing resolved yet
    EXPECT_EQ(pp_type.ref_type, ReferenceType::kUnqualified);
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
  static constexpr absl::string_view source_variants[] = {
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
  for (const auto& code : source_variants) {
    TestVerilogSourceFile src("foobar.sv", code);
    const auto status = src.Parse();
    ASSERT_TRUE(status.ok()) << status.message();
    SymbolTable symbol_table(nullptr);
    const SymbolTableNode& root_symbol(symbol_table.Root());

    const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

    EXPECT_TRUE(build_diagnostics.empty())
        << "Unexpected diagnostic:\n"
        << build_diagnostics.front().message();

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
      const ReferenceComponentNode* ref_node = pp_type->LastLeaf();
      ASSERT_NE(ref_node, nullptr);
      const ReferenceComponent& ref(ref_node->Value());
      EXPECT_EQ(ref.identifier, "pp");
      EXPECT_TRUE(verible::IsSubRange(ref.identifier,
                                      src.GetTextStructure()->Contents()));
      EXPECT_EQ(ref.ref_type, ReferenceType::kUnqualified);
      EXPECT_EQ(ref.resolved_symbol, nullptr);
    }

    // "r1" and "r2" are both instances of type "pp"
    static constexpr absl::string_view pp_instances[] = {"r1", "r2"};
    for (const auto& pp_inst : pp_instances) {
      MUST_ASSIGN_LOOKUP_SYMBOL(rr, qq, pp_inst);
      EXPECT_TRUE(rr_info.local_references_to_bind.empty());
      EXPECT_NE(rr_info.declared_type.user_defined_type, nullptr);
      {
        const ReferenceComponent& pp_type(
            rr_info.declared_type.user_defined_type->Value());
        EXPECT_EQ(pp_type.identifier, "pp");
        EXPECT_EQ(pp_type.resolved_symbol, nullptr);  // nothing resolved yet
        EXPECT_EQ(pp_type.ref_type, ReferenceType::kUnqualified);
      }
      EXPECT_EQ(rr_info.file_origin, &src);
    }

    {
      std::vector<absl::Status> resolve_diagnostics;
      symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
      EXPECT_TRUE(resolve_diagnostics.empty());

      for (const auto& pp_inst : pp_instances) {
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
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(m_node, root_symbol, "m");
  EXPECT_EQ(m_node_info.type, SymbolType::kModule);
  EXPECT_EQ(m_node_info.file_origin, &src);
  EXPECT_EQ(m_node_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  MUST_ASSIGN_LOOKUP_SYMBOL(clk_node, m_node, "clk");
  EXPECT_EQ(clk_node_info.type, SymbolType::kDataNetVariableInstance);
  EXPECT_EQ(clk_node_info.declared_type.user_defined_type,
            nullptr);  // types are primitive

  MUST_ASSIGN_LOOKUP_SYMBOL(q_node, m_node, "q");
  EXPECT_EQ(q_node_info.type, SymbolType::kDataNetVariableInstance);
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
    EXPECT_TRUE(resolve_diagnostics.empty());

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
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(m_node, root_symbol, "m");
  EXPECT_EQ(m_node_info.type, SymbolType::kModule);
  EXPECT_EQ(m_node_info.file_origin, &src);
  EXPECT_EQ(m_node_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  MUST_ASSIGN_LOOKUP_SYMBOL(clk_node, m_node, "clk");
  EXPECT_EQ(clk_node_info.type, SymbolType::kDataNetVariableInstance);
  EXPECT_EQ(clk_node_info.declared_type.user_defined_type,
            nullptr);  // types are primitive

  MUST_ASSIGN_LOOKUP_SYMBOL(q_node, m_node, "q");
  EXPECT_EQ(q_node_info.type, SymbolType::kDataNetVariableInstance);
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

  const ReferenceComponentNode& m_inst_ref_root(*m_inst_ref->components);
  ASSERT_EQ(m_inst_ref_root.Children().size(), 2);
  const ReferenceComponentMap port_refs(
      ReferenceComponentNodeMapView(m_inst_ref_root));

  const auto found_clk_ref = port_refs.find("clk");
  ASSERT_NE(found_clk_ref, port_refs.end());
  const ReferenceComponentNode& clk_ref(*found_clk_ref->second);
  EXPECT_EQ(clk_ref.Value().identifier, "clk");
  EXPECT_EQ(clk_ref.Value().ref_type, ReferenceType::kMemberOfTypeOfParent);
  EXPECT_EQ(clk_ref.Value().resolved_symbol, nullptr);  // not yet resolved

  const auto found_q_ref = port_refs.find("q");
  ASSERT_NE(found_q_ref, port_refs.end());
  const ReferenceComponentNode& q_ref(*found_q_ref->second);
  EXPECT_EQ(q_ref.Value().identifier, "q");
  EXPECT_EQ(q_ref.Value().ref_type, ReferenceType::kMemberOfTypeOfParent);
  EXPECT_EQ(q_ref.Value().resolved_symbol, nullptr);  // not yet resolved

  // Get the local symbol definitions for wires "c" and "d".
  MUST_ASSIGN_LOOKUP_SYMBOL(c_node, rr_node, "c");
  MUST_ASSIGN_LOOKUP_SYMBOL(d_node, rr_node, "d");

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_TRUE(resolve_diagnostics.empty());

    // Expect to resolve local references to wires c and d
    EXPECT_EQ(c_ref->LastLeaf()->Value().resolved_symbol, &c_node);
    EXPECT_EQ(d_ref->LastLeaf()->Value().resolved_symbol, &d_node);

    // Expect to resolved named port references to "clk" and "q".
    EXPECT_EQ(clk_ref.Value().resolved_symbol, &clk_node);
    EXPECT_EQ(q_ref.Value().resolved_symbol, &q_node);
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
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(m_node, root_symbol, "m");
  EXPECT_EQ(m_node_info.type, SymbolType::kModule);
  EXPECT_EQ(m_node_info.file_origin, &src);
  EXPECT_EQ(m_node_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  MUST_ASSIGN_LOOKUP_SYMBOL(clk_node, m_node, "clk");
  EXPECT_EQ(clk_node_info.type, SymbolType::kDataNetVariableInstance);
  EXPECT_EQ(clk_node_info.declared_type.user_defined_type,
            nullptr);  // types are primitive

  MUST_ASSIGN_LOOKUP_SYMBOL(q_node, m_node, "q");
  EXPECT_EQ(q_node_info.type, SymbolType::kDataNetVariableInstance);
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

  const ReferenceComponentNode& m_inst_ref_root(*m_inst_ref->components);
  ASSERT_EQ(m_inst_ref_root.Children().size(), 2);
  const ReferenceComponentMap port_refs(
      ReferenceComponentNodeMapView(m_inst_ref_root));

  const auto found_clk_ref = port_refs.find("clk");
  ASSERT_NE(found_clk_ref, port_refs.end());
  const ReferenceComponentNode& clk_ref(*found_clk_ref->second);
  EXPECT_EQ(clk_ref.Value().identifier, "clk");
  EXPECT_EQ(clk_ref.Value().ref_type, ReferenceType::kMemberOfTypeOfParent);
  // "clk" is a non-local reference that will not even be resolved below
  EXPECT_EQ(clk_ref.Value().resolved_symbol, nullptr);

  const auto found_q_ref = port_refs.find("q");
  ASSERT_NE(found_q_ref, port_refs.end());
  const ReferenceComponentNode& q_ref(*found_q_ref->second);
  EXPECT_EQ(q_ref.Value().identifier, "q");
  EXPECT_EQ(q_ref.Value().ref_type, ReferenceType::kMemberOfTypeOfParent);
  // "q" is a non-local reference that will not even be resolved below
  EXPECT_EQ(q_ref.Value().resolved_symbol, nullptr);

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
    EXPECT_EQ(clk_ref.Value().resolved_symbol, nullptr);
    EXPECT_EQ(q_ref.Value().resolved_symbol, nullptr);
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
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(m_node, root_symbol, "m");
  EXPECT_EQ(m_node_info.type, SymbolType::kModule);
  EXPECT_EQ(m_node_info.file_origin, &src);
  EXPECT_EQ(m_node_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  MUST_ASSIGN_LOOKUP_SYMBOL(n_param, m_node, "N");
  EXPECT_EQ(n_param_info.type, SymbolType::kParameter);
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
    EXPECT_TRUE(resolve_diagnostics.empty());

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
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(m_node, root_symbol, "m");
  EXPECT_EQ(m_node_info.type, SymbolType::kModule);
  EXPECT_EQ(m_node_info.file_origin, &src);
  EXPECT_EQ(m_node_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  MUST_ASSIGN_LOOKUP_SYMBOL(n_param, m_node, "N");
  EXPECT_EQ(n_param_info.type, SymbolType::kParameter);
  EXPECT_EQ(n_param_info.declared_type.user_defined_type,
            nullptr);  // types are primitive

  MUST_ASSIGN_LOOKUP_SYMBOL(p_param, m_node, "P");
  EXPECT_EQ(p_param_info.type, SymbolType::kParameter);
  EXPECT_EQ(p_param_info.declared_type.user_defined_type,
            nullptr);  // types are primitive

  MUST_ASSIGN_LOOKUP_SYMBOL(rr_node, root_symbol, "rr");
  MUST_ASSIGN_LOOKUP_SYMBOL(m_inst_node, rr_node, "m_inst");

  const auto ref_map(rr_node_info.LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(m_type_ref, ref_map, "m");

  const ReferenceComponentNode& m_type_ref_root(*m_type_ref->components);
  ASSERT_EQ(m_type_ref_root.Children().size(), 2);
  const ReferenceComponentMap param_refs(
      ReferenceComponentNodeMapView(m_type_ref_root));

  ASSIGN_MUST_FIND(n_ref, param_refs, "N");
  const ReferenceComponent& n_ref_comp(n_ref->Value());
  EXPECT_EQ(n_ref_comp.identifier, "N");
  EXPECT_EQ(n_ref_comp.ref_type, ReferenceType::kDirectMember);
  EXPECT_EQ(n_ref_comp.resolved_symbol, nullptr);  // not yet resolved

  ASSIGN_MUST_FIND(p_ref, param_refs, "P");
  const ReferenceComponent& p_ref_comp(p_ref->Value());
  EXPECT_EQ(p_ref_comp.identifier, "P");
  EXPECT_EQ(p_ref_comp.ref_type, ReferenceType::kDirectMember);
  EXPECT_EQ(p_ref_comp.resolved_symbol, nullptr);  // not yet resolved

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_TRUE(resolve_diagnostics.empty())
        << "Unexpected diagnostic: " << resolve_diagnostics.front().message();

    // Expect ".N" and ".P" to resolve to formal parameters of "m".
    EXPECT_EQ(n_ref_comp.resolved_symbol, &n_param);
    EXPECT_EQ(p_ref_comp.resolved_symbol, &p_param);
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
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(m_node, root_symbol, "m");
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  MUST_ASSIGN_LOOKUP_SYMBOL(clk_node, m_node, "clk");

  MUST_ASSIGN_LOOKUP_SYMBOL(q_node, m_node, "q");

  MUST_ASSIGN_LOOKUP_SYMBOL(rr_node, root_symbol, "rr");

  const auto ref_map(rr_node_info.LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(m_inst_ref, ref_map, "m_inst");
  ASSERT_NE(m_inst_ref, nullptr);

  const ReferenceComponentNode& m_inst_ref_root(*m_inst_ref->components);
  ASSERT_EQ(m_inst_ref_root.Children().size(), 2);
  const ReferenceComponentMap port_refs(
      ReferenceComponentNodeMapView(m_inst_ref_root));

  const auto found_clk_ref = port_refs.find("clk");
  ASSERT_NE(found_clk_ref, port_refs.end());
  const ReferenceComponentNode& clk_ref(*found_clk_ref->second);
  EXPECT_EQ(clk_ref.Value().identifier, "clk");
  EXPECT_EQ(clk_ref.Value().ref_type, ReferenceType::kMemberOfTypeOfParent);
  EXPECT_EQ(clk_ref.Value().resolved_symbol, nullptr);  // not yet resolved

  const auto found_p_ref = port_refs.find("p");
  ASSERT_NE(found_p_ref, port_refs.end());
  const ReferenceComponentNode& p_ref(*found_p_ref->second);
  EXPECT_EQ(p_ref.Value().identifier, "p");
  EXPECT_EQ(p_ref.Value().ref_type, ReferenceType::kMemberOfTypeOfParent);
  EXPECT_EQ(p_ref.Value().resolved_symbol, nullptr);  // not yet resolved

  // Get the local symbol definitions for wire "c".
  MUST_ASSIGN_LOOKUP_SYMBOL(c_node, rr_node, "c");

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);

    ASSIGN_MUST_HAVE_UNIQUE(err, resolve_diagnostics);
    EXPECT_EQ(err.code(), absl::StatusCode::kNotFound);
    EXPECT_THAT(err.message(),
                HasSubstr("No member symbol \"p\" in parent scope m."));

    // Expect to resolved named port reference to "clk", but not "p".
    EXPECT_EQ(clk_ref.Value().resolved_symbol, &clk_node);
    EXPECT_EQ(p_ref.Value().resolved_symbol, nullptr);  // failed to resolve
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
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(m_node, root_symbol, "m");
  EXPECT_EQ(m_node_info.type, SymbolType::kModule);
  EXPECT_EQ(m_node_info.file_origin, &src);
  EXPECT_EQ(m_node_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  MUST_ASSIGN_LOOKUP_SYMBOL(n_param, m_node, "N");
  EXPECT_EQ(n_param_info.type, SymbolType::kParameter);
  EXPECT_EQ(n_param_info.declared_type.user_defined_type,
            nullptr);  // types are primitive

  MUST_ASSIGN_LOOKUP_SYMBOL(p_param, m_node, "P");
  EXPECT_EQ(p_param_info.type, SymbolType::kParameter);
  EXPECT_EQ(p_param_info.declared_type.user_defined_type,
            nullptr);  // types are primitive

  MUST_ASSIGN_LOOKUP_SYMBOL(rr_node, root_symbol, "rr");
  MUST_ASSIGN_LOOKUP_SYMBOL(m_inst_node, rr_node, "m_inst");

  const auto ref_map(rr_node_info.LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(m_type_ref, ref_map, "m");

  const ReferenceComponentNode& m_type_ref_root(*m_type_ref->components);
  ASSERT_EQ(m_type_ref_root.Children().size(), 2);
  const ReferenceComponentMap param_refs(
      ReferenceComponentNodeMapView(m_type_ref_root));

  ASSIGN_MUST_FIND(n_ref, param_refs, "N");
  const ReferenceComponent& n_ref_comp(n_ref->Value());
  EXPECT_EQ(n_ref_comp.identifier, "N");
  EXPECT_EQ(n_ref_comp.ref_type, ReferenceType::kDirectMember);
  EXPECT_EQ(n_ref_comp.resolved_symbol, nullptr);  // not yet resolved

  ASSIGN_MUST_FIND(q_ref, param_refs, "Q");
  const ReferenceComponent& q_ref_comp(q_ref->Value());
  EXPECT_EQ(q_ref_comp.identifier, "Q");
  EXPECT_EQ(q_ref_comp.ref_type, ReferenceType::kDirectMember);
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
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(mint_param, root_symbol, "mint");
  EXPECT_EQ(mint_param_info.type, SymbolType::kParameter);
  EXPECT_EQ(mint_param_info.file_origin, &src);
  ASSERT_NE(mint_param_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(
      verible::StringSpanOfSymbol(*mint_param_info.declared_type.syntax_origin),
      "int");
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_TRUE(resolve_diagnostics.empty());
  }
}

TEST(BuildSymbolTableTest, OneGlobalUndefinedTypeParameter) {
  TestVerilogSourceFile src("foobar.sv", "localparam foo_t gun = 1;\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(gun_param, root_symbol, "gun");
  EXPECT_EQ(gun_param_info.type, SymbolType::kParameter);
  EXPECT_EQ(gun_param_info.file_origin, &src);
  ASSERT_NE(gun_param_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(
      verible::StringSpanOfSymbol(*gun_param_info.declared_type.syntax_origin),
      "foo_t");
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

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
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(tea, root_symbol, "tea");
  EXPECT_EQ(tea_info.type, SymbolType::kParameter);

  MUST_ASSIGN_LOOKUP_SYMBOL(mint, root_symbol, "mint");
  EXPECT_EQ(mint_info.type, SymbolType::kParameter);
  EXPECT_EQ(mint_info.file_origin, &src);
  ASSERT_NE(mint_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(verible::StringSpanOfSymbol(*mint_info.declared_type.syntax_origin),
            "int");
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  // There should be one reference: "mint" (line 2)
  EXPECT_EQ(root_symbol.Value().local_references_to_bind.size(), 1);
  const auto ref_map(root_symbol.Value().LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(ref, ref_map, "mint");
  const ReferenceComponent& ref_comp(ref->components->Value());
  EXPECT_TRUE(ref->components->is_leaf());
  EXPECT_EQ(ref_comp.identifier, "mint");
  EXPECT_EQ(ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(ref_comp.resolved_symbol,
            nullptr);  // have not tried to resolve yet

  {  // resolve symbols
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_TRUE(resolve_diagnostics.empty());
    EXPECT_EQ(ref_comp.resolved_symbol, &mint);  // resolved
  }
}

TEST(BuildSymbolTableTest, OneUnresolvedReferenceInExpression) {
  TestVerilogSourceFile src("foobar.sv", "localparam int mint = spice;\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(mint, root_symbol, "mint");
  EXPECT_EQ(mint_info.type, SymbolType::kParameter);
  EXPECT_EQ(mint_info.file_origin, &src);
  ASSERT_NE(mint_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(verible::StringSpanOfSymbol(*mint_info.declared_type.syntax_origin),
            "int");
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  // There should be one reference: "spice" (line 2)
  EXPECT_EQ(root_symbol.Value().local_references_to_bind.size(), 1);
  const auto ref_map(root_symbol.Value().LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(ref, ref_map, "spice");
  const ReferenceComponent& ref_comp(ref->components->Value());
  EXPECT_TRUE(ref->components->is_leaf());
  EXPECT_EQ(ref_comp.identifier, "spice");
  EXPECT_EQ(ref_comp.ref_type, ReferenceType::kUnqualified);
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
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(my_pkg, root_symbol, "my_pkg");
  EXPECT_EQ(my_pkg_info.type, SymbolType::kPackage);
  EXPECT_EQ(my_pkg_info.file_origin, &src);
  EXPECT_EQ(my_pkg_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_TRUE(resolve_diagnostics.empty());
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
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(p_pkg, root_symbol, "p");
  EXPECT_EQ(p_pkg_info.type, SymbolType::kPackage);

  ASSERT_EQ(p_pkg_info.local_references_to_bind.size(), 1);
  const auto ref_map(p_pkg_info.LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(ref, ref_map, "mint");
  const ReferenceComponent& mint_ref(ref->components->Value());
  EXPECT_EQ(mint_ref.identifier, "mint");
  EXPECT_EQ(mint_ref.resolved_symbol, nullptr);  // not yet resolved

  MUST_ASSIGN_LOOKUP_SYMBOL(tea, p_pkg, "tea");  // p::tea
  EXPECT_EQ(tea_info.type, SymbolType::kParameter);

  MUST_ASSIGN_LOOKUP_SYMBOL(mint, root_symbol, "mint");
  EXPECT_EQ(mint_info.type, SymbolType::kParameter);
  EXPECT_EQ(mint_info.file_origin, &src);
  ASSERT_NE(mint_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(verible::StringSpanOfSymbol(*mint_info.declared_type.syntax_origin),
            "int");
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  {  // resolve symbols
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_TRUE(resolve_diagnostics.empty());

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
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(p_pkg, root_symbol, "p");
  EXPECT_EQ(p_pkg_info.type, SymbolType::kPackage);

  ASSERT_EQ(root_symbol.Value().local_references_to_bind.size(), 1);
  // p_mint_ref is the reference chain for "p::mint".
  const auto ref_map(root_symbol.Value().LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(p_mint_ref, ref_map, "p");
  const ReferenceComponent& p_ref(p_mint_ref->components->Value());
  EXPECT_EQ(p_ref.identifier, "p");
  EXPECT_EQ(p_ref.resolved_symbol, nullptr);  // not yet resolved
  const ReferenceComponent& mint_ref(p_mint_ref->LastLeaf()->Value());
  EXPECT_EQ(mint_ref.identifier, "mint");
  EXPECT_EQ(mint_ref.resolved_symbol, nullptr);  // not yet resolved

  MUST_ASSIGN_LOOKUP_SYMBOL(tea, root_symbol, "tea");
  EXPECT_EQ(tea_info.type, SymbolType::kParameter);

  MUST_ASSIGN_LOOKUP_SYMBOL(mint, p_pkg, "mint");  // p::mint
  EXPECT_EQ(mint_info.type, SymbolType::kParameter);
  EXPECT_EQ(mint_info.file_origin, &src);
  ASSERT_NE(mint_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(verible::StringSpanOfSymbol(*mint_info.declared_type.syntax_origin),
            "int");
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  {  // resolve symbols
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_TRUE(resolve_diagnostics.empty());

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
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(p_pkg, root_symbol, "p");
  EXPECT_EQ(p_pkg_info.type, SymbolType::kPackage);

  ASSERT_EQ(root_symbol.Value().local_references_to_bind.size(), 1);
  // p_mint_ref is the reference chain for "p::mint".
  const auto ref_map(root_symbol.Value().LocalReferencesMapViewForTesting());
  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(p_mint_ref, ref_map, "p");
  const ReferenceComponent& p_ref(p_mint_ref->components->Value());
  EXPECT_EQ(p_ref.identifier, "p");
  EXPECT_EQ(p_ref.resolved_symbol, nullptr);  // not yet resolved
  const ReferenceComponent& zzz_ref(p_mint_ref->LastLeaf()->Value());
  EXPECT_EQ(zzz_ref.identifier, "zzz");
  EXPECT_EQ(zzz_ref.resolved_symbol, nullptr);  // not yet resolved

  MUST_ASSIGN_LOOKUP_SYMBOL(tea, root_symbol, "tea");
  EXPECT_EQ(tea_info.type, SymbolType::kParameter);

  MUST_ASSIGN_LOOKUP_SYMBOL(mint, p_pkg, "mint");
  EXPECT_EQ(mint_info.type, SymbolType::kParameter);
  EXPECT_EQ(mint_info.file_origin, &src);
  ASSERT_NE(mint_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(verible::StringSpanOfSymbol(*mint_info.declared_type.syntax_origin),
            "int");
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

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
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(module_node, root_symbol, "m");
  EXPECT_EQ(module_node_info.type, SymbolType::kModule);
  EXPECT_EQ(module_node_info.file_origin, &src);
  EXPECT_EQ(module_node_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  MUST_ASSIGN_LOOKUP_SYMBOL(w_param, module_node, "W");
  EXPECT_EQ(w_param_info.type, SymbolType::kParameter);
  const ReferenceComponentNode* w_type_ref =
      w_param_info.declared_type.user_defined_type;
  EXPECT_EQ(w_type_ref, nullptr);  // int is primitive type

  MUST_ASSIGN_LOOKUP_SYMBOL(b_param, module_node, "B");
  EXPECT_EQ(b_param_info.type, SymbolType::kParameter);
  const ReferenceComponentNode* b_type_ref =
      b_param_info.declared_type.user_defined_type;
  ASSERT_NE(b_type_ref, nullptr);
  EXPECT_EQ(b_type_ref->Value().ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(b_type_ref->Value().identifier, "bar");

  ASSERT_EQ(module_node_info.local_references_to_bind.size(), 2);
  const auto ref_map(module_node_info.LocalReferencesMapViewForTesting());

  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(w_ref, ref_map, "W");
  const ReferenceComponent& w_ref_comp(w_ref->components->Value());
  EXPECT_EQ(w_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(w_ref_comp.identifier, "W");
  EXPECT_EQ(w_ref_comp.resolved_symbol, nullptr);  // not yet resolved

  ASSIGN_MUST_FIND_EXACTLY_ONE_REF(bar_ref, ref_map, "bar");
  const ReferenceComponent& bar_ref_comp(bar_ref->components->Value());
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
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(module_m, root_symbol, "m");
  EXPECT_EQ(module_m_info.type, SymbolType::kModule);
  EXPECT_EQ(module_m_info.file_origin, &src);
  EXPECT_EQ(module_m_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  MUST_ASSIGN_LOOKUP_SYMBOL(n_param, module_m, "N");
  EXPECT_EQ(n_param_info.type, SymbolType::kParameter);
  const ReferenceComponentNode* n_type_ref =
      n_param_info.declared_type.user_defined_type;
  EXPECT_EQ(n_type_ref, nullptr);  // int is primitive type

  EXPECT_EQ(module_m_info.local_references_to_bind.size(), 6);
  const auto ref_map(module_m_info.LocalReferencesMapViewForTesting());

  ASSIGN_MUST_FIND(n_refs, ref_map, "N");
  ASSERT_EQ(n_refs.size(), 6);  // all references to "N" parameter
  for (const auto& n_ref : n_refs) {
    const ReferenceComponent& n_ref_comp(n_ref->components->Value());
    EXPECT_EQ(n_ref_comp.ref_type, ReferenceType::kUnqualified);
    EXPECT_EQ(n_ref_comp.identifier, "N");
    EXPECT_EQ(n_ref_comp.resolved_symbol, nullptr);  // not yet resolved
  }

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_TRUE(resolve_diagnostics.empty())
        << "Unexpected diagnostic: " << resolve_diagnostics.front().message();

    // All references to "N" resolved.
    for (const auto& n_ref : n_refs) {
      const ReferenceComponent& n_ref_comp(n_ref->components->Value());
      EXPECT_EQ(n_ref_comp.resolved_symbol, &n_param);  // resolved successfully
    }
  }
}

TEST(BuildSymbolTableTest, ClassDeclarationSingle) {
  TestVerilogSourceFile src("foobar.sv", "class ccc;\nendclass\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(ccc, root_symbol, "ccc");
  EXPECT_EQ(ccc_info.type, SymbolType::kClass);
  EXPECT_EQ(ccc_info.file_origin, &src);
  EXPECT_EQ(ccc_info.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_TRUE(resolve_diagnostics.empty());
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
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  MUST_ASSIGN_LOOKUP_SYMBOL(pp, root_symbol, "pp");
  EXPECT_EQ(pp_info.type, SymbolType::kPackage);
  EXPECT_EQ(pp_info.file_origin, &src);
  EXPECT_EQ(pp_info.declared_type.syntax_origin,
            nullptr);  // there is no package meta-type
  {
    MUST_ASSIGN_LOOKUP_SYMBOL(c_outer, pp, "c_outer");
    EXPECT_EQ(c_outer_info.type, SymbolType::kClass);
    EXPECT_EQ(c_outer_info.file_origin, &src);
    EXPECT_EQ(c_outer_info.declared_type.syntax_origin,
              nullptr);  // there is no class meta-type
    {
      MUST_ASSIGN_LOOKUP_SYMBOL(c_inner, c_outer, "c_inner");
      EXPECT_EQ(c_inner_info.type, SymbolType::kClass);
      EXPECT_EQ(c_inner_info.file_origin, &src);
      EXPECT_EQ(c_inner_info.declared_type.syntax_origin,
                nullptr);  // there is no class meta-type
    }
  }
  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_TRUE(resolve_diagnostics.empty());
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
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  MUST_ASSIGN_LOOKUP_SYMBOL(class_cc, root_symbol, "cc");
  EXPECT_EQ(class_cc_info.type, SymbolType::kClass);
  EXPECT_EQ(class_cc_info.file_origin, &src);
  EXPECT_EQ(class_cc_info.declared_type.syntax_origin,
            nullptr);  // there is no class meta-type
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  MUST_ASSIGN_LOOKUP_SYMBOL(n_param, class_cc, "N");
  EXPECT_EQ(n_param_info.type, SymbolType::kParameter);
  const ReferenceComponentNode* n_type_ref =
      n_param_info.declared_type.user_defined_type;
  EXPECT_EQ(n_type_ref, nullptr);  // int is primitive type

  EXPECT_TRUE(class_cc_info.local_references_to_bind.empty());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_TRUE(resolve_diagnostics.empty());
  }
}

TEST(BuildSymbolTableTest, FunctionDeclarationNoReturnType) {
  TestVerilogSourceFile src("funkytown.sv",
                            "function ff;\n"
                            "endfunction\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  MUST_ASSIGN_LOOKUP_SYMBOL(function_ff, root_symbol, "ff");
  EXPECT_EQ(function_ff_info.type, SymbolType::kFunction);
  EXPECT_EQ(function_ff_info.file_origin, &src);
  // no return type
  EXPECT_EQ(function_ff_info.declared_type.syntax_origin, nullptr);

  EXPECT_TRUE(function_ff_info.local_references_to_bind.empty());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_TRUE(resolve_diagnostics.empty());
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
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  MUST_ASSIGN_LOOKUP_SYMBOL(function_ff, root_symbol, "ff");
  EXPECT_EQ(function_ff_info.type, SymbolType::kFunction);
  EXPECT_EQ(function_ff_info.file_origin, &src);
  EXPECT_EQ(function_ff_info.declared_type.syntax_origin,
            nullptr);  // there is no function return type

  MUST_ASSIGN_LOOKUP_SYMBOL(param_g, function_ff, "g");
  EXPECT_EQ(param_g_info.type, SymbolType::kDataNetVariableInstance);
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
    EXPECT_TRUE(resolve_diagnostics.empty());
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
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  MUST_ASSIGN_LOOKUP_SYMBOL(function_ff, root_symbol, "ff");
  EXPECT_EQ(function_ff_info.type, SymbolType::kFunction);
  EXPECT_EQ(function_ff_info.file_origin, &src);
  EXPECT_EQ(function_ff_info.declared_type.syntax_origin,
            nullptr);  // there is no function return type

  MUST_ASSIGN_LOOKUP_SYMBOL(local_g, function_ff, "g");
  EXPECT_EQ(local_g_info.type, SymbolType::kDataNetVariableInstance);
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
    EXPECT_TRUE(resolve_diagnostics.empty());
  }
}

TEST(BuildSymbolTableTest, FunctionDeclarationVoidReturnType) {
  TestVerilogSourceFile src("funkytown.sv",
                            "function void ff;\n"
                            "endfunction\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  MUST_ASSIGN_LOOKUP_SYMBOL(function_ff, root_symbol, "ff");
  EXPECT_EQ(function_ff_info.type, SymbolType::kFunction);
  EXPECT_EQ(function_ff_info.file_origin, &src);
  ASSERT_NE(function_ff_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(verible::StringSpanOfSymbol(
                *function_ff_info.declared_type.syntax_origin),
            "void");

  EXPECT_TRUE(function_ff_info.local_references_to_bind.empty());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_TRUE(resolve_diagnostics.empty());
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
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  MUST_ASSIGN_LOOKUP_SYMBOL(class_cc, root_symbol, "cc");
  MUST_ASSIGN_LOOKUP_SYMBOL(function_ff, root_symbol, "ff");
  EXPECT_EQ(function_ff_info.type, SymbolType::kFunction);
  EXPECT_EQ(function_ff_info.file_origin, &src);
  ASSERT_NE(function_ff_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(verible::StringSpanOfSymbol(
                *function_ff_info.declared_type.syntax_origin),
            "cc");
  const ReferenceComponentNode* cc_ref =
      function_ff_info.declared_type.user_defined_type;
  ASSERT_NE(cc_ref, nullptr);
  const ReferenceComponent& cc_ref_comp(cc_ref->Value());
  EXPECT_EQ(cc_ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(cc_ref_comp.identifier, "cc");
  EXPECT_EQ(cc_ref_comp.resolved_symbol, nullptr);

  // There should be one reference to return type "cc" of function "ff".
  EXPECT_EQ(root_symbol.Value().local_references_to_bind.size(), 1);

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_TRUE(resolve_diagnostics.empty());

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
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  MUST_ASSIGN_LOOKUP_SYMBOL(module_mm, root_symbol, "mm");
  MUST_ASSIGN_LOOKUP_SYMBOL(function_ff, module_mm, "ff");
  EXPECT_EQ(function_ff_info.type, SymbolType::kFunction);
  EXPECT_EQ(function_ff_info.file_origin, &src);
  ASSERT_NE(function_ff_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(verible::StringSpanOfSymbol(
                *function_ff_info.declared_type.syntax_origin),
            "void");
  const ReferenceComponentNode* ff_type =
      function_ff_info.declared_type.user_defined_type;
  EXPECT_EQ(ff_type, nullptr);

  // There are no references to resolve.
  EXPECT_TRUE(root_symbol.Value().local_references_to_bind.empty());
  EXPECT_TRUE(module_mm.Value().local_references_to_bind.empty());
  EXPECT_TRUE(function_ff.Value().local_references_to_bind.empty());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_TRUE(resolve_diagnostics.empty());
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
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  MUST_ASSIGN_LOOKUP_SYMBOL(class_cc, root_symbol, "cc");
  MUST_ASSIGN_LOOKUP_SYMBOL(function_ff, class_cc, "ff");
  EXPECT_EQ(function_ff_info.type, SymbolType::kFunction);
  EXPECT_EQ(function_ff_info.file_origin, &src);
  ASSERT_NE(function_ff_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(verible::StringSpanOfSymbol(
                *function_ff_info.declared_type.syntax_origin),
            "int");
  const ReferenceComponentNode* ff_type =
      function_ff_info.declared_type.user_defined_type;
  EXPECT_EQ(ff_type, nullptr);

  // There are no references to resolve.
  EXPECT_TRUE(root_symbol.Value().local_references_to_bind.empty());
  EXPECT_TRUE(class_cc.Value().local_references_to_bind.empty());
  EXPECT_TRUE(function_ff.Value().local_references_to_bind.empty());

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_TRUE(resolve_diagnostics.empty());
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
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  MUST_ASSIGN_LOOKUP_SYMBOL(package_aa, root_symbol, "aa");
  MUST_ASSIGN_LOOKUP_SYMBOL(package_bb, root_symbol, "bb");
  MUST_ASSIGN_LOOKUP_SYMBOL(class_vv, package_aa, "vv");
  MUST_ASSIGN_LOOKUP_SYMBOL(class_cc, package_bb, "cc");
  MUST_ASSIGN_LOOKUP_SYMBOL(function_ff, class_cc, "ff");

  EXPECT_EQ(function_ff_info.type, SymbolType::kFunction);
  EXPECT_EQ(function_ff_info.file_origin, &src);
  ASSERT_NE(function_ff_info.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(verible::StringSpanOfSymbol(
                *function_ff_info.declared_type.syntax_origin),
            "aa::vv");

  // return type points to the last component of the chain, "vv"
  const ReferenceComponentNode* vv_ref =
      function_ff_info.declared_type.user_defined_type;
  ASSERT_NE(vv_ref, nullptr);
  const ReferenceComponent& vv_ref_comp(vv_ref->Value());
  EXPECT_EQ(vv_ref_comp.ref_type, ReferenceType::kDirectMember);
  EXPECT_EQ(vv_ref_comp.identifier, "vv");
  EXPECT_EQ(vv_ref_comp.resolved_symbol, nullptr);

  // dependent reference parent is "aa" in "aa::vv"
  const ReferenceComponentNode* aa_ref = vv_ref->Parent();
  ASSERT_NE(aa_ref, nullptr);
  const ReferenceComponent& aa_ref_comp(aa_ref->Value());
  EXPECT_EQ(aa_ref_comp.ref_type, ReferenceType::kUnqualified);
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
    EXPECT_TRUE(resolve_diagnostics.empty());

    // Expect to resolve type reference chain "aa:vv"
    EXPECT_EQ(aa_ref_comp.resolved_symbol, &package_aa);
    EXPECT_EQ(vv_ref_comp.resolved_symbol, &class_vv);
  }
}

static bool SourceFileLess(const TestVerilogSourceFile* left,
                           const TestVerilogSourceFile* right) {
  return left->ReferencedPath() < right->ReferencedPath();
}

static void SortSourceFiles(
    std::vector<const TestVerilogSourceFile*>* sources) {
  std::sort(sources->begin(), sources->end(), SourceFileLess);
}

static bool PermuteSourceFiles(
    std::vector<const TestVerilogSourceFile*>* sources) {
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
  std::vector<const TestVerilogSourceFile*> ordering(
      {&pp_src, &qq_src, &ss_src});
  // start with the lexicographically "lowest" permutation
  SortSourceFiles(&ordering);
  int count = 0;
  do {
    SymbolTable symbol_table(nullptr);
    const SymbolTableNode& root_symbol(symbol_table.Root());

    for (const auto* src : ordering) {
      const auto build_diagnostics = BuildSymbolTable(*src, &symbol_table);
      EXPECT_TRUE(build_diagnostics.empty())
          << "Unexpected diagnostic:\n"
          << build_diagnostics.front().message();
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
        const ReferenceComponentNode* ref_node = pp_type->LastLeaf();
        ASSERT_NE(ref_node, nullptr);
        const ReferenceComponent& ref(ref_node->Value());
        EXPECT_EQ(ref.identifier, "pp");
        EXPECT_TRUE(verible::IsSubRange(ref.identifier,
                                        qq_src.GetTextStructure()->Contents()));
        EXPECT_EQ(ref.ref_type, ReferenceType::kUnqualified);
        EXPECT_EQ(ref.resolved_symbol, nullptr);
      }
      {  // self-reference to "pp_inst" instance
        ASSIGN_MUST_FIND_EXACTLY_ONE_REF(pp_inst_self_ref, ref_map, "pp_inst");
        EXPECT_TRUE(pp_inst_self_ref->components->is_leaf());  // no named ports
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
        const ReferenceComponentNode* ref_node = qq_type->LastLeaf();
        ASSERT_NE(ref_node, nullptr);
        const ReferenceComponent& ref(ref_node->Value());
        EXPECT_EQ(ref.identifier, "qq");
        EXPECT_TRUE(verible::IsSubRange(ref.identifier,
                                        ss_src.GetTextStructure()->Contents()));
        EXPECT_EQ(ref.ref_type, ReferenceType::kUnqualified);
        EXPECT_EQ(ref.resolved_symbol, nullptr);
      }
      {  // self-reference to "qq_inst" instance
        ASSIGN_MUST_FIND_EXACTLY_ONE_REF(qq_inst_self_ref, ref_map, "qq_inst");
        EXPECT_TRUE(qq_inst_self_ref->components->is_leaf());  // no named ports
        // self-reference is already bound.
        EXPECT_EQ(qq_inst_self_ref->components->Value().resolved_symbol,
                  &qq_inst);
      }
    }

    {  // Verify pp_inst's type info
      EXPECT_TRUE(pp_inst_info.local_references_to_bind.empty());
      EXPECT_NE(pp_inst_info.declared_type.user_defined_type, nullptr);
      const ReferenceComponent& pp_type(
          pp_inst_info.declared_type.user_defined_type->Value());
      EXPECT_EQ(pp_type.identifier, "pp");
      EXPECT_EQ(pp_type.resolved_symbol, nullptr);  // nothing resolved yet
      EXPECT_EQ(pp_type.ref_type, ReferenceType::kUnqualified);
      EXPECT_EQ(pp_inst_info.file_origin, &qq_src);
    }

    {  // Verify qq_inst's type info
      EXPECT_TRUE(qq_inst_info.local_references_to_bind.empty());
      EXPECT_NE(qq_inst_info.declared_type.user_defined_type, nullptr);
      const ReferenceComponent& qq_type(
          qq_inst_info.declared_type.user_defined_type->Value());
      EXPECT_EQ(qq_type.identifier, "qq");
      EXPECT_EQ(qq_type.resolved_symbol, nullptr);  // nothing resolved yet
      EXPECT_EQ(qq_type.ref_type, ReferenceType::kUnqualified);
      EXPECT_EQ(qq_inst_info.file_origin, &ss_src);
    }

    // Resolve symbols.
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);

    EXPECT_TRUE(resolve_diagnostics.empty());
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
  constexpr absl::string_view  //
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
  for (const auto* file : {&file1, &file2, &file3}) {
    const auto status_or_file =
        project.OpenTranslationUnit(Basename(file->filename()));
    ASSERT_TRUE(status_or_file.ok());
  }

  SymbolTable symbol_table(&project);
  EXPECT_EQ(symbol_table.Project(), &project);

  // Caller decides order of processing files, which doesn't matter for this
  // example.
  std::vector<absl::Status> build_diagnostics;
  for (const auto* file : {&file3, &file2, &file1}) {
    symbol_table.BuildSingleTranslationUnit(Basename(file->filename()),
                                            &build_diagnostics);
    ASSERT_TRUE(build_diagnostics.empty())
        << "Unexpected diagnostic:\n"
        << build_diagnostics.front().message();
  }

  const SymbolTableNode& root_symbol(symbol_table.Root());

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
      const ReferenceComponentNode* ref_node = pp_type->LastLeaf();
      ASSERT_NE(ref_node, nullptr);
      const ReferenceComponent& ref(ref_node->Value());
      EXPECT_EQ(ref.identifier, "pp");
      EXPECT_EQ(ref.ref_type, ReferenceType::kUnqualified);
      EXPECT_EQ(ref.resolved_symbol, nullptr);
    }
    {  // self-reference to "pp_inst" instance
      ASSIGN_MUST_FIND_EXACTLY_ONE_REF(pp_inst_self_ref, ref_map, "pp_inst");
      EXPECT_TRUE(pp_inst_self_ref->components->is_leaf());  // no named ports
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
      const ReferenceComponentNode* ref_node = qq_type->LastLeaf();
      ASSERT_NE(ref_node, nullptr);
      const ReferenceComponent& ref(ref_node->Value());
      EXPECT_EQ(ref.identifier, "qq");
      EXPECT_EQ(ref.ref_type, ReferenceType::kUnqualified);
      EXPECT_EQ(ref.resolved_symbol, nullptr);
    }
    {  // self-reference to "qq_inst" instance
      ASSIGN_MUST_FIND_EXACTLY_ONE_REF(qq_inst_self_ref, ref_map, "qq_inst");
      EXPECT_TRUE(qq_inst_self_ref->components->is_leaf());  // no named ports
      // self-reference is already bound.
      EXPECT_EQ(qq_inst_self_ref->components->Value().resolved_symbol,
                &qq_inst);
    }
  }

  {  // Verify pp_inst's type info
    EXPECT_TRUE(pp_inst_info.local_references_to_bind.empty());
    EXPECT_NE(pp_inst_info.declared_type.user_defined_type, nullptr);
    const ReferenceComponent& pp_type(
        pp_inst_info.declared_type.user_defined_type->Value());
    EXPECT_EQ(pp_type.identifier, "pp");
    EXPECT_EQ(pp_type.resolved_symbol, nullptr);  // nothing resolved yet
    EXPECT_EQ(pp_type.ref_type, ReferenceType::kUnqualified);
  }

  {  // Verify qq_inst's type info
    EXPECT_TRUE(qq_inst_info.local_references_to_bind.empty());
    EXPECT_NE(qq_inst_info.declared_type.user_defined_type, nullptr);
    const ReferenceComponent& qq_type(
        qq_inst_info.declared_type.user_defined_type->Value());
    EXPECT_EQ(qq_type.identifier, "qq");
    EXPECT_EQ(qq_type.resolved_symbol, nullptr);  // nothing resolved yet
    EXPECT_EQ(qq_type.ref_type, ReferenceType::kUnqualified);
  }

  // Resolve symbols.
  std::vector<absl::Status> resolve_diagnostics;
  symbol_table.Resolve(&resolve_diagnostics);

  EXPECT_TRUE(resolve_diagnostics.empty());
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
  EXPECT_THAT(build_diagnostics.front().message(), HasSubstr("No such file"));
}

TEST(BuildSymbolTableTest, ModuleInstancesFromProjectFilesGood) {
  const auto tempdir = ::testing::TempDir();
  const std::string sources_dir = JoinPath(tempdir, __FUNCTION__);
  ASSERT_TRUE(CreateDir(sources_dir).ok());

  VerilogProject project(sources_dir, {/* no include path */});

  // Linear dependency chain between 3 files.  Order arbitrarily chosen.
  constexpr absl::string_view  //
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
  for (const auto* file : {&file1, &file2, &file3}) {
    const auto status_or_file =
        project.OpenTranslationUnit(Basename(file->filename()));
    ASSERT_TRUE(status_or_file.ok());
  }

  SymbolTable symbol_table(&project);
  EXPECT_EQ(symbol_table.Project(), &project);

  std::vector<absl::Status> build_diagnostics;
  symbol_table.Build(&build_diagnostics);
  ASSERT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  const SymbolTableNode& root_symbol(symbol_table.Root());

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
      const ReferenceComponentNode* ref_node = pp_type->LastLeaf();
      ASSERT_NE(ref_node, nullptr);
      const ReferenceComponent& ref(ref_node->Value());
      EXPECT_EQ(ref.identifier, "pp");
      EXPECT_EQ(ref.ref_type, ReferenceType::kUnqualified);
      EXPECT_EQ(ref.resolved_symbol, nullptr);
    }
    {  // self-reference to "pp_inst" instance
      ASSIGN_MUST_FIND_EXACTLY_ONE_REF(pp_inst_self_ref, ref_map, "pp_inst");
      EXPECT_TRUE(pp_inst_self_ref->components->is_leaf());  // no named ports
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
      const ReferenceComponentNode* ref_node = qq_type->LastLeaf();
      ASSERT_NE(ref_node, nullptr);
      const ReferenceComponent& ref(ref_node->Value());
      EXPECT_EQ(ref.identifier, "qq");
      EXPECT_EQ(ref.ref_type, ReferenceType::kUnqualified);
      EXPECT_EQ(ref.resolved_symbol, nullptr);
    }
    {  // self-reference to "qq_inst" instance
      ASSIGN_MUST_FIND_EXACTLY_ONE_REF(qq_inst_self_ref, ref_map, "qq_inst");
      EXPECT_TRUE(qq_inst_self_ref->components->is_leaf());  // no named ports
      // self-reference is already bound.
      EXPECT_EQ(qq_inst_self_ref->components->Value().resolved_symbol,
                &qq_inst);
    }
  }

  {  // Verify pp_inst's type info
    EXPECT_TRUE(pp_inst_info.local_references_to_bind.empty());
    EXPECT_NE(pp_inst_info.declared_type.user_defined_type, nullptr);
    const ReferenceComponent& pp_type(
        pp_inst_info.declared_type.user_defined_type->Value());
    EXPECT_EQ(pp_type.identifier, "pp");
    EXPECT_EQ(pp_type.resolved_symbol, nullptr);  // nothing resolved yet
    EXPECT_EQ(pp_type.ref_type, ReferenceType::kUnqualified);
  }

  {  // Verify qq_inst's type info
    EXPECT_TRUE(qq_inst_info.local_references_to_bind.empty());
    EXPECT_NE(qq_inst_info.declared_type.user_defined_type, nullptr);
    const ReferenceComponent& qq_type(
        qq_inst_info.declared_type.user_defined_type->Value());
    EXPECT_EQ(qq_type.identifier, "qq");
    EXPECT_EQ(qq_type.resolved_symbol, nullptr);  // nothing resolved yet
    EXPECT_EQ(qq_type.ref_type, ReferenceType::kUnqualified);
  }

  // Resolve symbols.
  std::vector<absl::Status> resolve_diagnostics;
  symbol_table.Resolve(&resolve_diagnostics);

  EXPECT_TRUE(resolve_diagnostics.empty());
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
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

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
      const ReferenceComponentNode* ref_node = ss_type->LastLeaf();
      ASSERT_NE(ref_node, nullptr);
      const ReferenceComponent& ref(ref_node->Value());
      EXPECT_EQ(ref.identifier, "ss");
      EXPECT_TRUE(verible::IsSubRange(ref.identifier,
                                      src.GetTextStructure()->Contents()));
      EXPECT_EQ(ref.ref_type, ReferenceType::kUnqualified);
      EXPECT_EQ(ref.resolved_symbol, nullptr);
    }
    {  // self-reference to "ss_inst" instance
      ASSIGN_MUST_FIND_EXACTLY_ONE_REF(ss_inst_self_ref, ref_map, "ss_inst");
      EXPECT_TRUE(ss_inst_self_ref->components->is_leaf());  // no named ports
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
      const ReferenceComponentNode* ref_node = pp_type->LastLeaf();
      ASSERT_NE(ref_node, nullptr);
      const ReferenceComponent& ref(ref_node->Value());
      EXPECT_EQ(ref.identifier, "pp");
      EXPECT_TRUE(verible::IsSubRange(ref.identifier,
                                      src.GetTextStructure()->Contents()));
      EXPECT_EQ(ref.ref_type, ReferenceType::kUnqualified);
      EXPECT_EQ(ref.resolved_symbol, nullptr);
    }
    {  // self-reference to "pp_inst" instance
      ASSIGN_MUST_FIND_EXACTLY_ONE_REF(pp_inst_self_ref, ref_map, "pp_inst");
      EXPECT_TRUE(pp_inst_self_ref->components->is_leaf());  // no named ports
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
      const ReferenceComponentNode* ref_node = qq_type->LastLeaf();
      ASSERT_NE(ref_node, nullptr);
      const ReferenceComponent& ref(ref_node->Value());
      EXPECT_EQ(ref.identifier, "qq");
      EXPECT_TRUE(verible::IsSubRange(ref.identifier,
                                      src.GetTextStructure()->Contents()));
      EXPECT_EQ(ref.ref_type, ReferenceType::kUnqualified);
      EXPECT_EQ(ref.resolved_symbol, nullptr);
    }
    {  // self-reference to "qq_inst" instance
      ASSIGN_MUST_FIND_EXACTLY_ONE_REF(qq_inst_self_ref, ref_map, "qq_inst");
      EXPECT_TRUE(qq_inst_self_ref->components->is_leaf());  // no named ports
      // self-reference is already bound.
      EXPECT_EQ(qq_inst_self_ref->components->Value().resolved_symbol,
                &qq_inst);
    }
  }

  {  // Verify ss_inst's type info
    EXPECT_TRUE(ss_inst_info.local_references_to_bind.empty());
    EXPECT_NE(ss_inst_info.declared_type.user_defined_type, nullptr);
    const ReferenceComponent& ss_type(
        ss_inst_info.declared_type.user_defined_type->Value());
    EXPECT_EQ(ss_type.identifier, "ss");
    EXPECT_EQ(ss_type.resolved_symbol, nullptr);  // nothing resolved yet
    EXPECT_EQ(ss_type.ref_type, ReferenceType::kUnqualified);
    EXPECT_EQ(ss_inst_info.file_origin, &src);
  }

  {  // Verify pp_inst's type info
    EXPECT_TRUE(pp_inst_info.local_references_to_bind.empty());
    EXPECT_NE(pp_inst_info.declared_type.user_defined_type, nullptr);
    const ReferenceComponent& pp_type(
        pp_inst_info.declared_type.user_defined_type->Value());
    EXPECT_EQ(pp_type.identifier, "pp");
    EXPECT_EQ(pp_type.resolved_symbol, nullptr);  // nothing resolved yet
    EXPECT_EQ(pp_type.ref_type, ReferenceType::kUnqualified);
    EXPECT_EQ(pp_inst_info.file_origin, &src);
  }

  {  // Verify qq_inst's type info
    EXPECT_TRUE(qq_inst_info.local_references_to_bind.empty());
    EXPECT_NE(qq_inst_info.declared_type.user_defined_type, nullptr);
    const ReferenceComponent& qq_type(
        qq_inst_info.declared_type.user_defined_type->Value());
    EXPECT_EQ(qq_type.identifier, "qq");
    EXPECT_EQ(qq_type.resolved_symbol, nullptr);  // nothing resolved yet
    EXPECT_EQ(qq_type.ref_type, ReferenceType::kUnqualified);
    EXPECT_EQ(qq_inst_info.file_origin, &src);
  }

  // Resolve symbols.
  std::vector<absl::Status> resolve_diagnostics;
  symbol_table.Resolve(&resolve_diagnostics);

  EXPECT_TRUE(resolve_diagnostics.empty());
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
  std::vector<const TestVerilogSourceFile*> ordering(
      {&pp_src, &qq_src, &ss_src});
  // start with the lexicographically "lowest" permutation
  SortSourceFiles(&ordering);
  int count = 0;
  do {
    SymbolTable symbol_table(nullptr);
    const SymbolTableNode& root_symbol(symbol_table.Root());

    for (const auto* src : ordering) {
      const auto build_diagnostics = BuildSymbolTable(*src, &symbol_table);
      EXPECT_TRUE(build_diagnostics.empty())
          << "Unexpected diagnostic:\n"
          << build_diagnostics.front().message();
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
        const ReferenceComponentNode* ref_node = ss_type->LastLeaf();
        ASSERT_NE(ref_node, nullptr);
        const ReferenceComponent& ref(ref_node->Value());
        EXPECT_EQ(ref.identifier, "ss");
        EXPECT_TRUE(verible::IsSubRange(ref.identifier,
                                        pp_src.GetTextStructure()->Contents()));
        EXPECT_EQ(ref.ref_type, ReferenceType::kUnqualified);
        EXPECT_EQ(ref.resolved_symbol, nullptr);
      }
      {  // self-reference to "ss_inst" instance
        ASSIGN_MUST_FIND_EXACTLY_ONE_REF(ss_inst_self_ref, ref_map, "ss_inst");
        EXPECT_TRUE(ss_inst_self_ref->components->is_leaf());  // no named ports
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
        const ReferenceComponentNode* ref_node = pp_type->LastLeaf();
        ASSERT_NE(ref_node, nullptr);
        const ReferenceComponent& ref(ref_node->Value());
        EXPECT_EQ(ref.identifier, "pp");
        EXPECT_TRUE(verible::IsSubRange(ref.identifier,
                                        qq_src.GetTextStructure()->Contents()));
        EXPECT_EQ(ref.ref_type, ReferenceType::kUnqualified);
        EXPECT_EQ(ref.resolved_symbol, nullptr);
      }
      {  // self-reference to "pp_inst" instance
        ASSIGN_MUST_FIND_EXACTLY_ONE_REF(pp_inst_self_ref, ref_map, "pp_inst");
        EXPECT_TRUE(pp_inst_self_ref->components->is_leaf());  // no named ports
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
        const ReferenceComponentNode* ref_node = qq_type->LastLeaf();
        ASSERT_NE(ref_node, nullptr);
        const ReferenceComponent& ref(ref_node->Value());
        EXPECT_EQ(ref.identifier, "qq");
        EXPECT_TRUE(verible::IsSubRange(ref.identifier,
                                        ss_src.GetTextStructure()->Contents()));
        EXPECT_EQ(ref.ref_type, ReferenceType::kUnqualified);
        EXPECT_EQ(ref.resolved_symbol, nullptr);
      }
      {  // self-reference to "qq_inst" instance
        ASSIGN_MUST_FIND_EXACTLY_ONE_REF(qq_inst_self_ref, ref_map, "qq_inst");
        EXPECT_TRUE(qq_inst_self_ref->components->is_leaf());  // no named ports
        // self-reference is already bound.
        EXPECT_EQ(qq_inst_self_ref->components->Value().resolved_symbol,
                  &qq_inst);
      }
    }

    {  // Verify ss_inst's type info
      EXPECT_TRUE(ss_inst_info.local_references_to_bind.empty());
      EXPECT_NE(ss_inst_info.declared_type.user_defined_type, nullptr);
      const ReferenceComponent& ss_type(
          ss_inst_info.declared_type.user_defined_type->Value());
      EXPECT_EQ(ss_type.identifier, "ss");
      EXPECT_EQ(ss_type.resolved_symbol, nullptr);  // nothing resolved yet
      EXPECT_EQ(ss_type.ref_type, ReferenceType::kUnqualified);
      EXPECT_EQ(ss_inst_info.file_origin, &pp_src);
    }

    {  // Verify pp_inst's type info
      EXPECT_TRUE(pp_inst_info.local_references_to_bind.empty());
      EXPECT_NE(pp_inst_info.declared_type.user_defined_type, nullptr);
      const ReferenceComponent& pp_type(
          pp_inst_info.declared_type.user_defined_type->Value());
      EXPECT_EQ(pp_type.identifier, "pp");
      EXPECT_EQ(pp_type.resolved_symbol, nullptr);  // nothing resolved yet
      EXPECT_EQ(pp_type.ref_type, ReferenceType::kUnqualified);
      EXPECT_EQ(pp_inst_info.file_origin, &qq_src);
    }

    {  // Verify qq_inst's type info
      EXPECT_TRUE(qq_inst_info.local_references_to_bind.empty());
      EXPECT_NE(qq_inst_info.declared_type.user_defined_type, nullptr);
      const ReferenceComponent& qq_type(
          qq_inst_info.declared_type.user_defined_type->Value());
      EXPECT_EQ(qq_type.identifier, "qq");
      EXPECT_EQ(qq_type.resolved_symbol, nullptr);  // nothing resolved yet
      EXPECT_EQ(qq_type.ref_type, ReferenceType::kUnqualified);
      EXPECT_EQ(qq_inst_info.file_origin, &ss_src);
    }

    // Resolve symbols.
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);

    EXPECT_TRUE(resolve_diagnostics.empty());
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
  const SymbolTableNode& root_symbol(symbol_table.Root());

  std::vector<absl::Status> build_diagnostics;
  symbol_table.Build(&build_diagnostics);
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  MUST_ASSIGN_LOOKUP_SYMBOL(pp, root_symbol, "pp");

  const VerilogSourceFile* included = project.LookupRegisteredFile("module.sv");
  ASSERT_NE(included, nullptr);
  EXPECT_EQ(pp_info.file_origin, included);

  // Resolve symbols.  Nothing to resolve.
  std::vector<absl::Status> resolve_diagnostics;
  symbol_table.Resolve(&resolve_diagnostics);
  EXPECT_TRUE(resolve_diagnostics.empty());
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
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  // Resolve symbols.  Nothing to resolve.
  std::vector<absl::Status> resolve_diagnostics;
  symbol_table.Resolve(&resolve_diagnostics);
  EXPECT_TRUE(resolve_diagnostics.empty());
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
  const SymbolTableNode& root_symbol(symbol_table.Root());

  std::vector<absl::Status> build_diagnostics;
  symbol_table.Build(&build_diagnostics);
  ASSERT_FALSE(build_diagnostics.empty());
  EXPECT_EQ(build_diagnostics.front().code(), absl::StatusCode::kNotFound);

  EXPECT_TRUE(root_symbol.Children().empty());

  // Resolve symbols.  Nothing to resolve.
  std::vector<absl::Status> resolve_diagnostics;
  symbol_table.Resolve(&resolve_diagnostics);
  EXPECT_TRUE(resolve_diagnostics.empty());
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
  EXPECT_TRUE(resolve_diagnostics.empty());
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
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  // Resolve symbols.  Nothing to resolve.
  std::vector<absl::Status> resolve_diagnostics;
  symbol_table.Resolve(&resolve_diagnostics);
  EXPECT_TRUE(resolve_diagnostics.empty());
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
  const VerilogSourceFile* pp_file = *file_or_status;
  ASSERT_NE(pp_file, nullptr);

  SymbolTable symbol_table(&project);
  const SymbolTableNode& root_symbol(symbol_table.Root());

  std::vector<absl::Status> build_diagnostics;
  symbol_table.Build(&build_diagnostics);
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  MUST_ASSIGN_LOOKUP_SYMBOL(pp, root_symbol, "pp");
  MUST_ASSIGN_LOOKUP_SYMBOL(qq, root_symbol, "qq");
  MUST_ASSIGN_LOOKUP_SYMBOL(pp_ww, pp, "ww");
  MUST_ASSIGN_LOOKUP_SYMBOL(qq_ww, qq, "ww");

  const VerilogSourceFile* included = project.LookupRegisteredFile("wires.sv");
  ASSERT_NE(included, nullptr);
  EXPECT_EQ(pp_info.file_origin, pp_file);
  EXPECT_EQ(qq_info.file_origin, pp_file);
  EXPECT_EQ(pp_ww_info.file_origin, included);
  EXPECT_EQ(qq_ww_info.file_origin, included);

  // Resolve symbols.  Nothing to resolve.
  std::vector<absl::Status> resolve_diagnostics;
  symbol_table.Resolve(&resolve_diagnostics);
  EXPECT_TRUE(resolve_diagnostics.empty());
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
  const VerilogSourceFile* pp_file = *pp_file_or_status;
  ASSERT_NE(pp_file, nullptr);

  const auto qq_file_or_status =
      project.OpenTranslationUnit(Basename(qq_src.filename()));
  ASSERT_TRUE(qq_file_or_status.ok()) << qq_file_or_status.status().message();
  const VerilogSourceFile* qq_file = *qq_file_or_status;
  ASSERT_NE(qq_file, nullptr);

  SymbolTable symbol_table(&project);
  const SymbolTableNode& root_symbol(symbol_table.Root());

  std::vector<absl::Status> build_diagnostics;
  symbol_table.Build(&build_diagnostics);
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  MUST_ASSIGN_LOOKUP_SYMBOL(pp, root_symbol, "pp");
  MUST_ASSIGN_LOOKUP_SYMBOL(qq, root_symbol, "qq");
  MUST_ASSIGN_LOOKUP_SYMBOL(pp_ww, pp, "ww");
  MUST_ASSIGN_LOOKUP_SYMBOL(qq_ww, qq, "ww");

  const VerilogSourceFile* included = project.LookupRegisteredFile("wires.sv");
  ASSERT_NE(included, nullptr);
  EXPECT_EQ(pp_info.file_origin, pp_file);
  EXPECT_EQ(qq_info.file_origin, qq_file);
  EXPECT_EQ(pp_ww_info.file_origin, included);
  EXPECT_EQ(qq_ww_info.file_origin, included);

  // Resolve symbols.  Nothing to resolve.
  std::vector<absl::Status> resolve_diagnostics;
  symbol_table.Resolve(&resolve_diagnostics);
  EXPECT_TRUE(resolve_diagnostics.empty());
}

struct FileListTestCase {
  absl::string_view contents;
  std::vector<absl::string_view> expected_files;
};

TEST(ParseSourceFileListFromFileTest, FileNotFound) {
  const auto files_or_status(ParseSourceFileListFromFile("/no/such/file.txt"));
  EXPECT_FALSE(files_or_status.ok());
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
  for (const auto& test : kTestCases) {
    const ScopedTestFile test_file(testing::TempDir(), test.contents);
    const auto files_or_status(
        ParseSourceFileListFromFile(test_file.filename()));
    ASSERT_TRUE(files_or_status.ok()) << files_or_status.status().message();
    EXPECT_THAT(*files_or_status, ElementsAreArray(test.expected_files))
        << "input: " << test.contents;
  }
}

}  // namespace
}  // namespace verilog
