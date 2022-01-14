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

#ifndef VERILOG_TOOLS_LS_LSP_PARSE_BUFFER_H
#define VERILOG_TOOLS_LS_LSP_PARSE_BUFFER_H

#include <cstdint>

#include "common/lsp/lsp-text-buffer.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/analysis/verilog_linter.h"

// ParseBuffer and BufferTrackerContainer are tracking fully parsed content
// and are corresponding to verible::lsp::EditTextBuffer and
// verible::lsp::BufferCollection that are responsible for tracking the
// bare editor text.

namespace verilog {
// A parsed buffer collects all the artifacts generated from a text buffer
// from parsing or running the linter.
//
// Right now, the ParsedBuffer is synchronously filling its internal structure
// on construction, but plan is to do that on-demand and possibly with
// std::future<>s evaluated in separate threads.
class ParsedBuffer {
 public:
  ParsedBuffer(int64_t version, absl::string_view uri,
               absl::string_view content);

  bool parsed_successfully() const {
    return parser_->LexStatus().ok() && parser_->ParseStatus().ok();
  }

  const verilog::VerilogAnalyzer &parser() const { return *parser_; }
  const std::vector<verible::LintRuleStatus> &lint_result() const {
    return lint_statuses_;
  }

  int64_t version() const { return version_; }
  const std::string &uri() const { return uri_; }

 private:
  const int64_t version_;
  const std::string uri_;
  std::unique_ptr<verilog::VerilogAnalyzer> parser_;
  std::vector<verible::LintRuleStatus> lint_statuses_;
};

// A buffer tracker tracks the EditTextBuffer content and keeps up to
// two versions of ParsedBuffers - the latest, that might have parse errors,
// and the last known good that parsed without errors (if available).
class BufferTracker {
 public:
  void Update(const std::string &filename,
              const verible::lsp::EditTextBuffer &txt);

  const ParsedBuffer *current() const { return current_.get(); }
  const ParsedBuffer *last_good() const { return last_good_.get(); }

 private:
  // The same ParsedBuffer can in both, current and last_good, or last_good can
  // be an older version. Use shared_ptr to keep track of the reference count.
  std::shared_ptr<ParsedBuffer> current_;
  std::shared_ptr<ParsedBuffer> last_good_;
};

// Container holding all buffer trackers keyed by file uri.
// This is the correspondent to verible::lsp::BufferCollection that
// internally stores
class BufferTrackerContainer {
 public:
  // Return a callback that allows to subscribe to an lsp::BufferCollection
  // to update our internal state whenever the editor state changes.
  // (internally, they exercise Update() and Remove())
  verible::lsp::BufferCollection::UriBufferCallback GetSubscriptionCallback();

  // Add a change listener for clients of ours interested in updated fresly
  // parsed content.
  using ChangeCallback =
      std::function<void(const std::string &uri, const BufferTracker &tracker)>;
  void SetChangeListener(const ChangeCallback &cb) { change_listener_ = cb; }

  // Given the URI, find the associated parse buffer if it exists.
  const BufferTracker *FindBufferTrackerOrNull(const std::string &uri) const;

 private:
  // Update internal state of the given "uri" with the content of the text
  // buffer. Return the buffer tracker.
  BufferTracker *Update(const std::string &uri,
                        const verible::lsp::EditTextBuffer &txt);

  // Remove the buffer tracker for the given "uri".
  void Remove(const std::string &uri) { buffers_.erase(uri); }

  ChangeCallback change_listener_ = nullptr;
  std::unordered_map<std::string, std::unique_ptr<BufferTracker>> buffers_;
};
}  // namespace verilog
#endif  // VERILOG_TOOLS_LS_LSP_PARSE_BUFFER_H
