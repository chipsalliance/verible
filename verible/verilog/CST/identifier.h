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

#ifndef VERIBLE_VERILOG_CST_IDENTIFIER_H_
#define VERIBLE_VERILOG_CST_IDENTIFIER_H_

#include <vector>

#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/symbol.h"

namespace verilog {

// Returns all sub-nodes tagged with kIdentifierUnpackedDimensions
std::vector<verible::TreeSearchMatch> FindAllIdentifierUnpackedDimensions(
    const verible::Symbol &);

// Returns all sub-nodes tagged with kPortIdentifier
std::vector<verible::TreeSearchMatch> FindAllPortIdentifiers(
    const verible::Symbol &);

// Returns all sub-nodes tagged with kUnqualifiedId
std::vector<verible::TreeSearchMatch> FindAllUnqualifiedIds(
    const verible::Symbol &);

// Returns all sub-nodes tagged with kQualifiedId
std::vector<verible::TreeSearchMatch> FindAllQualifiedIds(
    const verible::Symbol &);

// Returns all leafs with token type SymbolIdentifier
std::vector<verible::TreeSearchMatch> FindAllSymbolIdentifierLeafs(
    const verible::Symbol &);

// Returns true if identifier node is qualified/scoped.
bool IdIsQualified(const verible::Symbol &);

// Extracts identifier leaf from a kUnqualifiedId node.
const verible::SyntaxTreeLeaf *GetIdentifier(const verible::Symbol &);

// Extracts identifier leaf from a kUnqualifiedId node, or returns the leaf
// as-is.  This automatically peels away the kUnqualifiedId node layer.
const verible::SyntaxTreeLeaf *AutoUnwrapIdentifier(const verible::Symbol &);

// Extracts SymbolIdentifier leaf from a kIdentifierUnpackedDimensions node.
// e.g. extracts "a" from "a[0:1]"
const verible::SyntaxTreeLeaf *
GetSymbolIdentifierFromIdentifierUnpackedDimensions(const verible::Symbol &);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_CST_IDENTIFIER_H_
