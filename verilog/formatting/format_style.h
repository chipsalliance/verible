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

#include "common/formatting/align.h"
#include "common/formatting/basic_format_style.h"

namespace verilog {
namespace formatter {

// Style parameters that are specific to Verilog formatter
struct FormatStyle : public verible::BasicFormatStyle {
  using AlignmentPolicy = verible::AlignmentPolicy;

  FormatStyle() : verible::BasicFormatStyle() {
    over_column_limit_penalty = 10000;
  }

  // Control how named port_declaration (e.g. in modules, interfaces) are
  // formatted.  Internal tests assume these are forced to kAlign.
  AlignmentPolicy port_declarations_alignment = AlignmentPolicy::kAlign;

  // If true, format module instantiations, else leave those regions
  // unformatted. A user may wish to disable this in favor of their own aligned
  // formatting. The default value here is true so that compact formatting can
  // continue to be covered in integration tests. This flag is prone to change
  // or be removed in the future.
  // TODO(b/164286027): align module instance named parameters
  //   and promote the control for this into AlignmentPolicy.
  bool format_module_instantiations = true;

  // Control how named parameters (e.g. in module instances) are formatted.
  // For internal testing purposes, this is default to kAlign.
  AlignmentPolicy named_parameter_alignment = AlignmentPolicy::kAlign;

  // Control how named ports (e.g. in module instances) are formatted.
  // Internal tests assume these are forced to kAlign.
  AlignmentPolicy named_port_alignment = AlignmentPolicy::kAlign;

  // Control how module-local net/variable declarations are formatted.
  // Internal tests assume these are forced to kAlign.
  AlignmentPolicy module_net_variable_alignment = AlignmentPolicy::kAlign;

  // Control how formal parameters in modules/interfaces/classes are formatted.
  // Internal tests assume these are forced to kAlign.
  AlignmentPolicy formal_parameters_alignment = AlignmentPolicy::kAlign;

  // Control how class member variables are formatted.
  // Internal tests assume these are forced to kAlign.
  AlignmentPolicy class_member_variable_alignment = AlignmentPolicy::kAlign;

  // At this time line wrap optimization is problematic and risks ruining
  // otherwise reasonable code.  When set to false, this switch will make the
  // formatter give-up and leave code as-is in cases where it would otherwise
  // attempt to do line wrap optimization.  By doing nothing in those cases, we
  // reduce the risk of harming already decent code.
  bool try_wrap_long_lines = true;

  // TODO(fangism): introduce the following knobs:
  //
  // Unless forced by previous line, starting a line with a comma is
  // generally discouraged.
  // int break_before_separator_penalty = 20;

  // TODO(fangism): parameter to limit number of consecutive blank lines to
  // preserve between partitions.

  void ApplyToAllAlignmentPolicies(AlignmentPolicy policy) {
    port_declarations_alignment = policy;
    named_parameter_alignment = policy;
    named_port_alignment = policy;
    module_net_variable_alignment = policy;
    formal_parameters_alignment = policy;
    class_member_variable_alignment = policy;
  }
};

}  // namespace formatter
}  // namespace verilog

#endif  // VERIBLE_VERILOG_FORMATTING_FORMAT_STYLE_H_
