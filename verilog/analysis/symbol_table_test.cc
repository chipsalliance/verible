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

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "common/text/tree_utils.h"
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

using testing::HasSubstr;

// Grab the first reference in local_references_to_bind that matches by name.
static const DependentReferences& GetFirstDependentReferenceByName(
    const SymbolInfo& sym, absl::string_view name) {
  for (const auto& ref : sym.local_references_to_bind) {
    if (ref.components->Value().identifier == name) return ref;
  }
  CHECK(false) << "No reference to \"" << name << "\" found.";
}

// An in-memory source file that doesn't require file-system access,
// nor create temporary files.
// TODO: if this is useful elsewhere, move to a test-only library.
class TestVerilogSourceFile : public VerilogSourceFile {
 public:
  // filename can be fake, it is not used to open any file.
  TestVerilogSourceFile(absl::string_view filename, absl::string_view contents)
      : VerilogSourceFile(filename, filename), contents_for_open_(contents) {}

  // Load text into analyzer structure without actually opening a file.
  absl::Status Open() override {
    analyzed_structure_ = ABSL_DIE_IF_NULL(
        absl::make_unique<VerilogAnalyzer>(contents_for_open_, ResolvedPath()));
    state_ = State::kOpened;
    status_ = absl::OkStatus();
    return status_;
  }

 private:
  const absl::string_view contents_for_open_;
};

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
      Node(Data{.identifier = "yy", .ref_type = ReferenceType::kStaticMember},
           Node(Data{.identifier = "zz",
                     .ref_type = ReferenceType::kObjectMember})));
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

TEST(BuildSymbolTableTest, IntegrityCheckResolvedSymbol) {
  const auto test_func = []() {
    SymbolTable::Tester symbol_table_1(nullptr), symbol_table_2(nullptr);
    SymbolTableNode& root1(symbol_table_1.MutableRoot());
    SymbolTableNode& root2(symbol_table_2.MutableRoot());
    // Deliberately point from one symbol table to the other.
    root1.Value().local_references_to_bind.push_back(DependentReferences{
        .components = absl::make_unique<ReferenceComponentNode>(
            ReferenceComponent{.identifier = "foo",
                               .ref_type = ReferenceType::kUnqualified,
                               .resolved_symbol = &root2})});
    // CheckIntegrity() will fail on destruction of symbol_table_1.
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
  TestVerilogSourceFile src("foobar.sv", "module;\nendmodule\n");
  const auto status = src.Parse();
  EXPECT_FALSE(status.ok());
  SymbolTable symbol_table(nullptr);

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

TEST(BuildSymbolTableTest, ModuleDeclarationSingleEmpty) {
  TestVerilogSourceFile src("foobar.sv", "module m;\nendmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  const auto found = root_symbol.Find("m");
  ASSERT_NE(found, root_symbol.end());
  EXPECT_EQ(found->first, "m");
  const SymbolInfo& module_symbol(found->second.Value());
  EXPECT_EQ(module_symbol.type, SymbolType::kModule);
  EXPECT_EQ(module_symbol.file_origin, &src);
  EXPECT_EQ(module_symbol.declared_type.syntax_origin,
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

  const auto found = root_symbol.Find("m");
  ASSERT_NE(found, root_symbol.end());
  EXPECT_EQ(found->first, "m");
  const SymbolTableNode& module_node(found->second);
  const SymbolInfo& module_symbol(module_node.Value());
  EXPECT_EQ(module_symbol.type, SymbolType::kModule);
  EXPECT_EQ(module_symbol.file_origin, &src);
  EXPECT_EQ(module_symbol.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  static constexpr absl::string_view members[] = {"w1", "w2", "l1", "l2"};
  for (const auto& member : members) {
    const auto found_member = module_node.Find(member);
    ASSERT_NE(found_member, module_node.end()) << "looking up: " << member;
    const SymbolTableNode& member_node(found_member->second);
    EXPECT_EQ(*member_node.Key(), member);
    EXPECT_EQ(member_node.Value().type, SymbolType::kDataNetVariableInstance);
    EXPECT_EQ(member_node.Value().declared_type.user_defined_type,
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

  const auto found = root_symbol.Find("m");
  ASSERT_NE(found, root_symbol.end());
  EXPECT_EQ(found->first, "m");
  const SymbolTableNode& module_node(found->second);
  const SymbolInfo& module_symbol(module_node.Value());
  EXPECT_EQ(module_symbol.type, SymbolType::kModule);
  EXPECT_EQ(module_symbol.file_origin, &src);
  EXPECT_EQ(module_symbol.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type
  ASSERT_EQ(build_diagnostics.size(), 1);
  const absl::Status& err_status(build_diagnostics.front());
  EXPECT_EQ(err_status.code(), absl::StatusCode::kAlreadyExists);
  EXPECT_THAT(err_status.message(),
              HasSubstr("\"y1\" is already defined in the $root::m scope"));

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

  const auto found = root_symbol.Find("m");
  ASSERT_NE(found, root_symbol.end());
  EXPECT_EQ(found->first, "m");
  const SymbolTableNode& module_node(found->second);
  const SymbolInfo& module_symbol(module_node.Value());
  EXPECT_EQ(module_symbol.type, SymbolType::kModule);
  EXPECT_EQ(module_symbol.file_origin, &src);
  EXPECT_EQ(module_symbol.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  static constexpr absl::string_view members[] = {"clk", "q"};
  for (const auto& member : members) {
    const auto found_member = module_node.Find(member);
    ASSERT_NE(found_member, module_node.end()) << "looking up: " << member;
    const SymbolTableNode& member_node(found_member->second);
    EXPECT_EQ(*member_node.Key(), member);
    EXPECT_EQ(member_node.Value().type, SymbolType::kDataNetVariableInstance);
    EXPECT_EQ(member_node.Value().declared_type.user_defined_type,
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
    const auto found = root_symbol.Find(expected_module);
    ASSERT_NE(found, root_symbol.end());
    EXPECT_EQ(found->first, expected_module);
    const SymbolInfo& module_symbol(found->second.Value());
    EXPECT_EQ(module_symbol.type, SymbolType::kModule);
    EXPECT_EQ(module_symbol.file_origin, &src);
    EXPECT_EQ(module_symbol.declared_type.syntax_origin,
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

  const absl::string_view expected_module("mm");
  const auto found = root_symbol.Find(expected_module);
  ASSERT_NE(found, root_symbol.end());
  EXPECT_EQ(found->first, expected_module);
  const SymbolInfo& module_symbol(found->second.Value());
  EXPECT_EQ(module_symbol.type, SymbolType::kModule);
  EXPECT_EQ(module_symbol.file_origin, &src);
  EXPECT_EQ(module_symbol.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type
  ASSERT_EQ(build_diagnostics.size(), 1);
  EXPECT_EQ(build_diagnostics.front().code(), absl::StatusCode::kAlreadyExists);
  EXPECT_THAT(build_diagnostics.front().message(),
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
                            "module m_inner;\n"
                            "endmodule\n"
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();
  const auto found = root_symbol.Find("m_outer");
  ASSERT_NE(found, root_symbol.end());
  EXPECT_EQ(found->first, "m_outer");
  const SymbolTableNode& outer_module_node(found->second);
  {
    const SymbolInfo& module_symbol(outer_module_node.Value());
    EXPECT_EQ(module_symbol.type, SymbolType::kModule);
    EXPECT_EQ(module_symbol.file_origin, &src);
    EXPECT_EQ(module_symbol.declared_type.syntax_origin,
              nullptr);  // there is no module meta-type
  }

  const auto found_inner = outer_module_node.Find("m_inner");
  {
    ASSERT_NE(found_inner, outer_module_node.end());
    EXPECT_EQ(found_inner->first, "m_inner");
    const SymbolTableNode& inner_module_node(found_inner->second);
    const SymbolInfo& module_symbol(inner_module_node.Value());
    EXPECT_EQ(module_symbol.type, SymbolType::kModule);
    EXPECT_EQ(module_symbol.file_origin, &src);
    EXPECT_EQ(module_symbol.declared_type.syntax_origin,
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
                            "module mm;\nendmodule\n"
                            "module mm;\nendmodule\n"  // dupe
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  const auto found = root_symbol.Find("outer");
  ASSERT_NE(found, root_symbol.end());
  EXPECT_EQ(found->first, "outer");
  ASSERT_EQ(build_diagnostics.size(), 1);
  EXPECT_EQ(build_diagnostics.front().code(), absl::StatusCode::kAlreadyExists);
  EXPECT_THAT(build_diagnostics.front().message(),
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

    const auto found_pp = root_symbol.Find("pp");
    ASSERT_NE(found_pp, root_symbol.end());
    EXPECT_EQ(found_pp->first, "pp");
    // Goal: resolve the reference of "pp" to this definition node.
    const SymbolTableNode& pp(found_pp->second);

    // Inspect inside the "qq" module definition.
    const auto found_qq = root_symbol.Find("qq");
    ASSERT_NE(found_qq, root_symbol.end());
    EXPECT_EQ(found_qq->first, "qq");
    const SymbolTableNode& qq(found_qq->second);

    // "rr" is an instance of type "pp"
    const auto found_rr = qq.Find("rr");
    ASSERT_NE(found_rr, qq.end());
    EXPECT_EQ(found_rr->first, "rr");
    const SymbolTableNode& rr(found_rr->second);

    {
      const SymbolInfo& qq_info(qq.Value());
      // There is only one reference, and it is to the "pp" module type.
      ASSERT_EQ(qq_info.local_references_to_bind.size(), 2);
      EXPECT_EQ(qq_info.file_origin, &src);
      {
        const DependentReferences& pp_type(
            GetFirstDependentReferenceByName(qq_info, "pp"));
        const ReferenceComponentNode* ref_node = pp_type.LastLeaf();
        ASSERT_NE(ref_node, nullptr);
        const ReferenceComponent& ref(ref_node->Value());
        EXPECT_EQ(ref.identifier, "pp");
        EXPECT_TRUE(verible::IsSubRange(ref.identifier,
                                        src.GetTextStructure()->Contents()));
        EXPECT_EQ(ref.ref_type, ReferenceType::kUnqualified);
        EXPECT_EQ(ref.resolved_symbol, nullptr);
      }
      {
        const DependentReferences& rr_self_ref(
            GetFirstDependentReferenceByName(qq_info, "rr"));
        EXPECT_TRUE(rr_self_ref.components->is_leaf());  // no named ports
        // self-reference is already bound.
        EXPECT_EQ(rr_self_ref.components->Value().resolved_symbol, &rr);
      }
    }

    const SymbolInfo& rr_info(rr.Value());
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

  const auto found_pp = root_symbol.Find("pp");
  EXPECT_EQ(found_pp, root_symbol.end());

  // Inspect inside the "qq" module definition.
  const auto found_qq = root_symbol.Find("qq");
  ASSERT_NE(found_qq, root_symbol.end());
  EXPECT_EQ(found_qq->first, "qq");
  const SymbolTableNode& qq(found_qq->second);
  {
    const SymbolInfo& qq_info(qq.Value());
    // There is only one reference of interest, the "pp" module type.
    ASSERT_EQ(qq_info.local_references_to_bind.size(), 2);
    EXPECT_EQ(qq_info.file_origin, &src);
    const DependentReferences& pp_type(
        GetFirstDependentReferenceByName(qq_info, "pp"));
    {  // verify that a reference to "pp" was established
      const ReferenceComponentNode* ref_node = pp_type.LastLeaf();
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
  const auto found_rr = qq.Find("rr");
  ASSERT_NE(found_rr, qq.end());
  EXPECT_EQ(found_rr->first, "rr");

  const SymbolTableNode& rr(found_rr->second);
  const SymbolInfo& rr_info(rr.Value());
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
    EXPECT_EQ(resolve_diagnostics.size(), 1);
    const auto& err_status(resolve_diagnostics.front());
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

    const auto found_pp = root_symbol.Find("pp");
    ASSERT_NE(found_pp, root_symbol.end());
    EXPECT_EQ(found_pp->first, "pp");
    const SymbolTableNode& pp(found_pp->second);

    // Inspect inside the "qq" module definition.
    const auto found_qq = root_symbol.Find("qq");
    ASSERT_NE(found_qq, root_symbol.end());
    EXPECT_EQ(found_qq->first, "qq");
    const SymbolTableNode& qq(found_qq->second);
    {
      const SymbolInfo& qq_info(qq.Value());
      // There is only one type reference of interest, the "pp" module type.
      // The other two are instance self-references.
      ASSERT_EQ(qq_info.local_references_to_bind.size(), 3);
      EXPECT_EQ(qq_info.file_origin, &src);
      {
        const DependentReferences& pp_type(
            GetFirstDependentReferenceByName(qq_info, "pp"));
        const ReferenceComponentNode* ref_node = pp_type.LastLeaf();
        ASSERT_NE(ref_node, nullptr);
        const ReferenceComponent& ref(ref_node->Value());
        EXPECT_EQ(ref.identifier, "pp");
        EXPECT_TRUE(verible::IsSubRange(ref.identifier,
                                        src.GetTextStructure()->Contents()));
        EXPECT_EQ(ref.ref_type, ReferenceType::kUnqualified);
        EXPECT_EQ(ref.resolved_symbol, nullptr);
      }
    }

    // "r1" and "r2" are both instances of type "pp"
    static constexpr absl::string_view pp_instances[] = {"r1", "r2"};
    for (const auto& pp_inst : pp_instances) {
      const auto found_rr = qq.Find(pp_inst);
      ASSERT_NE(found_rr, qq.end());
      EXPECT_EQ(found_rr->first, pp_inst);
      {
        const SymbolTableNode& rr(found_rr->second);
        const SymbolInfo& rr_info(rr.Value());
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
    }

    {
      std::vector<absl::Status> resolve_diagnostics;
      symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
      EXPECT_TRUE(resolve_diagnostics.empty());
      for (const auto& pp_inst : pp_instances) {
        const auto found_rr = qq.Find(pp_inst);
        ASSERT_NE(found_rr, qq.end());
        EXPECT_EQ(found_rr->first, pp_inst);
        {
          const SymbolTableNode& rr(found_rr->second);
          const SymbolInfo& rr_info(rr.Value());
          EXPECT_TRUE(rr_info.local_references_to_bind.empty());
          // Verify that typeof(rr) successfully resolved to module pp.
          EXPECT_EQ(
              rr_info.declared_type.user_defined_type->Value().resolved_symbol,
              &pp);
        }
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

  const auto m_found = root_symbol.Find("m");
  ASSERT_NE(m_found, root_symbol.end());
  EXPECT_EQ(m_found->first, "m");
  const SymbolTableNode& m_node(m_found->second);
  const SymbolInfo& m_symbol(m_node.Value());
  EXPECT_EQ(m_symbol.type, SymbolType::kModule);
  EXPECT_EQ(m_symbol.file_origin, &src);
  EXPECT_EQ(m_symbol.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  const auto found_clk = m_node.Find("clk");
  ASSERT_NE(found_clk, m_node.end());
  const SymbolTableNode& clk_node(found_clk->second);
  EXPECT_EQ(*clk_node.Key(), "clk");
  EXPECT_EQ(clk_node.Value().type, SymbolType::kDataNetVariableInstance);
  EXPECT_EQ(clk_node.Value().declared_type.user_defined_type,
            nullptr);  // types are primitive

  const auto found_q = m_node.Find("q");
  ASSERT_NE(found_q, m_node.end());
  const SymbolTableNode& q_node(found_q->second);
  EXPECT_EQ(*q_node.Key(), "q");
  EXPECT_EQ(q_node.Value().type, SymbolType::kDataNetVariableInstance);
  EXPECT_EQ(q_node.Value().declared_type.user_defined_type,
            nullptr);  // types are primitive

  const auto rr_found = root_symbol.Find("rr");
  ASSERT_NE(rr_found, root_symbol.end());
  EXPECT_EQ(rr_found->first, "rr");
  const SymbolTableNode& rr_node(rr_found->second);
  const SymbolInfo& rr_symbol(rr_node.Value());

  // Inspect local references to wires "c" and "d".
  ASSERT_EQ(rr_symbol.local_references_to_bind.size(), 4);
  // Sort references because there are no ordering guarantees among them.
  const DependentReferences* c_ref = nullptr;
  const DependentReferences* d_ref = nullptr;
  for (const auto& ref : rr_symbol.local_references_to_bind) {
    const absl::string_view base_ref(ref.components->Value().identifier);
    if (base_ref == "c") c_ref = &ref;
    if (base_ref == "d") d_ref = &ref;
  }
  ASSERT_NE(c_ref, nullptr);
  ASSERT_NE(d_ref, nullptr);
  EXPECT_EQ(c_ref->LastLeaf()->Value().identifier, "c");
  EXPECT_EQ(c_ref->LastLeaf()->Value().resolved_symbol, nullptr);
  EXPECT_EQ(d_ref->LastLeaf()->Value().identifier, "d");
  EXPECT_EQ(d_ref->LastLeaf()->Value().resolved_symbol, nullptr);

  // Get the local symbol definitions for wires "c" and "d".
  const auto found_c = rr_node.Find("c");
  ASSERT_NE(found_c, rr_node.end());
  const SymbolTableNode& c_node(found_c->second);
  const auto found_d = rr_node.Find("d");
  ASSERT_NE(found_d, rr_node.end());
  const SymbolTableNode& d_node(found_d->second);

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_TRUE(resolve_diagnostics.empty());

    // Expect to resolve local references to wires c and d
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

  const auto m_found = root_symbol.Find("m");
  ASSERT_NE(m_found, root_symbol.end());
  EXPECT_EQ(m_found->first, "m");
  const SymbolTableNode& m_node(m_found->second);
  const SymbolInfo& m_symbol(m_node.Value());
  EXPECT_EQ(m_symbol.type, SymbolType::kModule);
  EXPECT_EQ(m_symbol.file_origin, &src);
  EXPECT_EQ(m_symbol.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  const auto found_clk = m_node.Find("clk");
  ASSERT_NE(found_clk, m_node.end());
  const SymbolTableNode& clk_node(found_clk->second);
  EXPECT_EQ(*clk_node.Key(), "clk");
  EXPECT_EQ(clk_node.Value().type, SymbolType::kDataNetVariableInstance);
  EXPECT_EQ(clk_node.Value().declared_type.user_defined_type,
            nullptr);  // types are primitive

  const auto found_q = m_node.Find("q");
  ASSERT_NE(found_q, m_node.end());
  const SymbolTableNode& q_node(found_q->second);
  EXPECT_EQ(*q_node.Key(), "q");
  EXPECT_EQ(q_node.Value().type, SymbolType::kDataNetVariableInstance);
  EXPECT_EQ(q_node.Value().declared_type.user_defined_type,
            nullptr);  // types are primitive

  const auto rr_found = root_symbol.Find("rr");
  ASSERT_NE(rr_found, root_symbol.end());
  EXPECT_EQ(rr_found->first, "rr");
  const SymbolTableNode& rr_node(rr_found->second);
  const SymbolInfo& rr_symbol(rr_node.Value());

  // Inspect local references to wires "c" and "d".
  ASSERT_EQ(rr_symbol.local_references_to_bind.size(), 4);
  // Sort references because there are no ordering guarantees among them.
  const DependentReferences* c_ref = nullptr;
  const DependentReferences* d_ref = nullptr;
  const DependentReferences* m_inst_ref = nullptr;
  for (const auto& ref : rr_symbol.local_references_to_bind) {
    const absl::string_view base_ref(ref.components->Value().identifier);
    if (base_ref == "c") c_ref = &ref;
    if (base_ref == "d") d_ref = &ref;
    if (base_ref == "m_inst") m_inst_ref = &ref;  // instance self-reference
  }
  ASSERT_NE(c_ref, nullptr);
  ASSERT_NE(d_ref, nullptr);
  ASSERT_NE(m_inst_ref, nullptr);
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
  EXPECT_EQ(clk_ref.Value().ref_type, ReferenceType::kObjectMember);
  EXPECT_EQ(clk_ref.Value().resolved_symbol, nullptr);  // not yet resolved

  const auto found_q_ref = port_refs.find("q");
  ASSERT_NE(found_q_ref, port_refs.end());
  const ReferenceComponentNode& q_ref(*found_q_ref->second);
  EXPECT_EQ(q_ref.Value().identifier, "q");
  EXPECT_EQ(q_ref.Value().ref_type, ReferenceType::kObjectMember);
  EXPECT_EQ(q_ref.Value().resolved_symbol, nullptr);  // not yet resolved

  // Get the local symbol definitions for wires "c" and "d".
  const auto found_c = rr_node.Find("c");
  ASSERT_NE(found_c, rr_node.end());
  const SymbolTableNode& c_node(found_c->second);
  const auto found_d = rr_node.Find("d");
  ASSERT_NE(found_d, rr_node.end());
  const SymbolTableNode& d_node(found_d->second);

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

TEST(BuildSymbolTableTest, OneGlobalIntParameter) {
  TestVerilogSourceFile src("foobar.sv", "localparam int mint = 1;\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  const auto found = root_symbol.Find("mint");
  ASSERT_NE(found, root_symbol.end());
  EXPECT_EQ(found->first, "mint");
  const SymbolInfo& param_symbol(found->second.Value());
  EXPECT_EQ(param_symbol.type, SymbolType::kParameter);
  EXPECT_EQ(param_symbol.file_origin, &src);
  ASSERT_NE(param_symbol.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(
      verible::StringSpanOfSymbol(*param_symbol.declared_type.syntax_origin),
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

  const auto found = root_symbol.Find("gun");
  ASSERT_NE(found, root_symbol.end());
  EXPECT_EQ(found->first, "gun");
  const SymbolInfo& param_symbol(found->second.Value());
  EXPECT_EQ(param_symbol.type, SymbolType::kParameter);
  EXPECT_EQ(param_symbol.file_origin, &src);
  ASSERT_NE(param_symbol.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(
      verible::StringSpanOfSymbol(*param_symbol.declared_type.syntax_origin),
      "foo_t");
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_EQ(resolve_diagnostics.size(), 1);
    const auto& err_status(resolve_diagnostics.front());
    EXPECT_EQ(err_status.code(), absl::StatusCode::kNotFound);
    EXPECT_THAT(err_status.message(),
                HasSubstr("Unable to resolve symbol \"foo_t\""));
    EXPECT_EQ(
        param_symbol.declared_type.user_defined_type->Value().resolved_symbol,
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

  const auto found_tea = root_symbol.Find("tea");
  ASSERT_NE(found_tea, root_symbol.end());
  EXPECT_EQ(found_tea->first, "tea");
  const SymbolInfo& tea(found_tea->second.Value());
  EXPECT_EQ(tea.type, SymbolType::kParameter);

  const auto found = root_symbol.Find("mint");
  ASSERT_NE(found, root_symbol.end());
  EXPECT_EQ(found->first, "mint");
  const SymbolTableNode& mint_node(found->second);
  const SymbolInfo& mint(mint_node.Value());
  EXPECT_EQ(mint.type, SymbolType::kParameter);
  EXPECT_EQ(mint.file_origin, &src);
  ASSERT_NE(mint.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(verible::StringSpanOfSymbol(*mint.declared_type.syntax_origin),
            "int");
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  // There should be one reference: "mint" (line 2)
  EXPECT_EQ(root_symbol.Value().local_references_to_bind.size(), 1);
  const DependentReferences& ref(
      root_symbol.Value().local_references_to_bind.front());
  const ReferenceComponent& ref_comp(ref.components->Value());
  EXPECT_TRUE(ref.components->is_leaf());
  EXPECT_EQ(ref_comp.identifier, "mint");
  EXPECT_EQ(ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(ref_comp.resolved_symbol,
            nullptr);  // have not tried to resolve yet

  {  // resolve symbols
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_TRUE(resolve_diagnostics.empty());
    EXPECT_EQ(ref_comp.resolved_symbol, &mint_node);  // resolved
  }
}

TEST(BuildSymbolTableTest, OneUnresolvedReferenceInExpression) {
  TestVerilogSourceFile src("foobar.sv", "localparam int mint = spice;\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  const auto found = root_symbol.Find("mint");
  ASSERT_NE(found, root_symbol.end());
  EXPECT_EQ(found->first, "mint");
  const SymbolTableNode& mint_node(found->second);
  const SymbolInfo& mint(mint_node.Value());
  EXPECT_EQ(mint.type, SymbolType::kParameter);
  EXPECT_EQ(mint.file_origin, &src);
  ASSERT_NE(mint.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(verible::StringSpanOfSymbol(*mint.declared_type.syntax_origin),
            "int");
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  // There should be one reference: "spice" (line 2)
  EXPECT_EQ(root_symbol.Value().local_references_to_bind.size(), 1);
  const DependentReferences& ref(
      root_symbol.Value().local_references_to_bind.front());
  const ReferenceComponent& ref_comp(ref.components->Value());
  EXPECT_TRUE(ref.components->is_leaf());
  EXPECT_EQ(ref_comp.identifier, "spice");
  EXPECT_EQ(ref_comp.ref_type, ReferenceType::kUnqualified);
  EXPECT_EQ(ref_comp.resolved_symbol,
            nullptr);  // have not tried to resolve yet

  {  // resolve symbols
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    ASSERT_EQ(resolve_diagnostics.size(), 1);
    const absl::Status& err_status(resolve_diagnostics.front());
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

  const auto found = root_symbol.Find("my_pkg");
  ASSERT_NE(found, root_symbol.end());
  EXPECT_EQ(found->first, "my_pkg");
  const SymbolInfo& package_symbol(found->second.Value());
  EXPECT_EQ(package_symbol.type, SymbolType::kPackage);
  EXPECT_EQ(package_symbol.file_origin, &src);
  EXPECT_EQ(package_symbol.declared_type.syntax_origin,
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

  const auto found_p = root_symbol.Find("p");
  ASSERT_NE(found_p, root_symbol.end());
  EXPECT_EQ(found_p->first, "p");
  const SymbolTableNode& p_node(found_p->second);
  const SymbolInfo& p_pkg(p_node.Value());
  EXPECT_EQ(p_pkg.type, SymbolType::kPackage);

  ASSERT_EQ(p_pkg.local_references_to_bind.size(), 1);
  const ReferenceComponent& mint_ref(
      p_pkg.local_references_to_bind.front().components->Value());
  EXPECT_EQ(mint_ref.identifier, "mint");
  EXPECT_EQ(mint_ref.resolved_symbol, nullptr);  // not yet resolved

  const auto found_tea = p_node.Find("tea");  // p::tea
  ASSERT_NE(found_tea, p_node.end());
  EXPECT_EQ(found_tea->first, "tea");
  const SymbolInfo& tea(found_tea->second.Value());
  EXPECT_EQ(tea.type, SymbolType::kParameter);

  const auto found = root_symbol.Find("mint");
  ASSERT_NE(found, root_symbol.end());
  EXPECT_EQ(found->first, "mint");
  const SymbolTableNode& mint_node(found->second);
  const SymbolInfo& mint(mint_node.Value());
  EXPECT_EQ(mint.type, SymbolType::kParameter);
  EXPECT_EQ(mint.file_origin, &src);
  ASSERT_NE(mint.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(verible::StringSpanOfSymbol(*mint.declared_type.syntax_origin),
            "int");
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  {  // resolve symbols
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_TRUE(resolve_diagnostics.empty());
    EXPECT_EQ(mint_ref.resolved_symbol, &mint_node);  // resolved "mint"
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

  const auto found_p = root_symbol.Find("p");
  ASSERT_NE(found_p, root_symbol.end());
  EXPECT_EQ(found_p->first, "p");
  const SymbolTableNode& p_node(found_p->second);
  const SymbolInfo& p_pkg(p_node.Value());
  EXPECT_EQ(p_pkg.type, SymbolType::kPackage);

  ASSERT_EQ(root_symbol.Value().local_references_to_bind.size(), 1);
  // p_mint_ref is the reference chain for "p::mint".
  const DependentReferences& p_mint_ref(
      root_symbol.Value().local_references_to_bind.front());
  const ReferenceComponent& p_ref(p_mint_ref.components->Value());
  EXPECT_EQ(p_ref.identifier, "p");
  EXPECT_EQ(p_ref.resolved_symbol, nullptr);  // not yet resolved
  const ReferenceComponent& mint_ref(p_mint_ref.LastLeaf()->Value());
  EXPECT_EQ(mint_ref.identifier, "mint");
  EXPECT_EQ(mint_ref.resolved_symbol, nullptr);  // not yet resolved

  const auto found_tea = root_symbol.Find("tea");  // tea
  ASSERT_NE(found_tea, root_symbol.end());
  EXPECT_EQ(found_tea->first, "tea");
  const SymbolInfo& tea(found_tea->second.Value());
  EXPECT_EQ(tea.type, SymbolType::kParameter);

  const auto found = p_node.Find("mint");  // p::mint
  ASSERT_NE(found, p_node.end());
  EXPECT_EQ(found->first, "mint");
  const SymbolTableNode& mint_node(found->second);
  const SymbolInfo& mint(mint_node.Value());
  EXPECT_EQ(mint.type, SymbolType::kParameter);
  EXPECT_EQ(mint.file_origin, &src);
  ASSERT_NE(mint.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(verible::StringSpanOfSymbol(*mint.declared_type.syntax_origin),
            "int");
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  {  // resolve symbols
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_TRUE(resolve_diagnostics.empty());
    EXPECT_EQ(p_ref.resolved_symbol, &p_node);        // resolved "p"
    EXPECT_EQ(mint_ref.resolved_symbol, &mint_node);  // resolved "p::mint"
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

  const auto found_p = root_symbol.Find("p");
  ASSERT_NE(found_p, root_symbol.end());
  EXPECT_EQ(found_p->first, "p");
  const SymbolTableNode& p_node(found_p->second);
  const SymbolInfo& p_pkg(p_node.Value());
  EXPECT_EQ(p_pkg.type, SymbolType::kPackage);

  ASSERT_EQ(root_symbol.Value().local_references_to_bind.size(), 1);
  // p_mint_ref is the reference chain for "p::mint".
  const DependentReferences& p_mint_ref(
      root_symbol.Value().local_references_to_bind.front());
  const ReferenceComponent& p_ref(p_mint_ref.components->Value());
  EXPECT_EQ(p_ref.identifier, "p");
  EXPECT_EQ(p_ref.resolved_symbol, nullptr);  // not yet resolved
  const ReferenceComponent& zzz_ref(p_mint_ref.LastLeaf()->Value());
  EXPECT_EQ(zzz_ref.identifier, "zzz");
  EXPECT_EQ(zzz_ref.resolved_symbol, nullptr);  // not yet resolved

  const auto found_tea = root_symbol.Find("tea");  // tea
  ASSERT_NE(found_tea, root_symbol.end());
  EXPECT_EQ(found_tea->first, "tea");
  const SymbolInfo& tea(found_tea->second.Value());
  EXPECT_EQ(tea.type, SymbolType::kParameter);

  const auto found = p_node.Find("mint");  // p::mint
  ASSERT_NE(found, p_node.end());
  EXPECT_EQ(found->first, "mint");
  const SymbolTableNode& mint_node(found->second);
  const SymbolInfo& mint(mint_node.Value());
  EXPECT_EQ(mint.type, SymbolType::kParameter);
  EXPECT_EQ(mint.file_origin, &src);
  ASSERT_NE(mint.declared_type.syntax_origin, nullptr);
  EXPECT_EQ(verible::StringSpanOfSymbol(*mint.declared_type.syntax_origin),
            "int");
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  // resolving twice should not change results
  for (int i = 0; i < 2; ++i) {  // resolve symbols
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);
    EXPECT_EQ(resolve_diagnostics.size(), 1);
    EXPECT_EQ(p_ref.resolved_symbol, &p_node);    // resolved "p"
    EXPECT_EQ(zzz_ref.resolved_symbol, nullptr);  // unresolved "p::zzz"
  }
}

TEST(BuildSymbolTableTest, ClassDeclarationSingle) {
  TestVerilogSourceFile src("foobar.sv", "class ccc;\nendclass\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto build_diagnostics = BuildSymbolTable(src, &symbol_table);

  const auto found = root_symbol.Find("ccc");
  ASSERT_NE(found, root_symbol.end());
  EXPECT_EQ(found->first, "ccc");
  const SymbolInfo& class_symbol(found->second.Value());
  EXPECT_EQ(class_symbol.type, SymbolType::kClass);
  EXPECT_EQ(class_symbol.file_origin, &src);
  EXPECT_EQ(class_symbol.declared_type.syntax_origin,
            nullptr);  // there is no module meta-type
  EXPECT_TRUE(build_diagnostics.empty()) << "Unexpected diagnostic:\n"
                                         << build_diagnostics.front().message();

  {
    std::vector<absl::Status> resolve_diagnostics;
    symbol_table.Resolve(&resolve_diagnostics);  // nothing to resolve
    EXPECT_TRUE(resolve_diagnostics.empty());
  }
}

// TODO:
// Test out-of-order declarations/references (modules).
// expressions in ranges of dimensions.
// parameters package/module/class.
// Test unresolved symbols.

}  // namespace
}  // namespace verilog
