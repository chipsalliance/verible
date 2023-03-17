// Copyright 2021 The Verible Authors.
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
//

#include "verilog/tools/ls/lsp-parse-buffer.h"

#include "absl/status/status.h"
#include "common/lsp/lsp-file-utils.h"
#include "common/util/logging.h"

namespace verilog {
static absl::StatusOr<std::vector<verible::LintRuleStatus>> RunLinter(
    absl::string_view filename, const verilog::VerilogAnalyzer &parser) {
  const auto &text_structure = parser.Data();

  verilog::LinterConfiguration config;
  const absl::string_view file_path = verible::lsp::LSPUriToPath(filename);
  if (auto from_flags = LinterConfigurationFromFlags(file_path);
      from_flags.ok()) {
    config = *from_flags;
  } else {
    LOG(ERROR) << from_flags.status().message() << std::endl;
  }

  return VerilogLintTextStructure(filename, config, text_structure);
}

ParsedBuffer::ParsedBuffer(int64_t version, absl::string_view uri,
                           absl::string_view content)
    : version_(version),
      uri_(uri),
      parser_(verilog::VerilogAnalyzer::AnalyzeAutomaticPreprocessFallback(
          content, uri)) {
  VLOG(1) << "Analyzed " << uri << " lex:" << parser_->LexStatus()
          << "; parser:" << parser_->ParseStatus() << std::endl;
  // TODO(hzeller): we should use a filename not URI; strip prefix.
  if (auto lint_result = RunLinter(uri, *parser_); lint_result.ok()) {
    lint_statuses_ = std::move(lint_result.value());
  }
}

void BufferTracker::Update(const std::string &filename,
                           const verible::lsp::EditTextBuffer &txt) {
  if (current_ && current_->version() == txt.last_global_version()) {
    return;  // Nothing to do (we don't really expect this to happen)
  }
  txt.RequestContent([&txt, &filename, this](absl::string_view content) {
    current_ = std::make_shared<ParsedBuffer>(txt.last_global_version(),
                                              filename, content);
  });
  if (current_->parsed_successfully()) {
    last_good_ = current_;
  }
}

verible::lsp::BufferCollection::UriBufferCallback
BufferTrackerContainer::GetSubscriptionCallback() {
  return
      [this](const std::string &uri, const verible::lsp::EditTextBuffer *txt) {
        if (txt) {
          const BufferTracker *tracker = Update(uri, *txt);
          // Now inform our listeners.
          for (const auto &change_listener : change_listeners_) {
            change_listener(uri, tracker);
          }
        } else {
          Remove(uri);
          for (const auto &change_listener : change_listeners_) {
            change_listener(uri, nullptr);
          }
        }
      };
}

BufferTracker *BufferTrackerContainer::Update(
    const std::string &uri, const verible::lsp::EditTextBuffer &txt) {
  auto inserted = buffers_.insert({uri, nullptr});
  if (inserted.second) {
    inserted.first->second.reset(new BufferTracker());
  }
  inserted.first->second->Update(uri, txt);
  return inserted.first->second.get();
}

const BufferTracker *BufferTrackerContainer::FindBufferTrackerOrNull(
    const std::string &uri) const {
  auto found = buffers_.find(uri);
  if (found == buffers_.end()) return nullptr;
  return found->second.get();
}
}  // namespace verilog
