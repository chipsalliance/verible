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

#ifndef VERIBLE_COMMON_FORMATTING_BASIC_FORMAT_STYLE_H_
#define VERIBLE_COMMON_FORMATTING_BASIC_FORMAT_STYLE_H_

#include <iosfwd>
#include <string>

#include "absl/strings/string_view.h"

namespace verible {

// Style configuration common to all languages.
struct BasicFormatStyle {
  // Each indentation level adds this many spaces.
  int indentation_spaces = 2;

  // Each wrap level adds this many spaces.  This applies when the first
  // element after an open-group section is wrapped.  Otherwise, the indentation
  // level is set to the column position of the open-group operator.
  int wrap_spaces = 4;

  // Target line length limit to stay under when formatting.
  int column_limit = 100;

  // For penalty minimization, this represents the baseline penalty value of
  // exceeding the column limit.  Additional penalty of 1 is incurred for each
  // character over the limit.
  int over_column_limit_penalty = 100;

  // Penalty added to solution for each introduced line break.
  int line_break_penalty = 2;

  // -- Note: when adding new fields, add them in basic_format_style_init.cc
};

// Control how a section of code is indented.
enum class IndentationStyle {
  // One unit of indentation.
  // This applies verible::BasicFormatStyle::indentation_spaces amount of
  // spaces.
  kIndent,
  // Indentation that is applied to long, continued lines.
  // This is also known as hang-indent.
  // This applies verible::BasicFormatStyle::wrap_spaces amount of spaces.
  kWrap,
};

std::ostream &operator<<(std::ostream &, IndentationStyle);

bool AbslParseFlag(absl::string_view, IndentationStyle *, std::string *);

std::string AbslUnparseFlag(const IndentationStyle &);

}  // namespace verible

#endif  // VERIBLE_COMMON_FORMATTING_BASIC_FORMAT_STYLE_H_
