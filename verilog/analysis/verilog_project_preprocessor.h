// Copyright 2017-2022 The Verible Authors.
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

#ifndef VERIBLE_VERILOG_ANALYSIS_VERILOG_PROJECT_PREPROCESSOR_H_
#define VERIBLE_VERILOG_ANALYSIS_VERILOG_PROJECT_PREPROCESSOR_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/strings/string_memory_map.h"
#include "common/text/text_structure.h"
#include "common/text/token_stream_view.h"
#include "verilog/parser/verilog_lexer.h"

namespace verilog {

class VerilogLexerContainer : public VerilogLexer {
 public:
  explicit VerilogLexerContainer(absl::string_view code);

 private:
  // Lexed token sequence.
  verible::TokenSequence data_;

  // is true if the lexining is finished.
  bool lexing_completed_ = false;

 public:
  verible::TokenSequence Data() { return data_; }
  bool LexedCompleted() { return lexing_completed_; }
  absl::Status Lex();
};

// A read-only view of a single Verilog source file.
class VerilogPreprocessorSourceFile {
 public:
  VerilogPreprocessorSourceFile(absl::string_view referenced_path,
                                absl::string_view resolved_path,
                                absl::string_view corpus)
      : referenced_path_(referenced_path),
        resolved_path_(resolved_path),
        corpus_(corpus) {}

  // When a file is not found among a set of paths, remember it with an
  // error status.
  VerilogPreprocessorSourceFile(absl::string_view referenced_path,
                                const absl::Status& status);

  VerilogPreprocessorSourceFile(const VerilogPreprocessorSourceFile&) = delete;
  VerilogPreprocessorSourceFile& operator=(
      const VerilogPreprocessorSourceFile&) = delete;

  VerilogPreprocessorSourceFile(VerilogPreprocessorSourceFile&&) =
      default;  // need for std::map::emplace
  VerilogPreprocessorSourceFile& operator=(VerilogPreprocessorSourceFile&&) =
      delete;
  virtual ~VerilogPreprocessorSourceFile() = default;

  // Opens a file using the resolved path and loads the contents into memory.
  // This does not attempt to parse/analyze the contents.
  virtual absl::Status Open();

  // Returns the first non-Ok status if there is one, else OkStatus().
  absl::Status Status() const { return status_; }

  // Returns the name used to reference the file.
  absl::string_view ReferencedPath() const { return referenced_path_; }

  // Returns the corpus to which this file belongs (e.g.,
  // github.com/chipsalliance/verible).
  absl::string_view Corpus() const { return corpus_; }

  // Returns a (possibly more qualified) path to the file.
  absl::string_view ResolvedPath() const { return resolved_path_; }

  // Get the token sequence of the source file.
  verible::TokenSequence GetTokenSequence() const;

  // Comparator for ordering files for internal storage.
  // Known limitation: this comparator won't work if you have multiple files
  // with the same name referenced without a distinguishing path prefix.
  struct Less {
    using is_transparent = void;  // hetergenous compare

    static absl::string_view to_string_view(absl::string_view s) { return s; }
    static absl::string_view to_string_view(
        const VerilogPreprocessorSourceFile& f) {
      return f.ReferencedPath();
    }
    static absl::string_view to_string_view(
        const VerilogPreprocessorSourceFile* f) {
      return f->ReferencedPath();
    }

    // T1/T2 could be any combination of:
    // {const VerilogPreprocessorSourceFile&, absl::string_view}.
    template <typename T1, typename T2>
    bool operator()(T1 left, T2 right) const {
      return to_string_view(left) < to_string_view(right);
    }
  };

 protected:
  // Tracking state for linear progression of analysis, which allows
  // prerequisite actions to be cached.
  enum State {
    // Only the paths have been established.
    kInitialized,

    // Files have been opened and contents loaded.
    kOpened,
  };

  // This is the how the file is referenced either in a file list or `include.
  // This should be const for the lifetime of this object, but isn't declared as
  // such so this class can have a default move constructor.
  std::string referenced_path_;

  // Often a concatenation of a base path with a relative path.
  // This should be const for the lifetime of this object, but isn't declared as
  // such so this class can have a default move constructor.
  std::string resolved_path_;

  // The corpus to which this file belongs to (e.g.,
  // github.com/chipsalliance/verible).
  absl::string_view corpus_;

  // State of this file.
  State state_ = State::kInitialized;

  // Holds any diagostics for problems encountered finding/reading this file.
  absl::Status status_;

  // Holds the file's string contents in owned memory, along with other forms
  // like token streams and syntax tree.
  std::unique_ptr<VerilogLexerContainer> analyzed_structure_;
};

// Printable representation for debugging.
std::ostream& operator<<(std::ostream&, const VerilogPreprocessorSourceFile&);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_VERILOG_PROJECT_PREPROCESSOR_H_
