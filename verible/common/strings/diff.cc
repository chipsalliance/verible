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

#include "verible/common/strings/diff.h"

#include <cstddef>
#include <iostream>
#include <string_view>
#include <vector>

#include "external_libs/editscript.h"
#include "verible/common/strings/position.h"
#include "verible/common/strings/split.h"
#include "verible/common/util/iterator-range.h"

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

LineDiffs::LineDiffs(std::string_view before, std::string_view after)
    : before_text(before),
      after_text(after),
      before_lines(SplitLinesKeepLineTerminator(before_text)),
      after_lines(SplitLinesKeepLineTerminator(after_text)),
      edits(diff::GetTokenDiffs(before_lines.begin(), before_lines.end(),
                                after_lines.begin(), after_lines.end())) {}

template <typename Iter>
static std::ostream &PrintLineRange(std::ostream &stream, char op, Iter start,
                                    Iter end) {
  for (const auto &line : make_range(start, end)) {
    stream << op << line;
  }
  return stream;
}

std::ostream &LineDiffs::PrintEdit(std::ostream &stream,
                                   const Edit &edit) const {
  const char op = EditOperationToLineMarker(edit.operation);
  if (edit.operation == Operation::INSERT) {
    PrintLineRange(stream, op, after_lines.begin() + edit.start,
                   after_lines.begin() + edit.end);
    if (after_lines[edit.end - 1].back() != '\n') stream << "\n";
  } else {
    PrintLineRange(stream, op, before_lines.begin() + edit.start,
                   before_lines.begin() + edit.end);
    if (before_lines[edit.end - 1].back() != '\n') stream << "\n";
  }
  stream << std::flush;
  return stream;
}

std::ostream &operator<<(std::ostream &stream, const LineDiffs &diffs) {
  for (const auto &edit : diffs.edits) {
    diffs.PrintEdit(stream, edit);
  }
  return stream;
}

LineNumberSet DiffEditsToAddedLineNumbers(const Edits &edits) {
  LineNumberSet added_lines;
  for (const auto &edit : edits) {
    if (edit.operation == Operation::INSERT) {
      // Add 1 to convert from 0-indexed to 1-indexed.
      added_lines.Add(
          {static_cast<int>(edit.start) + 1, static_cast<int>(edit.end) + 1});
    }
  }
  return added_lines;
}

std::vector<diff::Edits> DiffEditsToPatchHunks(const diff::Edits &edits,
                                               int common_context) {
  const int split_threshold = common_context * 2;
  std::vector<diff::Edits> hunks(1);  // start with 1 empty destination vector
  for (const diff::Edit &edit : edits) {
    auto &current_hunk = hunks.back();
    if (edit.operation == Operation::EQUALS) {
      const int edit_size = edit.end - edit.start;
      if (current_hunk.empty()) {
        // For "end-pieces" (in this case, the head), threshold should be
        // common_context, not split_threshold.
        if (edit_size > common_context) {
          // Add the tail end of this edit.
          current_hunk.push_back(
              diff::Edit{edit.operation, edit.end - common_context, edit.end});
        } else {
          // Add the whole edit.
          current_hunk.push_back(edit);
        }
      } else {  // !current_hunk.empty()
        // We don't know what follows this edit, so this may still be oversized.
        // A final pass will trim excess sizing of EQUALS edits in tail
        // position.
        if (edit_size > split_threshold) {
          // Close off the current hunk.
          current_hunk.push_back(diff::Edit{edit.operation, edit.start,
                                            edit.start + common_context});
          // Start the next hunk.
          hunks.push_back(diff::Edits{
              diff::Edit{edit.operation, edit.end - common_context, edit.end}});
        } else {
          // Add the whole edit.
          current_hunk.push_back(edit);
        }
      }
    } else {  // operation is INSERT or DELETE
      current_hunk.push_back(edit);
    }
  }

  // The last hunk may have been started before knowing it was the last one.
  // Remove if it is a no-op.
  const auto &last_hunk = hunks.back();  // hunks is always non-empty
  if (last_hunk.size() == 1 &&
      last_hunk.front().operation == Operation::EQUALS) {
    // This last hunk's only element is an Operation::EQUALS (no-change),
    // so remove it.
    hunks.pop_back();
  }

  // Trim excess EQUALS tail edits in each hunk.
  for (auto &hunk : hunks) {
    auto &tail = hunk.back();
    if (tail.operation == Operation::EQUALS) {
      if (tail.end - tail.start > common_context) {
        tail.end = tail.start + common_context;
      }
    }
  }
  return hunks;
}

void LineDiffsToUnifiedDiff(std::ostream &stream, const LineDiffs &linediffs,
                            unsigned common_context, std::string_view file_a,
                            std::string_view file_b) {
  const std::vector<diff::Edits> chunks =
      DiffEditsToPatchHunks(linediffs.edits, common_context);

  if (chunks.empty()) return;

  if (!file_a.empty()) {
    if (file_b.empty()) {
      stream << "--- a/" << file_a << std::endl;
      stream << "+++ b/" << file_a << std::endl;
    } else {
      stream << "--- " << file_a << std::endl;
      stream << "+++ " << file_b << std::endl;
    }
  }

  int added_lines_count = 0;
  for (const auto &chunk : chunks) {
    int chunk_before_lines_count = 0;
    int chunk_added_lines_count = 0;

    for (const auto &edit : chunk) {
      if (edit.operation == Operation::INSERT) {
        chunk_added_lines_count += edit.end - edit.start;
      } else if (edit.operation == Operation::DELETE) {
        chunk_before_lines_count += edit.end - edit.start;
        chunk_added_lines_count -= edit.end - edit.start;
      } else {
        chunk_before_lines_count += edit.end - edit.start;
      }
    }
    const int chunk_after_lines_count =
        chunk_before_lines_count + chunk_added_lines_count;

    // Chunk header
    stream << "@@ -" << (chunk.front().start + 1);
    if (chunk_before_lines_count > 1) stream << "," << chunk_before_lines_count;
    stream << " +" << (chunk.front().start + added_lines_count + 1);
    if (chunk_after_lines_count > 1) stream << "," << chunk_after_lines_count;
    stream << " @@" << std::endl;

    added_lines_count += chunk_added_lines_count;

    for (const auto &edit : chunk) {
      linediffs.PrintEdit(stream, edit);

      // Last line from either original or new text, and final '\n' is missing?
      if ((edit.operation != Operation::INSERT &&
           size_t(edit.end) == linediffs.before_lines.size() &&
           linediffs.before_text.back() != '\n') ||
          (edit.operation == Operation::INSERT &&
           size_t(edit.end) == linediffs.after_lines.size() &&
           linediffs.after_text.back() != '\n')) {
        stream << "\\ No newline at end of file" << std::endl;
      }
    }
  }
}

}  // namespace verible
