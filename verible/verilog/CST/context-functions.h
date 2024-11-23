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

#ifndef VERIBLE_VERILOG_CST_CONTEXT_FUNCTIONS_H_
#define VERIBLE_VERILOG_CST_CONTEXT_FUNCTIONS_H_

#include "verible/common/text/syntax-tree-context.h"
#include "verible/verilog/CST/verilog-nonterminals.h"

namespace verilog {
namespace analysis {

// The following functions are specialized for common Verilog context queries.

inline bool ContextIsInsideClass(const verible::SyntaxTreeContext &context) {
  return context.IsInside(NodeEnum::kClassDeclaration);
}

inline bool ContextIsInsideModule(const verible::SyntaxTreeContext &context) {
  return context.IsInside(NodeEnum::kModuleDeclaration);
}

// Does not treat global scope as being inside a package.
inline bool ContextIsInsidePackage(const verible::SyntaxTreeContext &context) {
  return context.IsInside(NodeEnum::kPackageDeclaration);
}

inline bool ContextIsInsidePackedDimensions(
    const verible::SyntaxTreeContext &context) {
  return context.IsInside(NodeEnum::kPackedDimensions);
}

inline bool ContextIsInsideUnpackedDimensions(
    const verible::SyntaxTreeContext &context) {
  // Exclude being inside an associative array dimensions ([type]).
  return context.IsInsideFirst({NodeEnum::kUnpackedDimensions},
                               {NodeEnum::kDimensionAssociativeType});
}

inline bool ContextIsInsideFormalParameterList(
    const verible::SyntaxTreeContext &context) {
  return context.IsInside(NodeEnum::kFormalParameterList);
}

inline bool ContextIsInsideTaskFunctionPortList(
    const verible::SyntaxTreeContext &context) {
  return context.IsInside(NodeEnum::kPortList);
}

inline bool ContextIsInsideStatement(
    const verible::SyntaxTreeContext &context) {
  return context.IsInside(NodeEnum::kStatement);
}

inline bool ContextIsInsideDeclarationDimensions(
    const verible::SyntaxTreeContext &context) {
  return context.DirectParentIsOneOf(
      {NodeEnum::kDimensionRange, NodeEnum::kDimensionScalar,
       NodeEnum::kDimensionSlice, NodeEnum::kDimensionAssociativeType});
}
// add more as needed

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_CST_CONTEXT_FUNCTIONS_H_
