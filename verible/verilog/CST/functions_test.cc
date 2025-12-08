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

#include "verible/verilog/CST/functions.h"

#include <memory>
#include <vector>

#include "gtest/gtest.h"
#include "verible/common/analysis/syntax-tree-search-test-utils.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info-test-util.h"
#include "verible/common/text/tree-utils.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/identifier.h"
#include "verible/verilog/CST/match-test-utils.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/verilog-analyzer.h"
#include "verible/verilog/parser/verilog-token-enum.h"

#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace {

using verible::CheckSymbolAsLeaf;
using verible::SymbolCastToNode;
using verible::SyntaxTreeSearchTestCase;
using verible::TextStructureView;
using verible::TreeSearchMatch;

TEST(FindAllFunctionDeclarationsTest, Various) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"class foo; endclass"},
      {"task foo; endtask"},
      {"module foo; endmodule"},
      {{kTag, "function foo(); endfunction"}},
      {// multiple function declarations
       {kTag, "function foo(); endfunction"},
       "\n",
       {kTag, "function foo2(); endfunction"},
       "\n"},
      {// package function method declaration
       "package bar;\n",
       {kTag, "function foo(); endfunction"},
       "\nendpackage\n"},
      {// class function method declaration
       "class bar;\n",
       {kTag, "function foo(); endfunction"},
       "\nendclass\n"},
      {// function declaration inside module
       "module bar;\n",
       {kTag, "function foo(); endfunction"},
       "\nendmodule\n"},
      {// forward declaration is not a full function declaration
       "class bar;\n",
       "extern function foo();\n"
       "endclass\n"},
      {// pure virtual is not a full function declaration
       "class bar;\n",
       "pure virtual function foo();\n"
       "endclass\n"},
      {// virtual is a full function declaration
       "class bar;\n",
       {kTag,
        "virtual function foo();\n"
        "endfunction"},
       "\n"
       "endclass\n"},
      {// function declaration inside cross_body_item
       "module cover_that;\n"
       "covergroup settings;\n"
       "  _name : cross dbi, mask {\n"
       "    ",
       {kTag,
        "function int foo(int bar);\n"  // function declaration
        "      return bar;\n"
        "    endfunction"},
       "\n"
       "  }\n"
       "endgroup\n"
       "endmodule"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          return FindAllFunctionDeclarations(*ABSL_DIE_IF_NULL(root));
        });
  }
}

TEST(FindAllFunctionPrototypesTest, Various) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"class foo; endclass"},
      {"task foo; endtask"},
      {"module foo; endmodule"},
      {"function foo(); endfunction"},
      {// package function method declaration
       "package bar;\n",
       "function foo(); endfunction\n"
       "endpackage\n"},
      {// class function method declaration (not a prototype)
       "class bar;\n",
       "function foo(); endfunction\n"
       "endclass\n"},
      {// function declaration inside module
       "module bar;\n"
       "function foo(); endfunction\n"
       "endmodule\n"},
      {// forward declaration is a prototype
       "class bar;\n",
       "extern ",
       {kTag, "function foo();"},
       "\n"
       "endclass\n"},
      {// pure virtual is a prototype
       "class bar;\n",
       "pure virtual ",
       {kTag, "function foo();"},
       "\n"
       "endclass\n"},
      {// virtual declaration is not a prototype
       "class bar;\n",
       "virtual function foo();\n"
       "endfunction\n"
       "endclass\n"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          return FindAllFunctionPrototypes(*ABSL_DIE_IF_NULL(root));
        });
  }
  // Protype headers map to the same range as the enclosing prototype.
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto protos(FindAllFunctionPrototypes(*ABSL_DIE_IF_NULL(root)));
          std::vector<TreeSearchMatch> headers;
          headers.reserve(protos.size());
          for (const auto &proto : protos) {
            headers.push_back(TreeSearchMatch{
                GetFunctionPrototypeHeader(*proto.match), /* no context */});
          }
          return headers;
        });
  }
}

TEST(FunctionPrototypesReturnTypesTest, Various) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {// forward declaration is a prototype, implicit return type
       "class bar;\n"
       "extern function foo();\n"
       "endclass\n"},
      {// forward declaration is a prototype, explicit return type
       "class bar;\n"
       "extern function ",
       {kTag, "bar#(int)"},
       " foo();\n"
       "endclass\n"},
      {// pure virtual is a prototype, implicit return type
       "class bar;\n"
       "pure virtual function foo();\n"
       "endclass\n"},
      {// pure virtual is a prototype, explicit return type
       "class bar;\n"
       "pure virtual function ",
       {kTag, "p_pkg::baz_t"},
       " foo();\n"
       "endclass\n"},
  };
  // Protype headers map to the same range as the enclosing prototype.
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto protos(FindAllFunctionPrototypes(*ABSL_DIE_IF_NULL(root)));
          std::vector<TreeSearchMatch> returns;
          for (const auto &proto : protos) {
            const auto *return_type = GetFunctionHeaderReturnType(
                *GetFunctionPrototypeHeader(*proto.match));
            if (return_type == nullptr) continue;
            if (verible::StringSpanOfSymbol(*return_type).empty()) continue;
            returns.push_back(TreeSearchMatch{return_type, /* no context */});
          }
          return returns;
        });
  }
}

TEST(FunctionPrototypesIdsTest, Various) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {// forward declaration is a prototype, implicit return type
       "class bar;\n"
       "extern function ",
       {kTag, "moo"},
       "();\n"
       "endclass\n"},
      {// forward declaration is a prototype, explicit return type
       "class bar;\n"
       "extern function bar#(int) ",
       {kTag, "gooo"},
       "();\n"
       "endclass\n"},
      {// pure virtual is a prototype, implicit return type
       "class bar;\n"
       "pure virtual function ",
       {kTag, "weeee"},
       "();\n"
       "endclass\n"},
      {// pure virtual is a prototype, explicit return type
       "class bar;\n",
       "pure virtual function p_pkg::baz_t ",
       {kTag, "h456"},
       "();\n"
       "endclass\n"},
  };
  // Protype headers map to the same range as the enclosing prototype.
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto protos(FindAllFunctionPrototypes(*ABSL_DIE_IF_NULL(root)));
          std::vector<TreeSearchMatch> ids;
          for (const auto &proto : protos) {
            const auto *id =
                GetFunctionHeaderId(*GetFunctionPrototypeHeader(*proto.match));
            if (id == nullptr) continue;
            if (verible::StringSpanOfSymbol(*id).empty()) continue;
            ids.push_back(TreeSearchMatch{id, /* no context */});
          }
          return ids;
        });
  }
}

TEST(FindAllFunctionHeadersTest, Various) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"class foo; endclass"},
      {"task foo; endtask"},
      {"module foo; endmodule"},
      {{kTag, "function foo();"}, " endfunction"},
      {// multiple function declarations
       {kTag, "function foo();"},
       " endfunction\n",
       {kTag, "function foo2();"},
       " endfunction\n"},
      {// package function method declaration
       "package bar;\n",
       {kTag, "function foo();"},
       " endfunction"
       "\nendpackage\n"},
      {// class function method declaration
       "class bar;\n",
       {kTag, "function foo();"},
       " endfunction"
       "\nendclass\n"},
      {// function declaration inside module
       "module bar;\n",
       {kTag, "function foo();"},
       " endfunction"
       "\nendmodule\n"},
      {// forward declaration
       "class bar;\n",
       "extern ",
       {kTag, "function foo();"},
       "\n"
       "endclass\n"},
      {// pure virtual
       "class bar;\n",
       "pure virtual ",
       {kTag, "function foo();"},
       "\n"
       "endclass\n"},
      {// virtual
       "class bar;\n",
       {kTag, "virtual function foo();"},
       "\n"
       "endfunction\n"
       "endclass\n"},
      {// function declaration inside cross_body_item
       "module cover_that;\n"
       "covergroup settings;\n"
       "  _name : cross dbi, mask {\n"
       "    ",
       {kTag, "function int foo(int bar);"},
       "\n"
       "      return bar;\n"
       "    endfunction\n"
       "  }\n"
       "endgroup\n"
       "endmodule"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          return FindAllFunctionHeaders(*ABSL_DIE_IF_NULL(root));
        });
  }
}

TEST(GetFunctionHeaderTest, DeclarationHeader) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {{kTag, "function foo();"}, " endfunction"},
      {"class c; ", {kTag, "function foo();"}, " endfunction endclass"},
      {"module m; ", {kTag, "function foo();"}, " endfunction endmodule"},
      {"package p; ", {kTag, "function foo();"}, " endfunction endpackage"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          // Root node is a description list, not a function.
          const auto function_declarations = FindAllFunctionDeclarations(*root);
          std::vector<TreeSearchMatch> headers;
          for (const auto &decl : function_declarations) {
            const auto &function_node = SymbolCastToNode(*decl.match);
            headers.push_back(TreeSearchMatch{GetFunctionHeader(function_node),
                                              /* no context */});
          }
          return headers;
        });
  }
}

TEST(GetFunctionLifetimeTest, NoLifetimeDeclared) {
  VerilogAnalyzer analyzer("function foo(); endfunction", "");
  ASSERT_OK(analyzer.Analyze());
  // Root node is a description list, not a function.
  const auto &root = analyzer.Data().SyntaxTree();
  const auto function_declarations = FindAllFunctionDeclarations(*root);
  ASSERT_EQ(function_declarations.size(), 1);
  const auto &function_node =
      SymbolCastToNode(*function_declarations.front().match);
  const auto *lifetime = GetFunctionLifetime(function_node);
  EXPECT_EQ(lifetime, nullptr);
}

TEST(GetFunctionLifetimeTest, StaticLifetimeDeclared) {
  VerilogAnalyzer analyzer("function static foo(); endfunction", "");
  ASSERT_OK(analyzer.Analyze());
  // Root node is a description list, not a function.
  const auto &root = analyzer.Data().SyntaxTree();
  const auto function_declarations = FindAllFunctionDeclarations(*root);
  ASSERT_EQ(function_declarations.size(), 1);
  const auto &function_node =
      SymbolCastToNode(*function_declarations.front().match);
  const auto *lifetime = GetFunctionLifetime(function_node);
  CheckSymbolAsLeaf(*ABSL_DIE_IF_NULL(lifetime), TK_static);
  // TODO(b/151371397): verify substring range
}

TEST(GetFunctionLifetimeTest, AutomaticLifetimeDeclared) {
  VerilogAnalyzer analyzer("function automatic foo(); endfunction", "");
  ASSERT_OK(analyzer.Analyze());
  // Root node is a description list, not a function.e
  const auto &root = analyzer.Data().SyntaxTree();
  const auto function_declarations = FindAllFunctionDeclarations(*root);
  ASSERT_EQ(function_declarations.size(), 1);
  const auto &function_node =
      SymbolCastToNode(*function_declarations.front().match);
  const auto *lifetime = GetFunctionLifetime(function_node);
  CheckSymbolAsLeaf(*ABSL_DIE_IF_NULL(lifetime), TK_automatic);
  // TODO(b/151371397): verify substring range
}

TEST(GetFunctionIdTest, UnqualifiedIds) {
  constexpr int kTag = 1;  // don't care
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {"function ", {kTag, "foo"}, "(); endfunction"},
      {"function automatic ", {kTag, "bar"}, "(); endfunction"},
      {"function static ", {kTag, "baz"}, "(); endfunction"},
      {"package p; function ", {kTag, "foo"}, "(); endfunction endpackage"},
      {"class c; function ", {kTag, "zoo"}, "(); endfunction endclass"},
      {"function ", {kTag, "myclass"}, "::", {kTag, "foo"}, "(); endfunction"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto function_declarations = FindAllFunctionDeclarations(*root);
          std::vector<TreeSearchMatch> got_ids;
          for (const auto &decl : function_declarations) {
            const auto &function_node = SymbolCastToNode(*decl.match);
            const auto *function_id = GetFunctionId(function_node);
            for (const auto &id : FindAllUnqualifiedIds(*function_id)) {
              const verible::SyntaxTreeLeaf *base = GetIdentifier(*id.match);
              got_ids.push_back(TreeSearchMatch{base, /* empty context */});
            }
          }
          return got_ids;
        });
  }
}

TEST(GetFunctionIdTest, QualifiedIds) {
  constexpr int kTag = 1;  // don't care
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {"function foo(); endfunction"},
      {"function ", {kTag, "myclass::foo"}, "(); endfunction"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto function_declarations = FindAllFunctionDeclarations(*root);
          std::vector<TreeSearchMatch> got_ids;
          for (const auto &decl : function_declarations) {
            const auto &function_node = SymbolCastToNode(*decl.match);
            const auto *function_id = GetFunctionId(function_node);
            for (const auto &id : FindAllQualifiedIds(*function_id)) {
              got_ids.push_back(id);
            }
          }
          return got_ids;
        });
  }
}

struct SubtreeTestData {
  NodeEnum expected_construct;
  verible::TokenInfoTestData token_data;
};

TEST(GetFunctionReturnTypeTest, VariousReturnTypes) {
  constexpr int kTag = 1;  // don't care
  const SyntaxTreeSearchTestCase kTestCases[] = {
      // implicit/missing return types
      {"function f;endfunction\n"},
      {"package p;\nfunction f;\nendfunction\nendpackage\n"},
      {"class c;\nfunction f;\nendfunction\nendclass\n"},
      {"module m;\nfunction f;\nendfunction\nendmodule\n"},
      // explicit return types
      {"function ", {kTag, "void"}, " f;endfunction\n"},
      {"package p;\nfunction ",
       {kTag, "int"},
       " f;\nendfunction\nendpackage\n"},
      {"class c;\nfunction ",
       {kTag, "foo_pkg::bar_t"},
       " f;\nendfunction\nendclass\n"},
      {"module m;\nfunction ",
       {kTag, "foo#(bar)"},
       " f;\nendfunction\nendmodule\n"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();

          const auto decls = FindAllFunctionDeclarations(*root);
          std::vector<TreeSearchMatch> returns;
          for (const auto &decl : decls) {
            const auto &statement = *decl.match;
            const auto *return_type = GetFunctionReturnType(statement);
            // Expect a type node, even when type is implicit or empty.
            if (return_type == nullptr) continue;
            const auto return_type_span =
                verible::StringSpanOfSymbol(*return_type);
            if (!return_type_span.empty()) {
              returns.push_back(TreeSearchMatch{return_type, /* no context */});
            }
          }
          return returns;
        });
  }
}

TEST(GetFunctionFormalPortsGroupTest, MixedFormalPorts) {
  constexpr int kTag = 1;  // don't care
  const SyntaxTreeSearchTestCase kTestCases[] = {
      // no ports
      {"function f;endfunction\n"},
      {"package p;\nfunction f;\nendfunction\nendpackage\n"},
      {"class c;\nfunction f;\nendfunction\nendclass\n"},
      {"module m;\nfunction f;\nendfunction\nendmodule\n"},
      // with ports
      {"function f", {kTag, "()"}, ";endfunction\n"},
      {"package p;\nfunction f",
       {kTag, "(string s)"},
       ";\nendfunction\nendpackage\n"},
      {"class c;\nfunction f",
       {kTag, "(int i, string s)"},
       ";\nendfunction\nendclass\n"},
      {"module m;\nfunction f",
       {kTag, "(input logic foo, bar)"},
       ";\nendfunction\nendmodule\n"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();

          const auto decls = FindAllFunctionDeclarations(*root);
          std::vector<TreeSearchMatch> ports;
          for (const auto &decl : decls) {
            const auto &statement = *decl.match;
            const auto *port_formals = GetFunctionFormalPortsGroup(statement);
            if (port_formals == nullptr) continue;
            const auto port_formals_span =
                verible::StringSpanOfSymbol(*port_formals);
            if (port_formals_span.empty()) continue;
            ports.push_back(TreeSearchMatch{port_formals, /* no context */});
          }
          return ports;
        });
  }
}

TEST(GetFunctionHeaderTest, GetFunctionName) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m(); endmodule: m"},
      {"function ",
       {kTag, "foo"},
       "();\n endfunction\n function ",
       {kTag, "bar"},
       "()\n; endfunction"},
      {"function int ",
       {kTag, "foo"},
       "();\n endfunction\n function ",
       {kTag, "bar"},
       "()\n; endfunction"},
      {"function int ",
       {kTag, "foo"},
       "(int a, bit x);\nreturn a;\n endfunction\n function ",
       {kTag, "bar"},
       "()\n; endfunction"},

      {"module my_module;\nfunction int ",
       {kTag, "inner_function"},
       "(int my_args);\nreturn my_args;\nendfunction\nendmodule"},
      {"class function_class;\nfunction int ",
       {kTag, "my_function"},
       "(input int a, b, output int c);\nc = a + b;\nreturn "
       "c;\nendfunction\nendclass"},
      {"package my_pkg;\nfunction automatic int ",
       {kTag, "my_function"},
       "(input int a, b, output int c);\nc = a + b;\nreturn "
       "c;\nendfunction\nendpackage"},
      {"class m;\n virtual function int ",
       {kTag, "my_fun"},
       "();\n return 10;\n endfunction\n  endclass"},
      {"class m;\n static function int ",
       {kTag, "my_fun"},
       "();\n return 10;\n endfunction\n  endclass"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto decls =
              FindAllFunctionDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> types;
          for (const auto &decl : decls) {
            const auto *type = GetFunctionName(*decl.match);
            types.push_back(TreeSearchMatch{type, {/* ignored context */}});
          }
          return types;
        });
  }
}

TEST(GetFunctionHeaderTest, GetFunctionClassCallName) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m(); endmodule: m"},
      {"module m();\n initial $display(my_class.",
       {kTag, "function_name"},
       "());\nendmodule"},
      {"module m();\n initial "
       "$display(pkg::my_class.",
       {kTag, "function_name"},
       "());\nendmodule"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto calls =
              FindAllFunctionOrTaskCallsExtension(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> names;
          for (const auto &Call : calls) {
            const auto *name =
                GetFunctionCallNameFromCallExtension(*Call.match);
            names.emplace_back(TreeSearchMatch{name, {/* ignored context */}});
          }
          return names;
        });
  }
}

TEST(GetFunctionBlockStatement, GetFunctionBody) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m(); endmodule: m"},
      {"function int foo(int a, bit x);\n",
       {kTag, "return a;"},
       " endfunction\n"},
      {"module my_module;\nfunction int inner_function(int my_args);\n",
       {kTag, "return my_args;"},
       "\nendfunction\nendmodule"},
      {"class function_class;\nfunction int my_function(input int a, b, output "
       "int c);\n",
       {kTag, "c = a + b;\nreturn c;"},
       "\nendfunction\nendclass"},
      {"package my_pkg;\nfunction automatic int my_function(input int a, b, "
       "output int c);\n",
       {kTag, "c = a + b;\nreturn c;"},
       "\nendfunction\nendpackage"},
      {"class m;\n virtual function int my_fun();\n ",
       {kTag, "return 10;"},
       "\n endfunction\n  endclass"},
      {"class m;\n static function int my_fun();\n ",
       {kTag, "return 10;"},
       "\n endfunction\n  endclass"},
      {"function int f;\n", {kTag, "return 1;"}, "\nendfunction"},
      {"class s;\nfunction int f;",
       {kTag, "return 1;"},
       "\nendfunction\nendclass"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto decls =
              FindAllFunctionDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> functions_body;
          for (const auto &decl : decls) {
            const auto &body = GetFunctionBlockStatementList(*decl.match);
            functions_body.push_back(
                TreeSearchMatch{body, {/* ignored context */}});
          }
          return functions_body;
        });
  }
}

TEST(FunctionCallTest, GetFunctionCallName) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m;\nendmodule"},
      {"module m;\ninitial begin\n", {kTag, "f1"}, "();\nend\nendmodule"},
      {"module m;\ninitial begin\n", {kTag, "pkg::f1"}, "();\nend\nendmodule"},
      {"module m;\ninitial begin\n",
       {kTag, "class_name#(1)::f1"},
       "(a, b, c);\nend\nendmodule"},
      {"r=this();"},
  };

  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &calls =
              FindAllFunctionOrTaskCalls(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> identifiers;
          for (const auto &call : calls) {
            const auto *identifier =
                GetIdentifiersFromFunctionCall(*call.match);
            if (identifier == nullptr) {
              continue;
            }
            identifiers.emplace_back(
                TreeSearchMatch{identifier, {/* ignored context */}});
          }
          return identifiers;
        });
  }
}  // namespace

TEST(FunctionCallTest, GetFunctionCallArguments) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {"module m;\ninitial foo", {kTag, "()"}, ";\nendmodule"},
      {"module m;\ninitial foo", {kTag, "(a, b, c)"}, ";\nendmodule"},
      {"class c;\nfunction foo();\nendfunction\nfunction "
       "bar();\nfoo",
       {kTag, "()"},
       ";\nendfunction\ntask tsk(x, y);\nendtask\ntask "
       "task_2();\ntsk",
       {kTag, "(a, b)"},
       ";\nendtask\nendclass"},

      // moved from the extension argument finder since the tree structure is
      // changed and this method always finds the first calls
      {"module m();\n initial $display(my_class.function_name",
       {kTag, "(x, y)"},
       ");\nendmodule"},
      {"module m();\n initial "
       "$display(pkg::my_class.function_name",
       {kTag, "()"},
       ");\nendmodule"},
      {"module m(); endmodule: m"},
      {"module m();\n initial $display(class_factory",
       {kTag, "(x, y)"},
       ".function_name());\nendmodule"},
      {"module m();\n initial "
       "$display(pkg::class_factory",
       {kTag, "()"},
       ".function_name(foo, bar));\nendmodule"},

  };

  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &instances =
              FindAllFunctionOrTaskCalls(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> paren_groups;
          for (const auto &decl : instances) {
            const auto *paren_group = GetParenGroupFromCall(*decl.match);
            paren_groups.emplace_back(
                TreeSearchMatch{paren_group, {/* ignored context */}});
          }
          return paren_groups;
        });
  }
}

TEST(FunctionCallTest, GetFunctionCallExtensionArguments) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m(); endmodule: m"},
      {"module m();\n initial $display(class_factory().function_name",
       {kTag, "(x, y)"},
       ");\nendmodule"},
      {"module m();\n initial "
       "$display(pkg::class_factory().function_name",
       {kTag, "()"},
       ");\nendmodule"},
  };

  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &instances =
              FindAllFunctionOrTaskCallsExtension(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> paren_groups;
          for (const auto &decl : instances) {
            const auto *paren_group =
                GetParenGroupFromCallExtension(*decl.match);
            paren_groups.emplace_back(
                TreeSearchMatch{paren_group, {/* ignored context */}});
          }
          return paren_groups;
        });
  }
}

TEST(FunctionCallTest, GetConstructorNewKeyword) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"class c; endclass: c"},
      {"class c;\n"
       "function int foo();\n"
       "endfunction\n"
       "endclass\n"},
      {"class c;\n"
       "extern function int foo();\n"
       "endclass\n"},
      {"class c;\n"
       "extern function ",
       {kTag, "new"},
       ";\n"
       "endclass\n"},
      {"class c;\n"
       "extern function ",
       {kTag, "new"},
       "();\n"
       "endclass\n"},
      {"class c;\n"
       "extern function ",
       {kTag, "new"},
       "(string name);\n"
       "endclass\n"},
  };

  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &ctors =
              FindAllConstructorPrototypes(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> new_tokens;
          for (const auto &decl : ctors) {
            const auto *paren_group =
                GetConstructorPrototypeNewKeyword(*decl.match);
            new_tokens.emplace_back(
                TreeSearchMatch{paren_group, {/* ignored context */}});
          }
          return new_tokens;
        });
  }
}

}  // namespace
}  // namespace verilog
