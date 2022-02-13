// Copyright 2017-2021 The Verible Authors.
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

// Implementation of a code layout optimizer described by
// Phillip Yelland in "A New Approach to Optimal Code Formatting"
// (https://research.google/pubs/pub44667/) and originally implemented
// in rfmt (https://github.com/google/rfmt).

#ifndef VERIBLE_VERILOG_FORMATTING_LAYOUT_OPTIMIZER_H_
#define VERIBLE_VERILOG_FORMATTING_LAYOUT_OPTIMIZER_H_

#include <algorithm>
#include <iterator>
#include <ostream>
#include <type_traits>
#include <vector>

#include "absl/container/fixed_array.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "common/formatting/basic_format_style.h"
#include "common/formatting/token_partition_tree.h"
#include "common/formatting/unwrapped_line.h"
#include "common/util/vector_tree.h"

namespace verible {

// Handles formatting of `node` using Layout Optimizer.
void OptimizeTokenPartitionTree(const BasicFormatStyle& style,
                                TokenPartitionTree* node);

}  // namespace verible

#endif  // VERIBLE_VERILOG_FORMATTING_LAYOUT_OPTIMIZER_H_
