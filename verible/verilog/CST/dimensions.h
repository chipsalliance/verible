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

// Helper functions for extracting dimension-related information from
// a Verilog concrete syntax tree.

#ifndef VERIBLE_VERILOG_CST_DIMENSIONS_H_
#define VERIBLE_VERILOG_CST_DIMENSIONS_H_

#include <vector>

#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"

namespace verilog {

// Find all packed dimensions.
std::vector<verible::TreeSearchMatch> FindAllPackedDimensions(
    const verible::Symbol &);

// Find all unpacked dimensions.
std::vector<verible::TreeSearchMatch> FindAllUnpackedDimensions(
    const verible::Symbol &);

// Find all dimension sequences, which can appear in packed and unpacked
// dimensions.
std::vector<verible::TreeSearchMatch> FindAllDeclarationDimensions(
    const verible::Symbol &);

// Returns x from [x:y] declared dimensions.  Argument must be a DimensionRange
// node.
const verible::Symbol *GetDimensionRangeLeftBound(const verible::Symbol &);

// Returns y from [x:y] declared dimensions.  Argument must be a DimensionRange
// node.
const verible::Symbol *GetDimensionRangeRightBound(const verible::Symbol &);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_CST_DIMENSIONS_H_
