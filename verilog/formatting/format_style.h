// Copyright 2017-2019 The Verible Authors.
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

#ifndef VERIBLE_VERILOG_FORMATTING_FORMAT_STYLE_H_
#define VERIBLE_VERILOG_FORMATTING_FORMAT_STYLE_H_

#include <iosfwd>
#include <string>

#include "absl/strings/string_view.h"
#include "common/formatting/basic_format_style.h"

namespace verilog {
namespace formatter {

// Formatting policy around using original input spaces.
enum class PreserveSpaces {
  // Completely discards all white space information from original input.
  // Performs wrapping optimization.
  None,

  // Use original spacing in inter-token cases that are not explicitly handled
  // in case logic.  Performs wrapping optimization on explicitly covered
  // inter-token cases.
  UnhandledCasesOnly,

  // Do NO formatting and print the pre-existing spaces between tokens
  // instead of calculating ideal spaces.  This is only used for internal
  // testing and debugging, and not intended for production use.
  All,
};

std::ostream& operator<<(std::ostream&, PreserveSpaces);

// Flag handling for PreserveSpaces.
bool AbslParseFlag(absl::string_view, PreserveSpaces*, std::string*);
std::string AbslUnparseFlag(const PreserveSpaces& mode);

// TODO(b/140277909): separate policy for vertical spaces and line breaks.

// Style parameters that are specific to Verilog formatter
struct FormatStyle : public verible::BasicFormatStyle {
  // TODO(fangism): introduce the following knobs:
  //
  // Unless forced by previous line, starting a line with a comma is
  // generally discouraged.
  // int break_before_separator_penalty = 20;

  // Horizontal (inter-token) space preservation policy.
  PreserveSpaces preserve_horizontal_spaces =
      PreserveSpaces::UnhandledCasesOnly;

  // Vertical (inter-line) space preservation policy.
  // This takes effect only if preserve_horizontal_spaces != All.
  PreserveSpaces preserve_vertical_spaces = PreserveSpaces::None;
};

}  // namespace formatter
}  // namespace verilog

#endif  // VERIBLE_VERILOG_FORMATTING_FORMAT_STYLE_H_
