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

#ifndef VERIBLE_COMMON_STRINGS_PATCH_H_
#define VERIBLE_COMMON_STRINGS_PATCH_H_

#include <functional>
#include <iosfwd>
#include <map>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "verible/common/strings/compare.h"
#include "verible/common/strings/position.h"
#include "verible/common/util/container-iterator-range.h"
#include "verible/common/util/logging.h"

namespace verible {
namespace internal {
// Forward declarations
class FilePatch;

// function interface like file::GetContentAsString()
using FileReaderFunction =
    std::function<absl::StatusOr<std::string>(absl::string_view filename)>;

// function interface like file::SetContents()
using FileWriterFunction = std::function<absl::Status(
    absl::string_view filename, absl::string_view contents)>;
}  // namespace internal

using FileLineNumbersMap =
    std::map<std::string, LineNumberSet, StringViewCompare>;

// Collection of file changes.
// Recursively inside these structs, we chose to copy std::string (owned memory)
// instead of string_views into file contents memory kept separately.
// This lets one safely modify patch structures.
class PatchSet {
 public:
  PatchSet() = default;

  // Parse a unified-diff patch file into internal representation.
  absl::Status Parse(absl::string_view patch_contents);

  // Prints a unified-diff formatted output.
  std::ostream &Render(std::ostream &stream) const;

  // Returns a map<filename, line_numbers> that indicates which lines in each
  // file are new (in patch files, these are hunk lines starting with '+').
  // New and existing files will always have entries in the returned map,
  // while deleted files will not.
  // If `new_file_ranges` is true, provide the full range of lines for new
  // files, otherwise leave their corresponding LineNumberSets empty.
  FileLineNumbersMap AddedLinesMap(bool new_file_ranges) const;

  // Interactively prompt user to select hunks to apply in-place.
  // 'ins' is the stream from which user-input is read,
  // and 'outs' is the stream that displays text and prompts to the user.
  absl::Status PickApplyInPlace(std::istream &ins, std::ostream &outs) const;

 protected:
  // For testing, allow mocking out of file I/O.
  absl::Status PickApply(std::istream &ins, std::ostream &outs,
                         const internal::FileReaderFunction &file_reader,
                         const internal::FileWriterFunction &file_writer) const;

 private:
  // Non-patch plain text that could describe the origins of the diff/patch,
  // e.g. from git-format-patch.
  std::vector<std::string> metadata_;

  // Collection of file differences.
  // Any metadata for the entire patch set will be lumped into the first file's
  // metadata.
  std::vector<internal::FilePatch> file_patches_;
};

std::ostream &operator<<(std::ostream &, const PatchSet &);

// Private implementation details follow.
namespace internal {

using LineIterator = std::vector<absl::string_view>::const_iterator;
using LineRange = container_iterator_range<LineIterator>;

// A single line of a patch hunk.
struct MarkedLine {
  std::string line;

  MarkedLine() = default;

  // only used in manual test case construction
  // so ok to use CHECK here.
  explicit MarkedLine(absl::string_view text) : line(text.begin(), text.end()) {
    CHECK(!line.empty()) << "MarkedLine must start with a marker in [ -+].";
    CHECK(Valid()) << "Unexpected marker '" << Marker() << "'.";
  }

  bool Valid() const {
    const char m = Marker();
    return m == ' ' || m == '-' || m == '+';
  }

  // The first column denotes whether a line is:
  // ' ': common context
  // '-': only in the left/old file
  // '+': only in the right/new file
  char Marker() const { return line[0]; }

  bool IsCommon() const { return Marker() == ' '; }
  bool IsAdded() const { return Marker() == '+'; }
  bool IsDeleted() const { return Marker() == '-'; }

  absl::string_view Text() const { return {&line[1], line.length() - 1}; }

  // default equality operator
  bool operator==(const MarkedLine &other) const { return line == other.line; }
  bool operator!=(const MarkedLine &other) const { return !(*this == other); }

  absl::Status Parse(absl::string_view);
};

std::ostream &operator<<(std::ostream &, const MarkedLine &);

struct HunkIndices {
  int start;  // starting line number for a hunk, 1-based
  int count;  // number of lines to expect in this hunk

  // default equality operator
  bool operator==(const HunkIndices &other) const {
    return start == other.start && count == other.count;
  }
  bool operator!=(const HunkIndices &other) const { return !(*this == other); }

  std::string FormatToString() const;

  absl::Status Parse(absl::string_view);
};

std::ostream &operator<<(std::ostream &, const HunkIndices &);

struct HunkHeader {
  HunkIndices old_range;
  HunkIndices new_range;
  // Some diff tools also include context such as the function or class
  // declaration that encloses this hunk.  This is optional metadata.
  std::string context;

  // default equality operator
  bool operator==(const HunkHeader &other) const {
    return old_range == other.old_range && new_range == other.new_range &&
           context == other.context;
  }
  bool operator!=(const HunkHeader &other) const { return !(*this == other); }

  absl::Status Parse(absl::string_view);
};

std::ostream &operator<<(std::ostream &, const HunkHeader &);

// One unit of a file change.
class Hunk {
 public:
  Hunk() = default;
  Hunk(const Hunk &) = default;

  template <typename Iter>
  Hunk(int old_starting_line, int new_starting_line, Iter begin, Iter end)
      : header_{.old_range = {old_starting_line, 0},
                .new_range = {new_starting_line, 0}},
        lines_(begin, end) /* copy */ {
    UpdateHeader();  // automatically count marked lines
  }

  // Hunk is valid if its header's line counts are consistent with the set of
  // MarkedLines.
  absl::Status IsValid() const;

  const HunkHeader &Header() const { return header_; }

  const std::vector<MarkedLine> &MarkedLines() const { return lines_; }

  // If a hunk is modified for any reason, the number of added/removed lines may
  // have changed, so this will update the .count values.
  void UpdateHeader();

  // Returns a set of line numbers for lines that are changed or new.
  LineNumberSet AddedLines() const;

  absl::Status Parse(const LineRange &);

  std::ostream &Print(std::ostream &) const;

  // Verify consistency of lines in the patch (old-file) against the file that
  // is read in whole.
  absl::Status VerifyAgainstOriginalLines(
      const std::vector<absl::string_view> &original_lines) const;

  // default structural comparison
  bool operator==(const Hunk &other) const {
    return header_ == other.header_ && lines_ == other.lines_;
  }
  bool operator!=(const Hunk &other) const { return !(*this == other); }

  // Splits this Hunk into smaller ones at the points of common context.
  // Returns array of hunks.  If this hunk can no longer be split, then it is
  // returned as a singleton in the array.
  std::vector<Hunk> Split() const;

 private:
  // The header describes how many of each type of edit lines to expect.
  HunkHeader header_;

  // Sequence of edit lines (common, old, new).
  std::vector<MarkedLine> lines_;
};

std::ostream &operator<<(std::ostream &, const Hunk &);

struct SourceInfo {
  std::string path;       // location to patched file, absolute or relative
  std::string timestamp;  // unspecified date format, not parsed, optional

  absl::Status Parse(absl::string_view);
};

std::ostream &operator<<(std::ostream &, const SourceInfo &);

// Set of changes for a single file.
class FilePatch {
 public:
  // Initialize data struct from a range of patch lines.
  absl::Status Parse(const LineRange &);

  // Returns true if this file is new.
  bool IsNewFile() const;

  // Returns true if this file is deleted.
  bool IsDeletedFile() const;

  const SourceInfo &NewFileInfo() const { return new_file_; }

  // Returns a set of line numbers for lines that are changed or new.
  LineNumberSet AddedLines() const;

  std::ostream &Print(std::ostream &) const;

  // Verify consistency of lines in the patch (old-file) against the file that
  // is read in whole.
  absl::Status VerifyAgainstOriginalLines(
      const std::vector<absl::string_view> &original_lines) const;

  absl::Status PickApplyInPlace(std::istream &ins, std::ostream &outs) const;

  // For testing with mocked-out file I/O.
  // Public to allow use by PatchSet::PickApply().
  absl::Status PickApply(std::istream &ins, std::ostream &outs,
                         const FileReaderFunction &file_reader,
                         const FileWriterFunction &file_writer) const;

 private:
  // These are lines of informational text only, such as how the diff was
  // generated.  They do not impact 'patch' behavior.
  std::vector<std::string> metadata_;
  SourceInfo old_file_;
  SourceInfo new_file_;
  std::vector<Hunk> hunks_;
};

std::ostream &operator<<(std::ostream &, const FilePatch &);

}  // namespace internal

}  // namespace verible

#endif  // VERIBLE_COMMON_STRINGS_PATCH_H_
