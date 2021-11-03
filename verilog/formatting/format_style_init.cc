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

#include "verilog/formatting/format_style_init.h"

#include "absl/flags/flag.h"

using verible::AlignmentPolicy;
using verible::IndentationStyle;

ABSL_FLAG(bool, try_wrap_long_lines, false,
          "If true, let the formatter attempt to optimize line wrapping "
          "decisions where wrapping is needed, else leave them unformatted.  "
          "This is a short-term measure to reduce risk-of-harm.");

ABSL_FLAG(bool, expand_coverpoints, false,
          "If true, always expand coverpoints.");

// These flags exist in the short term to disable formatting of some regions.
// Do not expect to be able to use these in the long term, once they find
// a better home in a configuration struct.

// "indent" means 2 spaces, "wrap" means 4 spaces.
ABSL_FLAG(IndentationStyle, port_declarations_indentation,
          IndentationStyle::kWrap, "Indent port declarations: {indent,wrap}");
ABSL_FLAG(IndentationStyle, formal_parameters_indentation,
          IndentationStyle::kWrap, "Indent formal parameters: {indent,wrap}");
ABSL_FLAG(IndentationStyle, named_parameter_indentation,
          IndentationStyle::kWrap,
          "Indent named parameter assignments: {indent,wrap}");
ABSL_FLAG(IndentationStyle, named_port_indentation, IndentationStyle::kWrap,
          "Indent named port connections: {indent,wrap}");

// For most of the following in this group, kInferUserIntent is a reasonable
// default behavior because it allows for user-control with minimal invasiveness
// and burden on the user.
ABSL_FLAG(AlignmentPolicy, port_declarations_alignment,
          AlignmentPolicy::kInferUserIntent,
          "Format port declarations: {align,flush-left,preserve,infer}");
ABSL_FLAG(AlignmentPolicy, struct_union_members_alignment,
          AlignmentPolicy::kInferUserIntent,
          "Format struct/union members: {align,flush-left,preserve,infer}");
ABSL_FLAG(AlignmentPolicy, named_parameter_alignment,
          AlignmentPolicy::kInferUserIntent,
          "Format named actual parameters: {align,flush-left,preserve,infer}");
ABSL_FLAG(AlignmentPolicy, named_port_alignment,
          AlignmentPolicy::kInferUserIntent,
          "Format named port connections: {align,flush-left,preserve,infer}");
ABSL_FLAG(
    AlignmentPolicy, net_variable_alignment,  //
    AlignmentPolicy::kInferUserIntent,
    "Format net/variable declarations: {align,flush-left,preserve,infer}");
ABSL_FLAG(AlignmentPolicy, formal_parameters_alignment,
          AlignmentPolicy::kInferUserIntent,
          "Format formal parameters: {align,flush-left,preserve,infer}");
ABSL_FLAG(AlignmentPolicy, class_member_variables_alignment,
          AlignmentPolicy::kInferUserIntent,
          "Format class member variables: {align,flush-left,preserve,infer}");
ABSL_FLAG(AlignmentPolicy, case_items_alignment,
          AlignmentPolicy::kInferUserIntent,
          "Format case items: {align,flush-left,preserve,infer}");
ABSL_FLAG(AlignmentPolicy, assignment_statement_alignment,
          AlignmentPolicy::kInferUserIntent,
          "Format various assignments: {align,flush-left,preserve,infer}");

ABSL_FLAG(bool, port_declarations_right_align_packed_dimensions, false,
          "If true, packed dimensions in contexts with enabled alignment are "
          "aligned to the right.");

ABSL_FLAG(bool, port_declarations_right_align_unpacked_dimensions, false,
          "If true, unpacked dimensions in contexts with enabled alignment are "
          "aligned to the right.");

namespace verilog {
namespace formatter {
void InitializeFromFlags(FormatStyle *style) {
  // formatting style flags
  style->try_wrap_long_lines = absl::GetFlag(FLAGS_try_wrap_long_lines);
  style->expand_coverpoints = absl::GetFlag(FLAGS_expand_coverpoints);

  // various indentation control
  style->port_declarations_indentation =
      absl::GetFlag(FLAGS_port_declarations_indentation);
  style->formal_parameters_indentation =
      absl::GetFlag(FLAGS_formal_parameters_indentation);
  style->named_parameter_indentation =
      absl::GetFlag(FLAGS_named_parameter_indentation);
  style->named_port_indentation = absl::GetFlag(FLAGS_named_port_indentation);

  // various alignment control
  style->port_declarations_alignment =
      absl::GetFlag(FLAGS_port_declarations_alignment);
  style->struct_union_members_alignment =
      absl::GetFlag(FLAGS_struct_union_members_alignment);
  style->named_parameter_alignment =
      absl::GetFlag(FLAGS_named_parameter_alignment);
  style->named_port_alignment = absl::GetFlag(FLAGS_named_port_alignment);
  style->module_net_variable_alignment =
      absl::GetFlag(FLAGS_net_variable_alignment);
  style->formal_parameters_alignment =
      absl::GetFlag(FLAGS_formal_parameters_alignment);
  style->class_member_variable_alignment =
      absl::GetFlag(FLAGS_class_member_variables_alignment);
  style->case_items_alignment = absl::GetFlag(FLAGS_case_items_alignment);
  style->assignment_statement_alignment =
      absl::GetFlag(FLAGS_assignment_statement_alignment);

  style->port_declarations_right_align_packed_dimensions =
      absl::GetFlag(FLAGS_port_declarations_right_align_packed_dimensions);
  style->port_declarations_right_align_unpacked_dimensions =
      absl::GetFlag(FLAGS_port_declarations_right_align_unpacked_dimensions);
}

}  // namespace formatter
}  // namespace verilog
