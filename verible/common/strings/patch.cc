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

#include "verible/common/strings/patch.h"

#include <algorithm>
#include <deque>
#include <functional>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "verible/common/strings/position.h"
#include "verible/common/strings/split.h"
#include "verible/common/util/algorithm.h"
#include "verible/common/util/container-iterator-range.h"
#include "verible/common/util/file-util.h"
#include "verible/common/util/iterator-adaptors.h"
#include "verible/common/util/iterator-range.h"
#include "verible/common/util/logging.h"
#include "verible/common/util/status-macros.h"
#include "verible/common/util/user-interaction.h"

namespace verible {

static bool LineMarksOldFile(std::string_view line) {
  return absl::StartsWith(line, "--- ");
}

static bool IsValidMarkedLine(std::string_view line) {
  if (line.empty()) return false;
  switch (line.front()) {
    case ' ':
    case '-':
    case '+':
      return true;
    default:
      return false;
  }
}

namespace internal {

template <typename RangeT, typename Iter>
static std::vector<RangeT> IteratorsToRanges(const std::vector<Iter> &iters) {
  // TODO(fangism): This pattern appears elsewhere in the codebase, so refactor.
  CHECK_GE(iters.size(), 2);
  std::vector<RangeT> result;
  result.reserve(iters.size());
  auto prev = iters.begin();
  for (auto next = std::next(prev); next != iters.end(); prev = next, ++next) {
    result.emplace_back(*prev, *next);
  }
  return result;
}

absl::Status MarkedLine::Parse(std::string_view text) {
  // text is already a whole line
  if (!IsValidMarkedLine(text)) {
    return absl::InvalidArgumentError(absl::StrCat(
        "MarkedLine must begin with one of [ -+], but got: \"", text, "\"."));
  }
  line.assign(text.begin(), text.end());  // copy
  return absl::OkStatus();
}

std::ostream &operator<<(std::ostream &stream, const MarkedLine &line) {
  const term::Color c = line.IsDeleted() ? term::Color::kRed
                        : line.IsAdded() ? term::Color::kCyan
                                         : term::Color::kNone;
  return term::Colored(stream, line.line, c);
}

std::string HunkIndices::FormatToString() const {
  return absl::StrCat(start, ",", count);
}

absl::Status HunkIndices::Parse(std::string_view text) {
  // text is expected to look like "int,int"
  StringSpliterator splitter(text);
  const std::string_view start_text = splitter(',');
  const std::string_view count_text = splitter(',');
  if (!absl::SimpleAtoi(start_text, &start) ||  //
      !absl::SimpleAtoi(count_text, &count) ||  //
      splitter /* unexpected second ',' */) {
    return absl::InvalidArgumentError(
        absl::StrCat("HunkIndices expects int,int, but got: ", text, "\"."));
  }
  return absl::OkStatus();
}

std::ostream &operator<<(std::ostream &stream, const HunkIndices &indices) {
  return stream << indices.FormatToString();
}

absl::Status HunkHeader::Parse(std::string_view text) {
  constexpr std::string_view kDelimiter("@@");
  StringSpliterator tokenizer(text);
  {
    std::string_view first = tokenizer(kDelimiter);
    // first token should be empty
    if (!first.empty() || !tokenizer) {
      return absl::InvalidArgumentError(absl::StrCat(
          "HunkHeader should start with @@, but got: ", text, "\"."));
    }
  }

  // Parse ranges between the "@@"s.
  {
    const std::string_view ranges =
        absl::StripAsciiWhitespace(tokenizer(kDelimiter));
    if (!tokenizer) {
      return absl::InvalidArgumentError(absl::StrCat(
          "HunkHeader expects ranges in @@...@@, but got: ", text, "\"."));
    }

    auto splitter = MakeStringSpliterator(ranges, ' ');
    std::string_view old_range_str(splitter());
    if (!absl::ConsumePrefix(&old_range_str, "-")) {
      return absl::InvalidArgumentError(absl::StrCat(
          "old-file range should start with '-', but got: ", old_range_str,
          "\"."));
    }
    std::string_view new_range_str(splitter());
    if (!absl::ConsumePrefix(&new_range_str, "+")) {
      return absl::InvalidArgumentError(absl::StrCat(
          "new-file range should start with '+', but got: ", new_range_str,
          "\"."));
    }
    RETURN_IF_ERROR(old_range.Parse(old_range_str));
    RETURN_IF_ERROR(new_range.Parse(new_range_str));
  }

  // Text that follows the last "@@" provides context and is optional.
  const std::string_view trailing_text = tokenizer(kDelimiter);
  context.assign(trailing_text.begin(), trailing_text.end());

  return absl::OkStatus();
}

std::ostream &operator<<(std::ostream &stream, const HunkHeader &header) {
  return term::Colored(
      stream,
      absl::StrCat("@@ -", header.old_range.FormatToString(), " +",
                   header.new_range.FormatToString(), " @@", header.context),
      term::Color::kGreen);
}

// Type M could be any container or range of MarkedLines.
template <class M>
static void CountMarkedLines(const M &lines, int *before, int *after) {
  *before = 0;
  *after = 0;
  for (const MarkedLine &line : lines) {
    switch (line.Marker()) {
      case ' ':  // line is common to both, unchanged
        ++*before;
        ++*after;
        break;
      case '-':
        ++*before;
        break;
      case '+':
        ++*after;
        break;
      default:
        break;
    }
  }
}

absl::Status Hunk::IsValid() const {
  int original_lines = 0;
  int new_lines = 0;
  CountMarkedLines(lines_, &original_lines, &new_lines);
  if (original_lines != header_.old_range.count) {
    return absl::InvalidArgumentError(
        absl::StrCat("Hunk is invalid: expected ", header_.old_range.count,
                     " lines before, but got ", original_lines, "."));
  }
  if (new_lines != header_.new_range.count) {
    return absl::InvalidArgumentError(
        absl::StrCat("Hunk is invalid: expected ", header_.new_range.count,
                     " lines after, but got ", new_lines, "."));
  }
  return absl::OkStatus();
}

void Hunk::UpdateHeader() {
  CountMarkedLines(lines_, &header_.old_range.count, &header_.new_range.count);
}

LineNumberSet Hunk::AddedLines() const {
  LineNumberSet line_numbers;
  int line_number = header_.new_range.start;
  for (const MarkedLine &line : lines_) {
    if (line.IsAdded()) line_numbers.Add(line_number);
    if (!line.IsDeleted()) ++line_number;
  }

  return line_numbers;
}

absl::Status Hunk::VerifyAgainstOriginalLines(
    const std::vector<std::string_view> &original_lines) const {
  int line_number = header_.old_range.start;  // 1-indexed
  for (const MarkedLine &line : lines_) {
    if (line.IsAdded()) continue;  // ignore added lines
    if (line_number > static_cast<int>(original_lines.size())) {
      return absl::OutOfRangeError(absl::StrCat(
          "Patch hunk references line ", line_number, " in a file with only ",
          original_lines.size(), " lines"));
    }
    const std::string_view original_line = original_lines[line_number - 1];
    if (line.Text() != original_line) {
      return absl::DataLossError(absl::StrCat(
          "Patch is inconsistent with original file!\nHunk at line ",
          line_number, " expected:\n", line.Text(), "\nbut got (original):\n",
          original_line, "\n"));
    }
    ++line_number;
  }
  return absl::OkStatus();
}

class HunkSplitter {
 public:
  HunkSplitter() = default;

  bool operator()(const MarkedLine &line) {
    if (previous_line_ == nullptr) {
      // first line
      previous_line_ = &line;
      return true;
    }
    const bool new_hunk = !previous_line_->IsCommon() && line.IsCommon();
    previous_line_ = &line;
    return new_hunk;
  }

 private:
  const MarkedLine *previous_line_ = nullptr;
};

std::vector<Hunk> Hunk::Split() const {
  std::vector<Hunk> sub_hunks;

  // Split after every group of changed lines.
  using MarkedLineIterator = std::vector<MarkedLine>::const_iterator;
  std::vector<MarkedLineIterator> cut_points;
  verible::find_all(lines_.begin(), lines_.end(),
                    std::back_inserter(cut_points), HunkSplitter());
  CHECK(!cut_points.empty());

  // Check whether or not there any line changes after the last cut point.
  // If not, then delete that cut point, which merges the last two partitions.
  if (cut_points.back() != lines_.begin()) {
    const bool last_partition_has_any_changes =
        std::any_of(cut_points.back(), lines_.end(),
                    [](const MarkedLine &m) { return !m.IsCommon(); });
    if (!last_partition_has_any_changes) cut_points.pop_back();
  }
  cut_points.push_back(lines_.end());  // always terminate partitions with end()
  // cut_points always contains lines_.begin(), and .end()

  // convert cut points to sub-ranges
  using MarkedLineRange = container_iterator_range<MarkedLineIterator>;
  const std::vector<MarkedLineRange> ranges(
      IteratorsToRanges<MarkedLineRange>(cut_points));

  // create sub hunks from each sub-range
  int old_starting_line = header_.old_range.start;
  int new_starting_line = header_.new_range.start;
  for (const MarkedLineRange &marked_line_range : ranges) {
    sub_hunks.emplace_back(old_starting_line, new_starting_line,
                           marked_line_range.begin(), marked_line_range.end());
    const HunkHeader &recent_header(sub_hunks.back().Header());
    old_starting_line += recent_header.old_range.count;
    new_starting_line += recent_header.new_range.count;
  }
  // TODO(b/161416776): if the resulting sub_hunks is singleton, then attempt to
  // further subdivide line-by-line.
  return sub_hunks;
}

absl::Status Hunk::Parse(const LineRange &hunk_lines) {
  RETURN_IF_ERROR(header_.Parse(hunk_lines.front()));

  LineRange body(hunk_lines);
  body.pop_front();  // remove the header
  lines_.resize(body.size());
  auto line_iter = lines_.begin();
  for (const auto &line : body) {
    RETURN_IF_ERROR(line_iter->Parse(line));
    ++line_iter;
  }
  return IsValid();
}

std::ostream &Hunk::Print(std::ostream &stream) const {
  stream << header_ << std::endl;
  for (const MarkedLine &line : lines_) {
    stream << line << std::endl;
  }
  return stream;
}

std::ostream &operator<<(std::ostream &stream, const Hunk &hunk) {
  return hunk.Print(stream);
}

absl::Status SourceInfo::Parse(std::string_view text) {
  StringSpliterator splitter(text);

  std::string_view token = splitter('\t');
  path.assign(token.begin(), token.end());
  if (path.empty()) {
    return absl::InvalidArgumentError(absl::StrCat(
        R"(Expected "path [timestamp]" (tab-separated), but got: ")", text,
        "\"."));
  }

  // timestamp is optional, allowed to be empty
  token = splitter('\t');  // time string (optional) is not parsed any further
  timestamp.assign(token.begin(), token.end());

  return absl::OkStatus();
}

std::ostream &operator<<(std::ostream &stream, const SourceInfo &info) {
  stream << info.path;
  if (!info.timestamp.empty()) stream << '\t' << info.timestamp;
  return stream;
}

static absl::Status ParseSourceInfoWithMarker(
    SourceInfo *info, std::string_view line, std::string_view expected_marker) {
  StringSpliterator splitter(line);
  std::string_view marker = splitter(' ');
  if (marker != expected_marker) {
    return absl::InvalidArgumentError(
        absl::StrCat("Expected old-file marker \"", expected_marker,
                     "\", but got: \"", marker, "\""));
  }
  return info->Parse(splitter.Remainder());
}

bool FilePatch::IsNewFile() const { return old_file_.path == "/dev/null"; }

bool FilePatch::IsDeletedFile() const { return new_file_.path == "/dev/null"; }

LineNumberSet FilePatch::AddedLines() const {
  LineNumberSet line_numbers;
  for (const Hunk &hunk : hunks_) {
    line_numbers.Union(hunk.AddedLines());
  }
  return line_numbers;
}

static char PromptHunkAction(std::istream &ins, std::ostream &outs) {
  // Suppress prompt in noninteractive mode.
  term::bold(outs, "Apply this hunk? [y,n,a,d,s,q,?] ");
  char c;
  ins >> c;  // user will need to hit <enter> after the character
  if (ins.eof()) {
    outs << "Reached end of user input, abandoning changes to this file and "
            "all remaining files."
         << std::endl;
    return 'q';
  }
  return c;
}

absl::Status FilePatch::VerifyAgainstOriginalLines(
    const std::vector<std::string_view> &original_lines) const {
  for (const Hunk &hunk : hunks_) {
    RETURN_IF_ERROR(hunk.VerifyAgainstOriginalLines(original_lines));
  }
  return absl::OkStatus();
}

absl::Status FilePatch::PickApplyInPlace(std::istream &ins,
                                         std::ostream &outs) const {
  return PickApply(ins, outs, &file::GetContentAsString, &file::SetContents);
}

absl::Status FilePatch::PickApply(std::istream &ins, std::ostream &outs,
                                  const FileReaderFunction &file_reader,
                                  const FileWriterFunction &file_writer) const {
  if (IsDeletedFile()) return absl::OkStatus();  // ignore
  if (IsNewFile()) return absl::OkStatus();      // ignore

  // Since this structure represents a patch, we need to retrieve the original
  // file's contents in whole.
  // If we had control over diff/patch generation, then we could rely on the
  // original diff structure/stream to provide original contents.
  // Below, we VerifyAgainstOriginalLines for all hunks in this FilePatch.
  const absl::StatusOr<std::string> orig_file_or = file_reader(old_file_.path);
  if (!orig_file_or.ok()) return orig_file_or.status();

  if (!hunks_.empty()) {
    // Display the file being processed, if there are any hunks.
    term::Colored(outs, absl::StrCat("--- ", old_file_.path, "\n"),
                  term::Color::kRed);
    term::Colored(outs, absl::StrCat("+++ ", new_file_.path, "\n"),
                  term::Color::kCyan);
  }

  const std::vector<std::string_view> orig_lines(SplitLines(*orig_file_or));
  RETURN_IF_ERROR(VerifyAgainstOriginalLines(orig_lines));

  // Accumulate lines to write here.
  // Needs own copy of string due to potential splitting into temporary hunks.
  std::vector<std::string> output_lines;

  int last_consumed_line = 0;                                     // 0-indexed
  std::deque<Hunk> hunks_worklist(hunks_.begin(), hunks_.end());  // copy-fill
  while (!hunks_worklist.empty()) {
    VLOG(1) << "hunks remaining: " << hunks_worklist.size();
    const Hunk &hunk(hunks_worklist.front());

    // Copy over unchanged lines before this hunk.
    const auto &old_range = hunk.Header().old_range;
    if (old_range.start < last_consumed_line) {
      return absl::InvalidArgumentError(
          absl::StrCat("Hunks are not properly ordered.  last_consumed_line=",
                       last_consumed_line, ", but current hunk starts at line ",
                       old_range.start));
    }
    for (; last_consumed_line < old_range.start - 1; ++last_consumed_line) {
      CHECK_LT(last_consumed_line, static_cast<int>(orig_lines.size()));
      const std::string_view &line(orig_lines[last_consumed_line]);
      output_lines.emplace_back(line.begin(), line.end());  // copy string
    }
    VLOG(1) << "copied up to (!including) line[" << last_consumed_line << "].";

    // Prompt user to apply or reject patch hunk.
    std::function<char()> prompt = [&hunk, &ins, &outs]() -> char {
      outs << hunk;
      return PromptHunkAction(ins, outs);
    };
    const char action = prompt();
    VLOG(1) << "user entered: " << action;
    switch (action) {
      case 'a': {
        // accept all remaining hunks in the current file
        prompt = []() -> char { return 'y'; };  // suppress prompt
        ABSL_FALLTHROUGH_INTENDED;
      }
      case 'y': {
        // accept this hunk, copy lines over
        for (const MarkedLine &marked_line : hunk.MarkedLines()) {
          if (!marked_line.IsDeleted()) {
            const std::string_view line(marked_line.Text());
            output_lines.emplace_back(line.begin(), line.end());  // copy string
          }
        }
        last_consumed_line = old_range.start + old_range.count - 1;
        break;  // switch
      }
      case 'd': {
        // reject all remaining hunks in the current file
        prompt = []() -> char { return 'n'; };  // suppress prompt
        ABSL_FALLTHROUGH_INTENDED;
      }
      case 'n':
        // reject this hunk
        // no need to do anything, next iteration will sweep up original lines
        break;  // switch
      case 's': {
        // split this hunk into smaller ones and prompt again
        const auto sub_hunks = hunk.Split();
        hunks_worklist.pop_front();  // replace current hunk with sub-hunks
        // maintain sequential order of sub-hunks in the queue by line number
        for (const auto &sub_hunk : verible::reversed_view(sub_hunks)) {
          hunks_worklist.push_front(sub_hunk);
        }
        continue;  // for-loop
      }
      // TODO(b/156530527): 'e' for hunk editing
      case 'q':
        // Abort this file, discard any elected edits.
        outs << "Leaving file " << old_file_.path << " unchanged." << std::endl;
        return absl::OkStatus();
      default:  // including '?'
        outs << "y - accept change\n"
                "n - reject change\n"
                "a - accept this and remaining changes in the current file\n"
                "d - reject this and remaining changes in the current file\n"
                "s - split this hunk into smaller ones and prompt each one\n"
                "q - abandon all changes in this file\n"
                "? - print this help and prompt again\n";
        continue;  // for-loop
    }

    hunks_worklist.pop_front();
  }
  // Copy over remaining lines after the last hunk.
  for (; last_consumed_line < static_cast<int>(orig_lines.size());
       ++last_consumed_line) {
    const std::string_view &line(orig_lines[last_consumed_line]);
    output_lines.emplace_back(line.begin(), line.end());  // copy string
  }
  VLOG(1) << "copied reamining lines up to [" << last_consumed_line << "].";

  const std::string rewrite_contents(absl::StrJoin(output_lines, "\n") + "\n");

  return file_writer(old_file_.path, rewrite_contents);
}

absl::Status FilePatch::Parse(const LineRange &lines) {
  LineIterator line_iter(
      std::find_if(lines.begin(), lines.end(), &LineMarksOldFile));
  if (lines.begin() == lines.end() || line_iter == lines.end()) {
    return absl::InvalidArgumentError(
        "Expected a file marker starting with \"---\", but did not find one.");
  }
  // Lines leading up to the old file marker "---" are metadata.
  for (const auto &line : make_range(lines.begin(), line_iter)) {
    metadata_.emplace_back(line);
  }

  RETURN_IF_ERROR(ParseSourceInfoWithMarker(&old_file_, *line_iter, "---"));
  ++line_iter;
  if (line_iter == lines.end()) {
    return absl::InvalidArgumentError(
        "Expected a file marker starting with \"+++\", but did not find one.");
  }
  RETURN_IF_ERROR(ParseSourceInfoWithMarker(&new_file_, *line_iter, "+++"));

  ++line_iter;

  // find hunk starts, and parse ranges of hunk texts
  std::vector<LineIterator> hunk_starts;
  find_all(line_iter, lines.end(), std::back_inserter(hunk_starts),
           [](std::string_view line) { return absl::StartsWith(line, "@@ "); });

  if (hunk_starts.empty()) {
    // Unusual, but degenerate case of no hunks is parseable and valid.
    return absl::OkStatus();
  }

  hunk_starts.push_back(lines.end());  // make it easier to construct ranges
  const std::vector<LineRange> hunk_ranges(
      IteratorsToRanges<LineRange>(hunk_starts));

  hunks_.resize(hunk_ranges.size());
  auto hunk_iter = hunks_.begin();
  for (const auto &hunk_range : hunk_ranges) {
    RETURN_IF_ERROR(hunk_iter->Parse(hunk_range));
    ++hunk_iter;
  }
  return absl::OkStatus();
}

std::ostream &FilePatch::Print(std::ostream &stream) const {
  for (const std::string &line : metadata_) {
    stream << line << std::endl;
  }
  stream << "--- " << old_file_ << '\n'  //
         << "+++ " << new_file_ << std::endl;
  for (const Hunk &hunk : hunks_) {
    stream << hunk;
  }
  return stream;
}

std::ostream &operator<<(std::ostream &stream, const FilePatch &patch) {
  return patch.Print(stream);
}

}  // namespace internal

static bool LineBelongsToPreviousSection(std::string_view line) {
  if (line.empty()) return true;
  return IsValidMarkedLine(line);
}

absl::Status PatchSet::Parse(std::string_view patch_contents) {
  // Split lines.  The resulting lines will not include the \n delimiters.
  std::vector<std::string_view> lines(
      absl::StrSplit(patch_contents, absl::ByChar('\n')));

  // Consider an empty patch file valid.
  if (lines.empty()) return absl::OkStatus();

  // Well-formed files end with a newline [POSIX], so delete the last partition.
  internal::LineRange lines_range(lines.cbegin(), std::prev(lines.cend()));

  // Split set of lines into ranges that correspond to individual files.
  // Strategy: find all old-file lines that start with "--- ", and then
  // search backwards to find the last line that starts with one of [ +-].
  std::vector<internal::LineIterator> file_patch_begins;
  {
    find_all(lines_range.begin(), lines_range.end(),
             std::back_inserter(file_patch_begins), &LineMarksOldFile);
    if (file_patch_begins.empty()) return absl::OkStatus();

    // Move line iterators back to correct starting points.
    for (auto &iter : file_patch_begins) {
      while (iter != lines_range.begin()) {
        const auto prev = std::prev(iter);
        const std::string_view &peek(*prev);
        if (LineBelongsToPreviousSection(peek)) break;
        iter = prev;
      }
    }

    // For easier construction of ranges, append an end() iterator.
    file_patch_begins.push_back(lines_range.end());
  }

  // Record metadata lines, if there are any.
  for (const auto &line :
       make_range(lines_range.begin(), file_patch_begins.front())) {
    metadata_.emplace_back(line);
  }

  // Parse individual file patches.
  const std::vector<internal::LineRange> file_patch_ranges(
      internal::IteratorsToRanges<internal::LineRange>(file_patch_begins));
  file_patches_.resize(file_patch_ranges.size());
  auto iter = file_patches_.begin();
  for (const auto &range : file_patch_ranges) {
    RETURN_IF_ERROR(iter->Parse(range));
    ++iter;
  }

  // TODO(fangism): pass around line numbers to include in diagnostics

  return absl::OkStatus();
}

std::ostream &PatchSet::Render(std::ostream &stream) const {
  for (const auto &line : metadata_) {
    stream << line << std::endl;
  }
  for (const internal::FilePatch &file_patch : file_patches_) {
    stream << file_patch;
  }
  return stream;
}

FileLineNumbersMap PatchSet::AddedLinesMap(bool new_file_ranges) const {
  FileLineNumbersMap result;
  for (const internal::FilePatch &file_patch : file_patches_) {
    if (file_patch.IsDeletedFile()) continue;
    LineNumberSet &entry = result[file_patch.NewFileInfo().path];
    if (file_patch.IsNewFile() && !new_file_ranges) {
      entry.clear();
    } else {
      entry = file_patch.AddedLines();
    }
  }
  return result;
}

absl::Status PatchSet::PickApplyInPlace(std::istream &ins,
                                        std::ostream &outs) const {
  return PickApply(ins, outs, &file::GetContentAsString, &file::SetContents);
}

absl::Status PatchSet::PickApply(
    std::istream &ins, std::ostream &outs,
    const internal::FileReaderFunction &file_reader,
    const internal::FileWriterFunction &file_writer) const {
  for (const internal::FilePatch &file_patch : file_patches_) {
    RETURN_IF_ERROR(file_patch.PickApply(ins, outs, file_reader, file_writer));
  }
  return absl::OkStatus();
}

std::ostream &operator<<(std::ostream &stream, const PatchSet &patch) {
  return patch.Render(stream);
}

}  // namespace verible
