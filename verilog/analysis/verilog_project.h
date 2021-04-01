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

#ifndef VERIBLE_VERILOG_ANALYSIS_VERILOG_PROJECT_H_
#define VERIBLE_VERILOG_ANALYSIS_VERILOG_PROJECT_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/strings/string_memory_map.h"
#include "common/text/text_structure.h"
#include "verilog/analysis/verilog_analyzer.h"

namespace verilog {

class VerilogProject;

// A read-only view of a single Verilog source file.
class VerilogSourceFile {
 public:
  VerilogSourceFile(absl::string_view referenced_path,
                    absl::string_view resolved_path, absl::string_view corpus)
      : referenced_path_(referenced_path),
        resolved_path_(resolved_path),
        corpus_(corpus) {}

  // When a file is not found among a set of paths, remember it with an
  // error status.
  VerilogSourceFile(absl::string_view referenced_path, absl::Status status);

  VerilogSourceFile(const VerilogSourceFile&) = delete;
  VerilogSourceFile& operator=(const VerilogSourceFile&) = delete;

  VerilogSourceFile(VerilogSourceFile&&) =
      default;  // need for std::map::emplace
  VerilogSourceFile& operator=(VerilogSourceFile&&) = delete;

  virtual ~VerilogSourceFile() = default;

  // Opens a file using the resolved path and loads the contents into memory.
  // This does not attempt to parse/analyze the contents.
  virtual absl::Status Open();

  // Attempts to lex and parse the file (without preprocessing).
  // Will Open() if the file is not already opened.
  // Depending on context, not all files are suitable for standalone parsing.
  absl::Status Parse();

  // After Open(), the underlying text structure contains at least the file's
  // contents.  After Parse(), it may contain other analyzed structural forms.
  // Before Open(), this returns nullptr.
  const verible::TextStructureView* GetTextStructure() const;

  // Returns the first non-Ok status if there is one, else OkStatus().
  absl::Status Status() const { return status_; }

  // Returns the name used to reference the file.
  absl::string_view ReferencedPath() const { return referenced_path_; }

  // Returns the corpus to which this file belongs (e.g.,
  // github.com/google/verible).
  absl::string_view Corpus() const { return corpus_; }

  // Returns a (possibly more qualified) path to the file.
  absl::string_view ResolvedPath() const { return resolved_path_; }

  // Comparator for ordering files for internal storage.
  // Known limitation: this comparator won't work if you have multiple files
  // with the same name referenced without a distinguishing path prefix.
  struct Less {
    using is_transparent = void;  // hetergenous compare

    static absl::string_view to_string_view(absl::string_view s) { return s; }
    static absl::string_view to_string_view(const VerilogSourceFile& f) {
      return f.ReferencedPath();
    }
    static absl::string_view to_string_view(const VerilogSourceFile* f) {
      return f->ReferencedPath();
    }

    // T1/T2 could be any combination of:
    // {const VerilogSourceFile&, absl::string_view}.
    template <typename T1, typename T2>
    bool operator()(T1 left, T2 right) const {
      return to_string_view(left) < to_string_view(right);
    }
  };

 private:
  friend class VerilogProject;

 protected:
  // Tracking state for linear progression of analysis, which allows
  // prerequisite actions to be cached.
  enum State {
    // Only the paths have been established.
    kInitialized,

    // Files have been opened and contents loaded.
    kOpened,

    // Parse() as at least attempted.
    // Lexical and syntax tree structures may be available.
    kParsed,
  };

  // This is the how the file is referenced either in a file list or `include.
  // This should be const for the lifetime of this object, but isn't declared as
  // such so this class can have a default move constructor.
  std::string referenced_path_;

  // Often a concatenation of a base path with a relative path.
  // This should be const for the lifetime of this object, but isn't declared as
  // such so this class can have a default move constructor.
  std::string resolved_path_;

  // The corpus to which this file belongs to (e.g., github.com/google/verible).
  absl::string_view corpus_;

  // State of this file.
  State state_ = State::kInitialized;

  // Holds any diagostics for problems encountered finding/reading this file.
  absl::Status status_;

  // Holds the file's string contents in owned memory, along with other forms
  // like token streams and syntax tree.
  std::unique_ptr<VerilogAnalyzer> analyzed_structure_;
};

// Printable representation for debugging.
std::ostream& operator<<(std::ostream&, const VerilogSourceFile&);

// An in-memory source file that doesn't require file-system access,
// nor create temporary files.
class InMemoryVerilogSourceFile : public VerilogSourceFile {
 public:
  // filename can be fake, it is not used to open any file.
  InMemoryVerilogSourceFile(absl::string_view filename,
                            absl::string_view contents,
                            absl::string_view corpus = "")
      : VerilogSourceFile(filename, filename, corpus),
        contents_for_open_(contents) {}

  // Load text into analyzer structure without actually opening a file.
  absl::Status Open() override;

 private:
  const absl::string_view contents_for_open_;
};

// VerilogProject represents a set of files as a cohesive unit of compilation.
// Files can include top-level translation units and preprocessor included
// files. This is responsible for owning string memory that corresponds
// to files' contents.
class VerilogProject {
  // Collection of per-file metadata and analyzer objects
  // key: referenced file name (as opposed to resolved filename)
  typedef std::map<std::string, std::unique_ptr<VerilogSourceFile>,
                   VerilogSourceFile::Less>
      file_set_type;

 public:
  typedef file_set_type::iterator iterator;
  typedef file_set_type::const_iterator const_iterator;

 public:
  VerilogProject(absl::string_view root,
                 const std::vector<std::string>& include_paths,
                 absl::string_view corpus = "")
      : translation_unit_root_(root),
        include_paths_(include_paths),
        corpus_(corpus) {}

  VerilogProject(const VerilogProject&) = delete;
  VerilogProject(VerilogProject&&) = delete;
  VerilogProject& operator=(const VerilogProject&) = delete;
  VerilogProject& operator=(VerilogProject&&) = delete;

  const_iterator begin() const { return files_.begin(); }
  iterator begin() { return files_.begin(); }
  const_iterator end() const { return files_.end(); }
  iterator end() { return files_.end(); }

  // Returns the directory to which translation units are referenced relatively.
  absl::string_view TranslationUnitRoot() const {
    return translation_unit_root_;
  }

  // Returns the corpus to which this project belongs to.
  absl::string_view Corpus() const { return corpus_; }

  // Opens a single top-level file, known as a "translation unit".
  // This uses translation_unit_root_ directory to calculate the file's path.
  // If the file was previously opened, that data is returned.
  absl::StatusOr<VerilogSourceFile*> OpenTranslationUnit(
      absl::string_view referenced_filename);

  // Opens a file that was `included.
  // If the file was previously opened, that data is returned.
  absl::StatusOr<VerilogSourceFile*> OpenIncludedFile(
      absl::string_view referenced_filename);

  // Adds an already opened file by directly passing its content.
  void AddVirtualFile(absl::string_view referenced_filename,
                      absl::string_view content);

  // Returns a collection of non-ok diagnostics for the entire project.
  std::vector<absl::Status> GetErrorStatuses() const;

  // Returns a previously referenced file, or else nullptr.
  VerilogSourceFile* LookupRegisteredFile(
      absl::string_view referenced_filename) {
    const auto found = files_.find(referenced_filename);
    if (found == files_.end()) return nullptr;
    return found->second.get();
  }

  // Non-modifying variant of lookup.
  const VerilogSourceFile* LookupRegisteredFile(
      absl::string_view referenced_filename) const {
    const auto found = files_.find(referenced_filename);
    if (found == files_.end()) return nullptr;
    return found->second.get();
  }

  // Find the source file that a particular string_view came from.
  // Returns nullptr if lookup failed for any reason.
  const VerilogSourceFile* LookupFileOrigin(
      absl::string_view content_substring) const;

 private:
  absl::StatusOr<VerilogSourceFile*> OpenFile(
      absl::string_view referenced_filename,
      absl::string_view resolved_filename, absl::string_view corpus);

  // Error status factory, when include file is not found.
  absl::Status IncludeFileNotFoundError(
      absl::string_view referenced_filename) const;

 private:
  // The path from which top-level translation units are referenced relatively
  // (often from a file list).  This path can be relative or absolute.
  // Default: the working directory of the invoking process.
  const std::string translation_unit_root_ = ".";

  // The sequence of directories from which to search for `included files.
  // These can be absolute, or relative to the process's working directory.
  const std::vector<std::string> include_paths_;

  // The corpus to which this project belongs (e.g.,
  // 'github.com/google/verible').
  const std::string corpus_;

  // Set of opened files, keyed by referenced (not resolved) filename.
  file_set_type files_;

  // Maps any string_view (substring) to its full source file text
  // (superstring).
  verible::StringViewSuperRangeMap string_view_map_;

  // Maps start of text buffer to its corresponding analyzer object.
  // key: the starting address of a string buffer belonging to an opened file.
  //   This can come from the .begin() of any entry in string_view_map_.
  std::map<absl::string_view::const_iterator, file_set_type::const_iterator>
      buffer_to_analyzer_map_;
};

// Reads in a list of files line-by-line from 'file_list_file'.
absl::StatusOr<std::vector<std::string>> ParseSourceFileListFromFile(
    absl::string_view file_list_file);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_VERILOG_PROJECT_H_
