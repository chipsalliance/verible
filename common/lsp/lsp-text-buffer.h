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

#ifndef VERIBLE_COMMON_LSP_LSP_TEXT_BUFFER_H
#define VERIBLE_COMMON_LSP_LSP_TEXT_BUFFER_H

#include <functional>
#include <memory>
#include <vector>

//
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "common/lsp/json-rpc-dispatcher.h"
#include "common/lsp/lsp-protocol.h"

namespace verible {
namespace lsp {
// The EditTextBuffer keeps track of the content of buffers on the client.
// It is fed initially with the full content, and from then on receives
// change events to keep in sync.
// It provides ways to pass the current content to a requestor that needs to
// process it.
class EditTextBuffer {
 public:
  using ContentProcessFun = std::function<void(absl::string_view)>;

  explicit EditTextBuffer(absl::string_view initial_text);
  EditTextBuffer(const EditTextBuffer &) = delete;
  EditTextBuffer(EditTextBuffer &&) = delete;

  // Requst to flatten the content call and call function "processor" that
  // gets a string_view of the current state that is valid for the duration
  // of the call.
  void RequestContent(const ContentProcessFun &processor) const;

  // Same as RequestContent() for a specific line.
  void RequestLine(int line, const ContentProcessFun &processor) const;

  // Apply a single LSP edit operation.
  bool ApplyChange(const TextDocumentContentChangeEvent &c);

  // Apply a sequence of changes.
  void ApplyChanges(const std::vector<TextDocumentContentChangeEvent> &cc);

  // Lines in this document.
  size_t lines() const { return lines_.size(); }

  // Length of document in bytes.
  int64_t document_length() const { return document_length_; }

  // Last global version number this buffer has edited from.
  int64_t last_global_version() const { return last_global_version_; }

  // Set global version; this typically will be done by the BufferCollection.
  void set_last_global_version(int64_t v) { last_global_version_ = v; }

 private:
  // TODO: this should be unique_ptr, but assignment in the insert() command
  // will not work. Needs to be formulated with something something std::move ?
  using LineVector = std::vector<std::shared_ptr<std::string>>;

  static LineVector GenerateLines(absl::string_view content);
  void ReplaceDocument(absl::string_view content);
  bool LineEdit(const TextDocumentContentChangeEvent &c, std::string *str);
  bool MultiLineEdit(const TextDocumentContentChangeEvent &c);

  int64_t last_global_version_ = 0;
  int64_t document_length_ = 0;
  LineVector lines_;
};

// A buffer collection keeps track of various open text buffers on the
// client side. Registers new ExitTextBuffers by subscribing to events
// coming from the client.
class BufferCollection {
 public:
  // Create buffer collection and subscribe to buffer events at the dispatcher.
  explicit BufferCollection(JsonRpcDispatcher *dispatcher);
  BufferCollection(const BufferCollection &) = delete;
  BufferCollection(BufferCollection &&) = delete;

  // Handle textDocument/didOpen event; create a new EditTextBuffer.
  void didOpenEvent(const DidOpenTextDocumentParams &o);

  // Handle textDocument/didChange event. Delegate changes to existing buffer.
  void didChangeEvent(const DidChangeTextDocumentParams &o);

  // Handle textDocument/didClose event. Forget about buffer.
  void didCloseEvent(const DidCloseTextDocumentParams &o);

  const EditTextBuffer *findBufferByUri(const std::string &uri) const {
    auto found = buffers_.find(uri);
    return found == buffers_.end() ? nullptr : found->second.get();
  }

  // Edits done on all buffers from all time. Allows to compare a single
  // number if there is any change since last time. Good to remember to get
  // only changed buffers when calling MapBuffersChangedSince()
  int64_t global_version() const { return global_version_; }

  // Calls "map_fun"() on each buffer that has changed since the given version.
  // This allows to only process changed buffers.
  // Use 0 (zero) as last version to have the map function receive all buffers.
  // "map_fun" can be nullptr in which case only the number of changed buffers.
  // are returned.
  // Returns number of buffers for which the condition applied.
  int MapBuffersChangedSince(
      int64_t last_global_version,
      const std::function<void(const std::string &uri,
                               const EditTextBuffer &buffer)> &map_fun) const;

  size_t documents_open() const { return buffers_.size(); }

 private:
  int64_t global_version_ = 0;
  std::unordered_map<std::string, std::unique_ptr<EditTextBuffer>> buffers_;
};
}  // namespace lsp
}  // namespace verible

#endif  // VERIBLE_COMMON_LSP_LSP_TEXT_BUFFER_H
