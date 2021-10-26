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
// LowRISC format style.

#ifndef VERIBLE_VERILOG_FORMATTING_LOWRISC_FORMAT_STYLE_H_
#define VERIBLE_VERILOG_FORMATTING_LOWRISC_FORMAT_STYLE_H_

#include "verilog/formatting/format_style.h"

namespace verilog {
namespace formatter {

// Style parameters specific to LowRISC format style guide
struct LowRISCFormatStyle : public FormatStyle {
  LowRISCFormatStyle() : FormatStyle() {
    indentation_spaces = 2;
    wrap_spaces  = 4;
    column_limit = 100;

    formal_parameters_indentation = verible::IndentationStyle::kIndent;
    named_parameter_indentation   = verible::IndentationStyle::kIndent;
    named_port_indentation        = verible::IndentationStyle::kIndent;
    port_declarations_alignment   = verible::AlignmentPolicy::kPreserve;

    style_name_ = "lowrisc";
  }

  LowRISCFormatStyle(unsigned int n) : LowRISCFormatStyle() {
    column_limit = n;
  }

  LowRISCFormatStyle(const LowRISCFormatStyle&) = default;
};

}  // namespace formatter
}  // namespace verilog

#endif  // VERIBLE_VERILOG_FORMATTING_LOWRISC_FORMAT_STYLE_H_
