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

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "verible/common/lsp/json-rpc-dispatcher.h"
#include "verible/common/lsp/lsp-protocol.h"

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
// client side. Registers new EditTextBuffers by subscribing to events
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

  // Edits done on all buffers from all time. Allows to compare a single
  // number if there is any change since last time. Good to remember to get
  // only changed buffers when calling MapBuffersChangedSince()
  int64_t global_version() const { return global_version_; }

  using UriBufferCallback =
      std::function<void(const std::string &uri, const EditTextBuffer *buffer)>;

  // Set a callback that is called with each change on the buffer collection.
  //
  // If a new buffer has been added or received a change, the callback is
  // called with the uri and a pointer to the buffer.
  // If a buffer has been removed, the uri of the removed buffer
  // and a nullptr will be sent.
  //
  // Note, there can only be one callback at a time, which is sufficient
  // for the current uses; if that changes, need some Add/Remove calls.
  void SetChangeListener(const UriBufferCallback &listener) {
    change_listener_ = listener;
  }

  // Number of open documents.
  size_t size() const { return buffers_.size(); }

 private:
  int64_t global_version_ = 0;
  UriBufferCallback change_listener_ = nullptr;
  absl::flat_hash_map<std::string, std::unique_ptr<EditTextBuffer>> buffers_;
};
}  // namespace lsp
}  // namespace verible

#endif  // VERIBLE_COMMON_LSP_LSP_TEXT_BUFFER_H
