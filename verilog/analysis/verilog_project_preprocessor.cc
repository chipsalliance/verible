// Copyright 2017-2020 The Verible Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#include "verilog/analysis/verilog_project_preprocessor.h"

#include <iostream>
#include <string>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "common/text/text_structure.h"
#include "common/util/file_util.h"
#include "common/util/logging.h"

namespace verilog {

VerilogLexerContainer::VerilogLexerContainer(absl::string_view code)
    : VerilogLexer(code) {}

absl::Status VerilogLexerContainer::Lex() {
  for (VerilogLexer::DoNextToken(); !VerilogLexer::GetLastToken().isEOF();
       VerilogLexer::DoNextToken()) {
    if (verilog::VerilogLexer::KeepSyntaxTreeTokens(
            VerilogLexer::GetLastToken())) {
      data_.push_back(VerilogLexer::GetLastToken());
    }
  }
  if (VerilogLexer::GetLastToken().isEOF()) {
    lexing_completed_ = true;
    return absl::OkStatus();
  }
  return absl::InvalidArgumentError("Error lexing is not completed.");
}

VerilogPreprocessorSourceFile::VerilogPreprocessorSourceFile(
    absl::string_view referenced_path, const absl::Status& status)
    : referenced_path_(referenced_path), status_(status) {}

absl::Status VerilogPreprocessorSourceFile::Open() {
  // Don't re-open.  analyzed_structure_ should be set/written once only.
  if (state_ != State::kInitialized) return status_;

  // Load file contents.
  std::string content;
  status_ = verible::file::GetContents(ResolvedPath(), &content);
  if (!status_.ok()) return status_;

  // TODO(fangism): std::move or memory-map to avoid a short-term copy.
  analyzed_structure_ = std::make_unique<VerilogLexerContainer>(content);
  state_ = State::kOpened;
  // status_ is Ok here.
  return status_;
}

verible::TokenSequence VerilogPreprocessorSourceFile::GetTokenSequence() const {
  // Need to check if the lexing is done successfully.
  // if(analyzed_structure_->LexedCompleted())
  return analyzed_structure_->Data();
}

std::ostream& operator<<(std::ostream& stream,
                         const VerilogPreprocessorSourceFile& source) {
  stream << "referenced path: " << source.ReferencedPath() << std::endl;
  stream << "resolved path: " << source.ResolvedPath() << std::endl;
  stream << "corpus: " << source.Corpus() << std::endl;
  const auto status = source.Status();
  stream << "status: " << (status.ok() ? "ok" : status.message()) << std::endl;
  const auto token_sequence = source.GetTokenSequence();
  for (const auto& token : token_sequence) stream << token;
  return stream;
}

}  // namespace verilog
