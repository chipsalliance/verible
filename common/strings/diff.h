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

#ifndef VERIBLE_COMMON_STRINGS_DIFF_H_
#define VERIBLE_COMMON_STRINGS_DIFF_H_

#include <iosfwd>
#include <vector>

#include "absl/strings/string_view.h"
#include "common/strings/position.h"
#include "external_libs/editscript.h"

namespace verible {

// The LineDiffs structure holds line-based views of two texts
// and the edit sequence (diff) to go from 'before_text' to 'after_text'.
// No string copying is done, and the caller is responsible for ensuring
// that the externally owned string memory outlives this object.
//
// Usage:
//   LineDiffs diffs(old_text, new_text);
//
struct LineDiffs {
  const absl::string_view before_text;
  const absl::string_view after_text;
  const std::vector<absl::string_view> before_lines;  // lines (excluding \n)
  const std::vector<absl::string_view> after_lines;   // lines (excluding \n)
  const diff::Edits edits;  // line difference/edit-sequence between texts.

  // Computes the line-difference between before_text and after_text.
  LineDiffs(absl::string_view before_text, absl::string_view after_text);

  std::ostream& PrintEdit(std::ostream&, const diff::Edit&) const;
};

// Prints a monolithic single-hunk unified-diff.
std::ostream& operator<<(std::ostream&, const LineDiffs& diffs);

// Translates diff::Edits to an interval-set representation.
// diff::Edits are 0-indexed, but the returned line numbers will be 1-indexed.
LineNumberSet DiffEditsToAddedLineNumbers(const diff::Edits&);

}  // namespace verible

#endif  // VERIBLE_COMMON_STRINGS_DIFF_H_
