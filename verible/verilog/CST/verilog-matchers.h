// Copyright 2017-2023 The Verible Authors.
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

#ifndef VERIBLE_VERILOG_CST_VERILOG_MATCHERS_H_
#define VERIBLE_VERILOG_CST_VERILOG_MATCHERS_H_

#include "verible/common/analysis/matcher/matcher-builders.h"
#include "verible/common/text/symbol.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {

// These are locally defined macros (undef'd at end of header) to enable
// concise description of Path Matchers.

#define N(tag) verible::NodeTag(NodeEnum::tag)
#define L(tag) verible::LeafTag(tag)

// NodeMatcher matches a syntax tree node with a specific tag.
// This is a template type alias.
template <NodeEnum NodeTag>
using NodeMatcher =
    verible::matcher::TagMatchBuilder<verible::SymbolKind::kNode, NodeEnum,
                                      NodeTag>;

// LeafMatcher matches a syntax tree leaf with a specific tag.
// This is a template type alias.
template <int LeafTag>
using LeafMatcher =
    verible::matcher::TagMatchBuilder<verible::SymbolKind::kLeaf, int, LeafTag>;

// Declaration of Leaf Matchers
// TODO(fangism): auto-generate leaf matches for named enum constants.

// Matches against system task or function identifiers, which are functions
// or tasks that begin with a $ character
//
// For instance, matches:
//    $psprintf(...);
//    $foo_bar(...);
//
inline constexpr LeafMatcher<SystemTFIdentifier> SystemTFIdentifierLeaf;

// Matches against macro call identifiers, which are identifiers
// begin with a ` character.
//
// For instance, matches '`MACRO' in:
//    `MACRO()
//    `MACRO();
//
inline constexpr LeafMatcher<MacroCallId> MacroCallIdLeaf;

// Matches against symbol identifiers.
//
// For instance, matches 'foo' in:
//    wire foo;
//    parameter foo = 32'hDEADBEEF;
//
inline constexpr LeafMatcher<SymbolIdentifier> SymbolIdentifierLeaf;

// Declaration of Node Matchers

// Declare every syntax tree node matcher.
// NodekFoo is a matcher that matches against syntax tree nodes tagged kFoo.
#define CONSIDER(tag) inline constexpr NodeMatcher<NodeEnum::tag> Node##tag;
#include "verible/verilog/CST/verilog_nonterminals_foreach.inc"  // IWYU pragma: keep
#undef CONSIDER

// Declare every syntax tree single-node path matcher.
// PathkFoo is a PathMatcher that matches against a single node tagged kFoo.
#define CONSIDER(tag)                                                \
  inline constexpr verible::matcher::PathMatchBuilder<1> Path##tag = \
      verible::matcher::MakePathMatcher(N(tag));
#include "verible/verilog/CST/verilog_nonterminals_foreach.inc"  // IWYU pragma: keep
#undef CONSIDER

// These matchers match on a specific type of AST Node

// NodekGenerateBlock matches against generate blocks
//
// For instance, matches:
//   generate
//     if (TypeIsPosedge) begin : gen_posedge
//       always @(posedge clk) foo <= bar;
//     end
//   endgenerate
//

// NodekVoidcast matches against voidcasts
//
// For instance, matches:
//   void'(foo());
//

// NodekExpression matches against expressions
//
// For instance, matches:
//   x = y; // assignment expression.
//   y = foo() // assignment expression and function call expression
//

// NodekActualParameterList matches against the parameter list provided
// to a parameterized module during instantiation.
//
// For instance, matches "#(1, 2, 3) in
//   foo #(1, 2, 3) bar;
//

// NodekGateInstance matches against the gate list for instantiated modules.
//
// For instance, matches "bar(port1, port2)" in
//   foo bar(port1, port2);
//

// NodekAlwaysStatement matches against an always statement block.
//
// For instance, matches "always @* begin ... end" in
//   module foo;
//   always @* begin
//     c = d;
//   end
//   endmodule

// Declarations of Traversal Matchers
//
// These matchers allow traversal between specific nodes

// Matches against the expression contained within a voidcast.
//
// For instance,
//   VoidcastNode(VoidcastHasExpression());
// matches all of
//   void'(expression());
//
inline constexpr auto VoidcastHasExpression =
    verible::matcher::MakePathMatcher(N(kParenGroup), N(kExpression));

// Matches against a top level function call contained within an expression.
//
// For instance,
//   ExpressionNode(ExpressionHasFunctionCall());
// matches "foo()" in
//   x = foo();
//
inline constexpr auto ExpressionHasFunctionCall =
    verible::matcher::MakePathMatcher(N(kFunctionCall), N(kReferenceCallBase),
                                      N(kParenGroup));

inline constexpr auto ExpressionHasFunctionCallNode =
    verible::matcher::MakePathMatcher(N(kFunctionCall));
inline constexpr auto FunctionCallHasHierarchyExtension =
    verible::matcher::MakePathMatcher(N(kReferenceCallBase), N(kReference),
                                      N(kHierarchyExtension));
inline constexpr auto FunctionCallHasParenGroup =
    verible::matcher::MakePathMatcher(N(kReferenceCallBase), N(kParenGroup));

// Matches a randomize call extension, or a call to an object's randomize
// method contained within an expression.
//
// For instance,
//   ExpressionNode(ExpressionHasRandomizeCallExtension());
// matches
//   result = obj.randomize();
//
inline constexpr auto NonCallHasRandomizeCallExtension =
    verible::matcher::MakePathMatcher(N(kFunctionCall), N(kReference),
                                      N(kRandomizeMethodCallExtension));
inline constexpr auto CallHasRandomizeCallExtension =
    verible::matcher::MakePathMatcher(N(kFunctionCall), N(kReferenceCallBase),
                                      N(kRandomizeMethodCallExtension));
inline constexpr auto ExpressionHasRandomizeCallExtension =
    verible::matcher::MakePathMatcher(N(kFunctionCall), N(kReferenceCallBase),
                                      N(kReference),
                                      N(kRandomizeMethodCallExtension));

// Matches a randomize function call contained within an expression.
//
// For instance,
//   ExpressionNode(ExpressionHasRandomizeFunction());
// matches
//   result = randomize(obj);
//
inline constexpr auto ExpressionHasRandomizeFunction =
    verible::matcher::MakePathMatcher(N(kRandomizeFunctionCall));

// Matches against the SymbolIdentifier leaf containing the name
// of a function
//
// For instance,
//   ExpressionNode(ExpressionHasFunctionCall(FunctionCallHasId()));
// matches
//   x = foo(); // innermost matcher matches "foo" token
//
inline constexpr auto UnqualifiedReferenceHasId =
    verible::matcher::MakePathMatcher(N(kLocalRoot), N(kUnqualifiedId),
                                      L(SymbolIdentifier));
inline constexpr auto FunctionCallHasId = UnqualifiedReferenceHasId;

inline constexpr auto ExpressionHasReference =
    verible::matcher::MakePathMatcher(N(kFunctionCall), N(kReferenceCallBase),
                                      N(kReference));

// Matches if the WIDTH in "WIDTH 'BASE DIGITS" is a constant (decimal).
//
// For instance,
//   NodekNumber(NumberHasConstantWidth())
// matches
//   "32" in "32'h0"
//
// TODO(fangism): The path matcher actually finds the constant in any
// child node position.  To be more precise, we only want to look at the
// leftmost child, as in a positional-child-matcher.
inline constexpr auto NumberHasConstantWidth =
    verible::matcher::MakePathMatcher(L(TK_DecNumber));

// Matches if the base of "'BASE DIGITS" is binary.
//
// For instance,
//   NodekNumber(NumberIsBinary())
// matches
//   "'b" in "4'b1111"
//
inline constexpr auto NumberIsBinary =
    verible::matcher::MakePathMatcher(L(TK_BinBase));

// Matches the digits of "'BASE DIGITS" when base is binary.
//
// For instance,
//   NodekNumber(NumberHasBinaryDigits())
// matches
//   "1111" in "4'b1111"
//
inline constexpr auto NumberHasBinaryDigits =
    verible::matcher::MakePathMatcher(L(TK_BinDigits));

// Matches if the LITERAL in "WIDTH'LITERAL" specifies a numeric base ([bdho]).
//
// For instance,
//   NodekNumber(NumberHasBasedLiteral())
// matches
//   "'b1111" in "4'b1111"
//
inline constexpr auto NumberHasBasedLiteral =
    verible::matcher::MakePathMatcher(N(kBaseDigits));

// Matches against the positional parameter list contained within an
// actual parameter list if one exists.
//
// For instance,
//   ActualParameterListNode(ActualParameterListHasPositionalParameterList());
// matches
//   foo #(1, 2) bar;
// and does not match
//   foo $(.param(1), .param2(2));
//
inline constexpr auto ActualParameterListHasPositionalParameterList =
    verible::matcher::MakePathMatcher(N(kParenGroup),
                                      N(kActualParameterPositionalList));

// Matches the port list of a gate instance.
// For instance,
//   ActualGateInstanceNode(GateInstanceHasPortList());
// matches
//   foo bar(port1, port2);
//
inline constexpr auto GateInstanceHasPortList =
    verible::matcher::MakePathMatcher(N(kParenGroup), N(kPortActualList));

// Matches against a node's child tagged with kBegin.kLabel if one exists.
//
// For instance, matches ": gen_posedge" within
//   generate
//     if (TypeIsPosedge) begin : gen_posedge
//       always @(posedge clk) foo <= bar;
//     end
//   endgenerate
inline constexpr auto HasBeginLabel =
    verible::matcher::MakePathMatcher(N(kBegin), N(kLabel));

// Matches against a disable node's child tagged with kReference if one exists.
inline constexpr auto DisableStatementHasLabel =
    verible::matcher::MakePathMatcher(N(kReference));

// Matches event controls that use *.
// For instance,
//
// matches down to '*' in:
//   always @* begin
//     c = d;
//   end
// and does not match:
//   always_comb begin
//     c = d;
//   end
inline constexpr auto AlwaysStatementHasEventControlStar =
    verible::matcher::MakePathMatcher(N(kProceduralTimingControlStatement),
                                      N(kEventControl), L('*'));

inline constexpr auto AlwaysStatementHasEventControlStarAndParentheses =
    verible::matcher::MakePathMatcher(N(kProceduralTimingControlStatement),
                                      N(kEventControl), N(kParenGroup), L('*'));

inline constexpr auto AlwaysStatementHasParentheses =
    verible::matcher::MakePathMatcher(N(kProceduralTimingControlStatement),
                                      N(kEventControl), N(kParenGroup));

// Matches occurrence of the 'always' keyword.
// This is needed to distinguish between various kAlwaysStatement's.
// This matches 'always', but not 'always_ff', nor 'always_comb'.
inline constexpr auto AlwaysKeyword =
    verible::matcher::MakePathMatcher(L(TK_always));

// Matches occurrence of the 'always_comb' keyword.
// This is needed to distinguish between various kAlwaysStatement's.
inline constexpr auto AlwaysCombKeyword =
    verible::matcher::MakePathMatcher(L(TK_always_comb));

// Matches occurrence of the 'always_ff' keyword.
// This is needed to distinguish between various kAlwaysStatement's.
inline constexpr auto AlwaysFFKeyword =
    verible::matcher::MakePathMatcher(L(TK_always_ff));

// Matches occurrence of the 'StringLiteral' keyword.
inline constexpr auto StringLiteralKeyword =
    verible::matcher::MakePathMatcher(L(TK_StringLiteral));

// Matches legacy-style begin-block inside generate region.
//
// For instance, matches:
//
//  generate
//     begin
//       ...
//     end
//  endgenerate
//
inline constexpr auto HasGenerateBlock =
    verible::matcher::MakePathMatcher(N(kGenerateItemList), N(kGenerateBlock));

// Matches the RHS of an assignment that is a function call.
// For instance, matches "bar(...)" in:
//
//   ... = bar(...);
//
// and matches "zz.bar(...)" in:
//
//   ... = zz.bar(...);
//
// and matches "zz::bar(...)" in:
//
//   ... = zz::bar(...);
//
inline constexpr auto RValueIsFunctionCall = verible::matcher::MakePathMatcher(
    N(kExpression), N(kFunctionCall), N(kReferenceCallBase));

// Matches a function call if it is qualified.
// For instance, matches:
//
//   foo::bar(...);
//
// but not:
//
//   bar(...);
//
inline constexpr auto FunctionCallIsQualified =
    verible::matcher::MakePathMatcher(N(kReference), N(kLocalRoot),
                                      N(kQualifiedId));

// Matches the arguments of a function call.
// For instance, matches "a", "b", "c" (including commas) of:
//
//   foo("a", "b", "c");
//
// Note: This does not match macro call arguments.
//
inline constexpr auto FunctionCallArguments =
    verible::matcher::MakePathMatcher(N(kParenGroup), N(kArgumentList));

// Matches sub-ranges of array declarations.
// For instances, matches the subtree "[x:y]" in both:
//
//   wire [x:y] w;
//   wire w [x:y];
//
inline constexpr auto DeclarationDimensionsHasRanges =
    verible::matcher::MakePathMatcher(N(kDimensionRange));

// Matches with a default case item.
// For instance, matches:
//
//   casez (in)
//     default : return 3
//   endcase
//
// but not:
//
//   casez (in)
//     1: return 0;
//   endcase
//
inline constexpr auto HasDefaultCase =
    verible::matcher::MakePathMatcher(N(kCaseItemList), N(kDefaultItem));

// Matches with statements qualified with "unique"
// For instance, matches:
//
//   unique case (in)
//     default: return 0;
//   endcase
//
//   unique if (a)
//     ...
//   else if (!a)
//     ...
//
// but not:
//
//   case (in)
//     default: return 0;
//   endcase
//
//   if (a)
//     ...
//   else if (!a)
//     ...
inline constexpr auto HasUniqueQualifier =
    verible::matcher::MakePathMatcher(L(TK_unique));
// Clean up macros
#undef N
#undef L

}  // namespace verilog

#endif  // VERIBLE_VERILOG_CST_VERILOG_MATCHERS_H_
