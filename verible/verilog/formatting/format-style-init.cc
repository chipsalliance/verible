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

#include "verible/verilog/formatting/format-style-init.h"

#include "absl/flags/flag.h"
#include "verible/common/formatting/align.h"
#include "verible/common/formatting/basic-format-style-init.h"
#include "verible/common/formatting/basic-format-style.h"
#include "verible/verilog/formatting/format-style.h"

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
    AlignmentPolicy, module_net_variable_alignment,  //
    AlignmentPolicy::kInferUserIntent,
    "Format net/variable declarations: {align,flush-left,preserve,infer}");
ABSL_FLAG(AlignmentPolicy, formal_parameters_alignment,
          AlignmentPolicy::kInferUserIntent,
          "Format formal parameters: {align,flush-left,preserve,infer}");
ABSL_FLAG(AlignmentPolicy, class_member_variable_alignment,
          AlignmentPolicy::kInferUserIntent,
          "Format class member variables: {align,flush-left,preserve,infer}");
ABSL_FLAG(AlignmentPolicy, case_items_alignment,
          AlignmentPolicy::kInferUserIntent,
          "Format case items: {align,flush-left,preserve,infer}");
ABSL_FLAG(AlignmentPolicy, distribution_items_alignment,
          AlignmentPolicy::kInferUserIntent,
          "Aligh distribution items: {align,flush-left,preserve,infer}");
ABSL_FLAG(AlignmentPolicy, assignment_statement_alignment,
          AlignmentPolicy::kInferUserIntent,
          "Format various assignments: {align,flush-left,preserve,infer}");
ABSL_FLAG(AlignmentPolicy, enum_assignment_statement_alignment,
          AlignmentPolicy::kInferUserIntent,
          "Format assignments with enums: {align,flush-left,preserve,infer}");

ABSL_FLAG(bool, compact_indexing_and_selections, true,
          "Use compact binary expressions inside indexing / bit selection "
          "operators");

ABSL_FLAG(bool, wrap_end_else_clauses, false,
          "Split end and else keywords into separate lines");

ABSL_FLAG(bool, port_declarations_right_align_packed_dimensions, false,
          "If true, packed dimensions in contexts with enabled alignment are "
          "aligned to the right.");

ABSL_FLAG(bool, port_declarations_right_align_unpacked_dimensions, false,
          "If true, unpacked dimensions in contexts with enabled alignment are "
          "aligned to the right.");

// -- Deprecated flags. These were typos. Remove after 2022-01-01
ABSL_RETIRED_FLAG(
    AlignmentPolicy, net_variable_alignment,  //
    AlignmentPolicy::kInferUserIntent,
    "Format net/variable declarations: {align,flush-left,preserve,infer}");

ABSL_RETIRED_FLAG(
    AlignmentPolicy, class_member_variables_alignment,
    AlignmentPolicy::kInferUserIntent,
    "Format class member variables: {align,flush-left,preserve,infer}");

namespace verilog {
namespace formatter {
void InitializeFromFlags(FormatStyle *style) {
  verible::InitializeFromFlags(style);  // Initialize BasicFormatStyle

#define STYLE_FROM_FLAG(name) style->name = absl::GetFlag(FLAGS_##name)

  // Simply in the sequence as declared in struct FormatStyle
  STYLE_FROM_FLAG(port_declarations_indentation);
  STYLE_FROM_FLAG(port_declarations_alignment);
  STYLE_FROM_FLAG(struct_union_members_alignment);
  STYLE_FROM_FLAG(named_parameter_indentation);
  STYLE_FROM_FLAG(named_parameter_alignment);
  STYLE_FROM_FLAG(named_port_indentation);
  STYLE_FROM_FLAG(named_port_alignment);
  STYLE_FROM_FLAG(module_net_variable_alignment);
  STYLE_FROM_FLAG(assignment_statement_alignment);
  STYLE_FROM_FLAG(enum_assignment_statement_alignment);
  STYLE_FROM_FLAG(formal_parameters_indentation);
  STYLE_FROM_FLAG(formal_parameters_alignment);
  STYLE_FROM_FLAG(class_member_variable_alignment);
  STYLE_FROM_FLAG(case_items_alignment);
  STYLE_FROM_FLAG(distribution_items_alignment);
  STYLE_FROM_FLAG(port_declarations_right_align_packed_dimensions);
  STYLE_FROM_FLAG(port_declarations_right_align_unpacked_dimensions);
  STYLE_FROM_FLAG(try_wrap_long_lines);
  STYLE_FROM_FLAG(expand_coverpoints);
  STYLE_FROM_FLAG(compact_indexing_and_selections);
  STYLE_FROM_FLAG(wrap_end_else_clauses);

#undef STYLE_FROM_FLAG
}

}  // namespace formatter
}  // namespace verilog
