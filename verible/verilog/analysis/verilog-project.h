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

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "verible/common/strings/mem-block.h"
#include "verible/common/strings/string-memory-map.h"
#include "verible/common/text/text-structure.h"
#include "verible/verilog/analysis/verilog-analyzer.h"

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
  VerilogSourceFile(absl::string_view referenced_path,
                    const absl::Status &status);

  VerilogSourceFile(const VerilogSourceFile &) = delete;
  VerilogSourceFile &operator=(const VerilogSourceFile &) = delete;

  VerilogSourceFile(VerilogSourceFile &&) =
      default;  // need for std::map::emplace
  VerilogSourceFile &operator=(VerilogSourceFile &&) = delete;

  virtual ~VerilogSourceFile() = default;

  // Opens a file using the resolved path and loads the contents into memory.
  // This does not attempt to parse/analyze the contents.
  virtual absl::Status Open();

  // After successful Open(), the content is filled; empty otherwise.
  virtual absl::string_view GetContent() const;

  // Attempts to lex and parse the file.
  // Will Open() if the file is not already opened.
  // Depending on context, not all files are suitable for standalone parsing.
  virtual absl::Status Parse();

  // Return if Parse() has been called and content has been parsed.
  // (see Status() if it was actually successful).
  bool is_parsed() const {
    return processing_state_ >= ProcessingState::kParsed;
  }

  // After Parse(), text structure may contain other analyzed structural forms.
  // Before successful Parse(), this is not initialized and returns nullptr.
  virtual const verible::TextStructureView *GetTextStructure() const;

  // Returns the first non-Ok status if there is one, else OkStatus().
  absl::Status Status() const { return status_; }

  // Return human readable error messages if available.
  std::vector<std::string> ErrorMessages() const;

  // Returns the name used to reference the file.
  absl::string_view ReferencedPath() const { return referenced_path_; }

  // Returns the corpus to which this file belongs (e.g.,
  // github.com/chipsalliance/verible).
  absl::string_view Corpus() const { return corpus_; }

  // Returns a (possibly more qualified) path to the file.
  absl::string_view ResolvedPath() const { return resolved_path_; }

  // Comparator for ordering files for internal storage.
  // Known limitation: this comparator won't work if you have multiple files
  // with the same name referenced without a distinguishing path prefix.
  struct Less {
    using is_transparent = void;  // hetergenous compare

    static absl::string_view to_string_view(absl::string_view s) { return s; }
    static absl::string_view to_string_view(const VerilogSourceFile &f) {
      return f.ReferencedPath();
    }
    static absl::string_view to_string_view(const VerilogSourceFile *f) {
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
  enum class ProcessingState {
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
  const std::string referenced_path_;

  // Often a concatenation of a base path with a relative path.
  // This should be const for the lifetime of this object, but isn't declared as
  // such so this class can have a default move constructor.
  const std::string resolved_path_;

  // The corpus to which this file belongs to (e.g.,
  // github.com/chipsalliance/verible).
  const absl::string_view corpus_;

  // Linear progression of analysis.
  ProcessingState processing_state_ = ProcessingState::kInitialized;

  // Holds any diagostics for problems encountered finding/reading this file.
  absl::Status status_;

  // MemBock holding the file content so that it can be used in other contexts.
  std::shared_ptr<verible::MemBlock> content_;

  // Contains token streams and syntax tree after Parse().
  std::unique_ptr<VerilogAnalyzer> analyzed_structure_;
};

// Printable representation for debugging.
std::ostream &operator<<(std::ostream &, const VerilogSourceFile &);

// An in-memory source file that doesn't require file-system access,
// nor create temporary files.
class InMemoryVerilogSourceFile final : public VerilogSourceFile {
 public:
  // filename can be fake, it is not used to open any file.
  InMemoryVerilogSourceFile(absl::string_view filename,
                            std::shared_ptr<verible::MemBlock> content,
                            absl::string_view corpus = "")
      : VerilogSourceFile(filename, filename, corpus) {
    content_ = std::move(content);
    processing_state_ = ProcessingState::kOpened;  // Advance state
    status_ = absl::OkStatus();
  }

  // Legacy
  InMemoryVerilogSourceFile(absl::string_view filename,
                            absl::string_view contents,
                            absl::string_view corpus = "")
      : InMemoryVerilogSourceFile(
            filename, std::make_shared<verible::StringMemBlock>(contents),
            corpus) {}

  // Nothing to do, content already loaded.
  absl::Status Open() final { return absl::OkStatus(); }
};

// Source file that was already parsed.
// Doesn't require file-system access, nor create temporary files.
// TODO: reorg class hierachy. The base class contains an owened analyzed
// structure; simplify to only track ownership in one place.
class ParsedVerilogSourceFile final : public VerilogSourceFile {
 public:
  // Construct with an already existing VerilogAnalyzer that already
  // parsed its content.
  // Ownership if "analyzer" is not taken over, must outlive this objet.
  ParsedVerilogSourceFile(absl::string_view referenced_path,
                          absl::string_view resolved_path,
                          const verilog::VerilogAnalyzer &analyzer,
                          absl::string_view corpus = "")
      : VerilogSourceFile(referenced_path, resolved_path, corpus),
        not_owned_analyzer_(&analyzer) {
    processing_state_ = ProcessingState::kParsed;  // Advance to full parsed.
    status_ = analyzer.ParseStatus();
  }

  // Do nothing (file contents already loaded)
  absl::Status Open() final { return absl::OkStatus(); }

  // Do nothing (contents already parsed)
  absl::Status Parse() final { return status_; }

  // Return TextStructureView provided previously in constructor
  const verible::TextStructureView *GetTextStructure() const final {
    return &not_owned_analyzer_->Data();
  }

  // Return string-view content range of text structure.
  absl::string_view GetContent() const final {
    return not_owned_analyzer_->Data().Contents();
  }

 private:
  // TODO: make some sort of owned/non-owned version so that we don't have
  // two different types.
  const verilog::VerilogAnalyzer *const not_owned_analyzer_;
};

// VerilogProject represents a set of files as a cohesive unit of compilation.
// Files can include top-level translation units and preprocessor included
// files. This is responsible for owning string memory that corresponds
// to files' contents and analysis results.
class VerilogProject {
  // Collection of per-file metadata and analyzer objects
  // key: referenced file name (as opposed to resolved filename)
  using NameToFileMap =
      std::map<std::string, std::unique_ptr<VerilogSourceFile>,
               VerilogSourceFile::Less>;

 public:
  using iterator = NameToFileMap::iterator;
  using const_iterator = NameToFileMap::const_iterator;

  // Construct VerilogProject with a choice of allowing to look up file
  // origin.
  VerilogProject(absl::string_view root,
                 const std::vector<std::string> &include_paths,
                 absl::string_view corpus = "",
                 bool provide_lookup_file_origin = true)
      : translation_unit_root_(root),
        corpus_(corpus),
        include_paths_(include_paths),
        content_index_(provide_lookup_file_origin
                           ? std::make_optional<ContentToFileIndex>()
                           : std::nullopt) {}

  VerilogProject(const VerilogProject &) = delete;
  VerilogProject(VerilogProject &&) = delete;
  VerilogProject &operator=(const VerilogProject &) = delete;
  VerilogProject &operator=(VerilogProject &&) = delete;

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
  absl::StatusOr<VerilogSourceFile *> OpenTranslationUnit(
      absl::string_view referenced_filename);

  // Opens a file that was `included.
  // If the file was previously opened, that data is returned.
  absl::StatusOr<VerilogSourceFile *> OpenIncludedFile(
      absl::string_view referenced_filename);

  // Adds an already opened file by directly passing its content.
  // This is needed in external kythe backends.
  void AddVirtualFile(absl::string_view resolved_filename,
                      absl::string_view content);

  // Returns a previously referenced file, or else nullptr.
  VerilogSourceFile *LookupRegisteredFile(
      absl::string_view referenced_filename) {
    return LookupRegisteredFileInternal(referenced_filename);
  }

  // Removes the file from project and releases the resources. Returns true if
  // the file was removed.
  bool RemoveRegisteredFile(absl::string_view referenced_filename);

  // Non-modifying variant of lookup.
  const VerilogSourceFile *LookupRegisteredFile(
      absl::string_view referenced_filename) const {
    return LookupRegisteredFileInternal(referenced_filename);
  }

  // Find the source file that a particular string_view came from.
  // Returns nullptr if lookup failed for any reason.
  const VerilogSourceFile *LookupFileOrigin(
      absl::string_view content_substring) const;

  // Returns relative path to the VerilogProject
  std::string GetRelativePathToSource(absl::string_view absolute_filepath);

  // Updates file from external source with an already parsed content.
  // (e.g. Language Server).
  // "parsed" needs to be a VerilogAnalyzer that is valid until the given
  // path is removed.
  // If "parsed" is nullptr, the old parsed file is removed and replaced
  // with a standard VerilogSourceFile, reading from a filesystem.
  // (TODO: this is a fairly specific functionality; make this composed).
  void UpdateFileContents(absl::string_view path,
                          const verilog::VerilogAnalyzer *parsed);

  // Adds include directory to the project
  void AddIncludePath(absl::string_view includepath) {
    std::string path = {includepath.begin(), includepath.end()};
    if (std::find(include_paths_.begin(), include_paths_.end(), path) ==
        include_paths_.end()) {
      include_paths_.push_back(path);
    }
  }

 private:
  absl::StatusOr<VerilogSourceFile *> OpenFile(
      absl::string_view referenced_filename,
      absl::string_view resolved_filename, absl::string_view corpus);

  // Error status factory, when include file is not found.
  absl::Status IncludeFileNotFoundError(
      absl::string_view referenced_filename) const;

  // Returns a previously referenced file, or else nullptr.
  VerilogSourceFile *LookupRegisteredFileInternal(
      absl::string_view referenced_filename) const;

  // Returns the opened file or parse/not found error. If the file is not
  // opened, returns nullopt.
  absl::optional<absl::StatusOr<VerilogSourceFile *>> FindOpenedFile(
      absl::string_view filename) const;

  // Attempt to remove file and metadata if it exists. Return 'true' on success.
  bool RemoveByName(const std::string &filename);

  // The path from which top-level translation units are referenced relatively
  // (often from a file list).  This path can be relative or absolute.
  // Default: the working directory of the invoking process.
  const std::string translation_unit_root_ = ".";

  // The corpus to which this project belongs (e.g.,
  // 'github.com/chipsalliance/verible').
  const std::string corpus_;

  // The sequence of directories from which to search for `included files.
  // These can be absolute, or relative to the process's working directory.
  std::vector<std::string> include_paths_;

  // Set of opened files, keyed by referenced (not resolved) filename.
  NameToFileMap files_;

  // Index files by content substrings. Any substring in any of the files
  // allows to look up the corresponding VerilogSourceFile object.
  class ContentToFileIndex {
   public:
    // Put content range of file into index.
    void Register(const VerilogSourceFile *file);

    // Remove given file from the index.
    void Unregister(const VerilogSourceFile *file);

    // Given a memory subrange of any of the indexed files, return the
    // corresponding file or nullptr if none of the files contains that range.
    const VerilogSourceFile *Lookup(absl::string_view content_substring) const;

   private:
    // Maps any string_view (substring) to its full source file text
    // (superstring).
    verible::StringViewSuperRangeMap string_view_map_;

    // Maps start of text buffer to its corresponding analyzer object.
    // key: the starting address of a string buffer belonging to an opened file.
    //   This can come from the .begin() of any entry in string_view_map_.
    std::map<absl::string_view::const_iterator, const VerilogSourceFile *>
        buffer_to_analyzer_map_;
  };

  std::optional<ContentToFileIndex> content_index_;
};

}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_VERILOG_PROJECT_H_
