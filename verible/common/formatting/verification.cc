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

#include "verible/common/formatting/verification.h"

#include <sstream>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "verible/common/strings/diff.h"
#include "verible/common/strings/position.h"

namespace verible {

absl::Status ReformatMustMatch(absl::string_view original_text,
                               const LineNumberSet &lines,
                               absl::string_view formatted_text,
                               absl::string_view reformatted_text) {
  if (reformatted_text != formatted_text) {
    const verible::LineDiffs formatting_diffs(formatted_text, reformatted_text);
    std::ostringstream diff_stream;
    diff_stream << formatting_diffs;
    std::ostringstream lines_stream;
    lines_stream << lines;
    return absl::DataLossError(absl::StrCat(
        "Re-formatted text does not match formatted text; "
        "formatting failed to converge!  Please file a bug.\n"
        "========== Original: --lines: ==========",
        lines_stream.str(), "\n", original_text,                         //
        "============== Formatted: ==============\n", formatted_text,    //
        "============= Re-formatted: ============\n", reformatted_text,  //
        "============== Diffs are: ==============\n", diff_stream.str()));
  }
  return absl::OkStatus();
}

}  // namespace verible
