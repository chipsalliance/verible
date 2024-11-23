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

#include "absl/strings/string_view.h"
#include "verible/common/formatting/token-partition-tree.h"
#include "verible/common/strings/position.h"  // for ByteOffsetSet
#include "verible/verilog/formatting/format-style.h"

namespace verilog {
namespace formatter {

// For certain Verilog language construct groups, vertically align some
// tokens by inserting padding-spaces.
// TODO(fangism): pass in disabled formatting ranges
void TabularAlignTokenPartitions(
    const FormatStyle &style, absl::string_view full_text,
    const verible::ByteOffsetSet &disabled_byte_ranges,
    verible::TokenPartitionTree *partition_ptr);

}  // namespace formatter
}  // namespace verilog

#endif  // VERIBLE_VERILOG_FORMATTING_ALIGN_H_
