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

#ifndef VERIBLE_VERILOG_FORMATTING_ALIGN_H_
#define VERIBLE_VERILOG_FORMATTING_ALIGN_H_

#include <vector>

#include "absl/strings/string_view.h"
#include "common/formatting/format_token.h"
#include "common/formatting/token_partition_tree.h"
#include "common/strings/position.h"  // for ByteOffsetSet
#include "verilog/formatting/format_style.h"

namespace verilog {
namespace formatter {

// For certain Verilog language construct groups, vertically align some
// tokens by inserting padding-spaces.
// 'ftokens' is only used to provide a base mutable iterator for the purpose
// of being able to modify inter-token spacing.
// TODO(fangism): pass in disabled formatting ranges
void TabularAlignTokenPartitions(
    verible::TokenPartitionTree* partition_ptr,
    std::vector<verible::PreFormatToken>* ftokens, absl::string_view full_text,
    const verible::ByteOffsetSet& disabled_byte_ranges,
    const FormatStyle& style);

}  // namespace formatter
}  // namespace verilog

#endif  // VERIBLE_VERILOG_FORMATTING_ALIGN_H_
