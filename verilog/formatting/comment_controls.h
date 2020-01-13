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

#ifndef VERIBLE_VERILOG_FORMATTING_COMMENT_CONTROLS_H_
#define VERIBLE_VERILOG_FORMATTING_COMMENT_CONTROLS_H_

#include "absl/strings/string_view.h"
#include "common/text/token_stream_view.h"
#include "common/util/interval_set.h"

namespace verilog {
namespace formatter {

// Collection of ranges of byte offsets.
using ByteOffsetSet = verible::IntervalSet<size_t>;

// Returns a representation of byte offsets where true (membership) means
// formatting is disabled.
ByteOffsetSet DisableFormattingRanges(absl::string_view text,
                                      const verible::TokenSequence& tokens);

}  // namespace formatter
}  // namespace verilog

#endif  // VERIBLE_VERILOG_FORMATTING_COMMENT_CONTROLS_H_
