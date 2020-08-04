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

#include "common/strings/diff.h"

#include <iostream>
#include <vector>

#include "absl/strings/str_split.h"
#include "common/strings/split.h"
#include "common/util/iterator_range.h"
#include "external_libs/editscript.h"

namespace verible {

using diff::Edit;
using diff::Edits;
using diff::Operation;

static char EditOperationToLineMarker(Operation op) {
  switch (op) {
    case Operation::DELETE:
      return '-';
    case Operation::EQUALS:
      return ' ';
    case Operation::INSERT:
      return '+';
    default:
      return '?';
  }
}

LineDiffs::LineDiffs(absl::string_view before, absl::string_view after)
    : before_text(before),
      after_text(after),
      before_lines(SplitLines(before_text)),
      after_lines(SplitLines(after_text)),
      edits(diff::GetTokenDiffs(before_lines.begin(), before_lines.end(),
                                after_lines.begin(), after_lines.end())) {}

template <typename Iter>
static std::ostream& PrintLineRange(std::ostream& stream, char op, Iter start,
                                    Iter end) {
  for (const auto& line : make_range(start, end)) {
    stream << op << line << std::endl;
  }
  return stream;
}

std::ostream& LineDiffs::PrintEdit(std::ostream& stream,
                                   const Edit& edit) const {
  const char op = EditOperationToLineMarker(edit.operation);
  if (edit.operation == Operation::INSERT) {
    PrintLineRange(stream, op, after_lines.begin() + edit.start,
                   after_lines.begin() + edit.end);
  } else {
    PrintLineRange(stream, op, before_lines.begin() + edit.start,
                   before_lines.begin() + edit.end);
  }
  return stream;
}

std::ostream& operator<<(std::ostream& stream, const LineDiffs& diffs) {
  for (const auto& edit : diffs.edits) {
    diffs.PrintEdit(stream, edit);
  }
  return stream;
}

LineNumberSet DiffEditsToAddedLineNumbers(const Edits& edits) {
  LineNumberSet added_lines;
  for (const auto& edit : edits) {
    if (edit.operation == Operation::INSERT) {
      // Add 1 to cover from 0-indexed to 1-indexed.
      added_lines.Add({edit.start + 1, edit.end + 1});
    }
  }
  return added_lines;
}

}  // namespace verible
