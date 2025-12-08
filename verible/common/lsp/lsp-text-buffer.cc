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

#include "verible/common/lsp/lsp-text-buffer.h"

#include <algorithm>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "verible/common/lsp/json-rpc-dispatcher.h"
#include "verible/common/lsp/lsp-protocol.h"
#include "verible/common/strings/utf8.h"

namespace verible {
namespace lsp {
EditTextBuffer::EditTextBuffer(std::string_view initial_text) {
  ReplaceDocument(initial_text);
}

void EditTextBuffer::ApplyChanges(
    const std::vector<TextDocumentContentChangeEvent> &cc) {
  for (const auto &c : cc) ApplyChange(c);
}

bool EditTextBuffer::ApplyChange(const TextDocumentContentChangeEvent &c) {
  if (!c.has_range) {
    ReplaceDocument(c.text);
    return true;
  }

  if (c.range.end.line >= static_cast<int>(lines_.size())) {
    lines_.emplace_back(new std::string(""));
  }

  if (c.range.start.line == c.range.end.line &&
      c.text.find_first_of('\n') == std::string::npos) {
    return LineEdit(c, lines_[c.range.start.line].get());  // simple case.
  }
  return MultiLineEdit(c);
}

/*static*/ EditTextBuffer::LineVector EditTextBuffer::GenerateLines(
    std::string_view content) {
  LineVector result;
  for (const std::string_view s : absl::StrSplit(content, '\n')) {
    result.emplace_back(new std::string(s));
    result.back()->append("\n");
  }

  // Files that do or do not have a newline file-ending: represent correctly.
  if (!content.empty() && content.back() == '\n') {
    result.pop_back();
  } else {
    result.back()->pop_back();
  }

  return result;
}

void EditTextBuffer::ReplaceDocument(std::string_view content) {
  document_length_ = content.length();
  if (content.empty()) return;
  lines_ = GenerateLines(content);
}

// Return success (might not if input out of range)
bool EditTextBuffer::LineEdit(const TextDocumentContentChangeEvent &c,
                              std::string *str) {
  int end_char = c.range.end.character;

  int str_end = utf8_len(*str);
  if (!str->empty() && str->back() == '\n') --str_end;

  if (c.range.start.character > str_end) return false;
  end_char = std::min(end_char, str_end);
  if (end_char < c.range.start.character) return false;

  document_length_ -= str->length();
  const std::string_view assembly = *str;
  const auto before = utf8_substr(assembly, 0, c.range.start.character);
  const auto after = utf8_substr(assembly, end_char);
  *str = absl::StrCat(before, c.text, after);
  document_length_ += str->length();
  return true;
}

// Returns success (always succeeds);
bool EditTextBuffer::MultiLineEdit(const TextDocumentContentChangeEvent &c) {
  const std::string_view start_line = *lines_[c.range.start.line];
  const auto before = utf8_substr(start_line, 0, c.range.start.character);

  const std::string_view end_line = *lines_[c.range.end.line];
  const auto after = utf8_substr(end_line, c.range.end.character);

  // Assemble the full content to replace the range of lines with including
  // the parts that come from the first and last line to be edited.
  const std::string new_content = absl::StrCat(before, c.text, after);

  // Content length update: substract all the bytes that were in the old
  // content and add all in the new content.
  const auto before_begin = lines_.begin() + c.range.start.line;
  const auto before_end = lines_.begin() + c.range.end.line + 1;
  document_length_ -=
      std::accumulate(before_begin, before_end, 0,
                      [](int sum, const LineVector::value_type &line) {
                        return sum + line->length();
                      });
  document_length_ += new_content.length();

  // The new content might include newlines, yielding multiple single lines.
  LineVector regenerated_lines = GenerateLines(new_content);

  // Update the affected lines. Probably not the most optimal but good enough
  lines_.erase(before_begin, before_end);
  lines_.insert(lines_.begin() + c.range.start.line, regenerated_lines.begin(),
                regenerated_lines.end());
  return true;
}

BufferCollection::BufferCollection(JsonRpcDispatcher *dispatcher) {
  // Route notification events from the dispatcher to the buffer collection
  // for them to keep track of what buffers are open and all of their edits
  // they receive.
  dispatcher->AddNotificationHandler(
      "textDocument/didOpen",
      [this](const DidOpenTextDocumentParams &p) { didOpenEvent(p); });
  dispatcher->AddNotificationHandler(
      "textDocument/didClose",
      [this](const DidCloseTextDocumentParams &p) { didCloseEvent(p); });
  dispatcher->AddNotificationHandler(
      "textDocument/didChange",
      [this](const DidChangeTextDocumentParams &p) { didChangeEvent(p); });
}

void BufferCollection::didOpenEvent(const DidOpenTextDocumentParams &o) {
  auto inserted = buffers_.insert({o.textDocument.uri, nullptr});
  if (inserted.second) {
    inserted.first->second.reset(new EditTextBuffer(o.textDocument.text));
    inserted.first->second->set_last_global_version(++global_version_);
    if (change_listener_) {
      change_listener_(o.textDocument.uri, inserted.first->second.get());
    }
  }
}

void BufferCollection::didCloseEvent(const DidCloseTextDocumentParams &o) {
  if (change_listener_) {
    // Let's call the callback first in case our users still have a dangling
    // reference.
    change_listener_(o.textDocument.uri, nullptr);
  }
  buffers_.erase(o.textDocument.uri);
}

void BufferCollection::didChangeEvent(const DidChangeTextDocumentParams &o) {
  auto found = buffers_.find(o.textDocument.uri);
  if (found == buffers_.end()) return;
  EditTextBuffer *const buffer = found->second.get();
  buffer->ApplyChanges(o.contentChanges);
  buffer->set_last_global_version(++global_version_);
  if (change_listener_) change_listener_(o.textDocument.uri, buffer);
}

void EditTextBuffer::RequestContent(const ContentProcessFun &processor) const {
  std::string flat_view;
  flat_view.reserve(document_length_);
  for (const auto &l : lines_) flat_view.append(*l);
  processor(flat_view);
}

void EditTextBuffer::RequestLine(int line,
                                 const ContentProcessFun &processor) const {
  if (line < 0 || line >= static_cast<int>(lines_.size())) {
    processor("");
  } else {
    processor(*lines_[line]);
  }
}
}  // namespace lsp
}  // namespace verible
