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

#include <iosfwd>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "common/util/container_iterator_range.h"

namespace verible {
namespace internal {
// Forward declarations
struct FilePatch;
}  // namespace internal

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

  // TODO(b/158630746): return LineNumberSet for --lines for incremental
  // operations, replacing diff-to-changed-lines.awk.

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
struct Hunk {
  HunkHeader header;
  std::vector<MarkedLine> lines;

  // Hunk is valid if its header's line counts are consistent with the set of
  // MarkedLines.
  absl::Status IsValid() const;

  // If a hunk is modified for any reason, the number of added/removed lines may
  // have changed, so this will update the .count values.
  void UpdateHeader();

  absl::Status Parse(const LineRange&);
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

  // TODO: LineNumberSet AddedLines() const; // for --lines

  absl::Status Parse(const LineRange&);
};

std::ostream& operator<<(std::ostream&, const FilePatch&);

}  // namespace internal

}  // namespace verible

#endif  // VERIBLE_COMMON_STRINGS_PATCH_H_
