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

#ifndef VERIBLE_VERILOG_FORMATTING_FORMAT_STYLE_H_
#define VERIBLE_VERILOG_FORMATTING_FORMAT_STYLE_H_

#include <iosfwd>
#include <string>

#include "absl/strings/string_view.h"
#include "common/formatting/basic_format_style.h"

namespace verilog {
namespace formatter {

// Style parameters that are specific to Verilog formatter
struct FormatStyle : public verible::BasicFormatStyle {
  // TODO(fangism): introduce the following knobs:
  //
  // Unless forced by previous line, starting a line with a comma is
  // generally discouraged.
  // int break_before_separator_penalty = 20;

  // TODO(fangism): parameter to limit number of consecutive blank lines to
  // preserve between partitions.
};

}  // namespace formatter
}  // namespace verilog

#endif  // VERIBLE_VERILOG_FORMATTING_FORMAT_STYLE_H_
