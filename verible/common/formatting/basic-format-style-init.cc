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

#include "verible/common/formatting/basic-format-style-init.h"

#include "absl/flags/flag.h"
#include "verible/common/formatting/basic-format-style.h"

ABSL_FLAG(int, indentation_spaces, 2,
          "Each indentation level adds this many spaces.");

ABSL_FLAG(int, wrap_spaces, 4,
          "Each wrap level adds this many spaces.  This applies when the first "
          "element after an open-group section is wrapped.  Otherwise, the "
          "indentation level is set to the column position of the open-group "
          "operator.");

ABSL_FLAG(int, column_limit, 100,
          "Target line length limit to stay under when formatting.");

ABSL_FLAG(int, over_column_limit_penalty, 100,
          "For penalty minimization, this represents the baseline penalty "
          "value of exceeding the column limit.  Additional penalty of 1 is "
          "incurred for each character over this limit");

ABSL_FLAG(int, line_break_penalty, 2,
          "Penalty added to solution for each introduced line break.");

namespace verible {
void InitializeFromFlags(BasicFormatStyle *style) {
#define STYLE_FROM_FLAG(name) style->name = absl::GetFlag(FLAGS_##name)

  // Simply in the sequence as declared in struct BasicFormatStyle
  STYLE_FROM_FLAG(indentation_spaces);
  STYLE_FROM_FLAG(wrap_spaces);
  STYLE_FROM_FLAG(column_limit);
  STYLE_FROM_FLAG(over_column_limit_penalty);
  STYLE_FROM_FLAG(line_break_penalty);

#undef STYLE_FROM_FLAG
}
}  // namespace verible
