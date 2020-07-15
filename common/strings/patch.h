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
#include "absl/strings/string_view.h"
#include "common/strings/compare.h"
#include "common/strings/position.h"
#include "common/util/container_iterator_range.h"

namespace verible {
namespace internal {
// Forward declarations
struct FilePatch;

// function interface like file::GetContents()
using FileReaderFunction = std::function<absl::Status(
    absl::string_view filename, std::string* contents)>;

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
  std::ostream& Render(std::ostream& stream) const;

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
  absl::Status PickApplyInPlace(std::istream& ins, std::ostream& outs) const;

 protected:
  // For testing, allow mocking out of file I/O.
  absl::Status PickApply(std::istream& ins, std::ostream& outs,
                         const internal::FileReaderFunction& file_reader,
                         const internal::FileWriterFunction& file_writer) const;

 private:
  // Non-patch plain text that could describe the origins of the diff/patch,
  // e.g. from git-format-patch.
  std::vector<std::string> metadata_;

  // Collection of file differences.
  // Any metadata for the entire patch set will be lumped into the first file's
  // metadata.
  std::vector<internal::FilePatch> file_patches_;
};

std::ostream& operator<<(std::ostream&, const PatchSet&);

// Private implementation details follow.
namespace internal {

using LineIterator = std::vector<absl::string_view>::const_iterator;
using LineRange = container_iterator_range<LineIterator>;

// A single line of a patch hunk.
struct MarkedLine {
  std::string line;

  // The first column denotes whether a line is:
  // ' ': common context
  // '-': only in the left/old file
  // '+': only in the right/new file
  char Marker() const { return line[0]; }

  absl::string_view Text() const {
    return absl::string_view(&line[1], line.length() - 1);
  }

  absl::Status Parse(absl::string_view);
};

std::ostream& operator<<(std::ostream&, const MarkedLine&);

struct HunkIndices {
  int start;  // starting line number for a hunk, 1-based
  int count;  // number of lines to expect in this hunk

  absl::Status Parse(absl::string_view);
};

std::ostream& operator<<(std::ostream&, const HunkIndices&);

struct HunkHeader {
  HunkIndices old_range;
  HunkIndices new_range;
  // Some diff tools also include context such as the function or class
  // declaration that encloses this hunk.  This is optional metadata.
  std::string context;

  absl::Status Parse(absl::string_view);
};

std::ostream& operator<<(std::ostream&, const HunkHeader&);

// One unit of a file change.
class Hunk {
 public:
  Hunk() = default;

  // Hunk is valid if its header's line counts are consistent with the set of
  // MarkedLines.
  absl::Status IsValid() const;

  const HunkHeader& Header() const { return header_; }

  const std::vector<MarkedLine>& MarkedLines() const { return lines_; }

  // If a hunk is modified for any reason, the number of added/removed lines may
  // have changed, so this will update the .count values.
  void UpdateHeader();

  // Returns a set of line numbers for lines that are changed or new.
  LineNumberSet AddedLines() const;

  absl::Status Parse(const LineRange&);

  std::ostream& Print(std::ostream&) const;

  // Verify consistency of lines in the patch (old-file) against the file that
  // is read in whole.
  absl::Status VerifyAgainstOriginalLines(
      const std::vector<absl::string_view>& original_lines) const;

 private:
  // The header describes how many of each type of edit lines to expect.
  HunkHeader header_;

  // Sequence of edit lines (common, old, new).
  std::vector<MarkedLine> lines_;
};

std::ostream& operator<<(std::ostream&, const Hunk&);

struct SourceInfo {
  std::string path;       // location to patched file, absolute or relative
  std::string timestamp;  // unspecified date format, not parsed, optional

  absl::Status Parse(absl::string_view);
};

std::ostream& operator<<(std::ostream&, const SourceInfo&);

// Set of changes for a single file.
struct FilePatch {
  // These are lines of informational text only, such as how the diff was
  // generated.  They do not impact 'patch' behavior.
  std::vector<std::string> metadata;
  SourceInfo old_file;
  SourceInfo new_file;
  std::vector<Hunk> hunks;

  // Returns true if this file is new.
  bool IsNewFile() const;

  // Returns true if this file is deleted.
  bool IsDeletedFile() const;

  // Returns a set of line numbers for lines that are changed or new.
  LineNumberSet AddedLines() const;

  absl::Status Parse(const LineRange&);

  // Verify consistency of lines in the patch (old-file) against the file that
  // is read in whole.
  absl::Status VerifyAgainstOriginalLines(
      const std::vector<absl::string_view>& original_lines) const;

  absl::Status PickApplyInPlace(std::istream& ins, std::ostream& outs) const;

  // For testing with mocked-out file I/O.
  // Public to allow use by PatchSet::PickApply().
  absl::Status PickApply(std::istream& ins, std::ostream& outs,
                         const FileReaderFunction& file_reader,
                         const FileWriterFunction& file_writer) const;
};

std::ostream& operator<<(std::ostream&, const FilePatch&);

}  // namespace internal

}  // namespace verible

#endif  // VERIBLE_COMMON_STRINGS_PATCH_H_
