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

#include "verible/common/lsp/message-stream-splitter.h"

#include <algorithm>
#include <cstddef>
#include <cstring>

#include "absl/status/status.h"
#include "absl/strings/escaping.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "verible/common/util/status-macros.h"

namespace verible {
namespace lsp {
absl::Status MessageStreamSplitter::PullFrom(const ReadFun &read_fun) {
  if (!message_processor_) {
    return absl::FailedPreconditionError(
        "MessageStreamSplitter: Message processor not yet set, needed "
        "before PullFrom() called");
  }
  return ReadInput(read_fun);
}

// Return -1 if header is incomplete (not enough data yet).
// Return -2 if header complete, but does not contain a valid
//    Content-Length header (i.e. an actual problem)
// On success, returns the offset to the body and its size in "body_size"
static constexpr int kIncompleteHeader = -1;
static constexpr int kGarbledHeader = -2;
int MessageStreamSplitter::ParseHeaderGetBodyOffset(absl::string_view data,
                                                    int *body_size) {
  // TODO(hzeller): Make this more robust. Parse each \r\n section separately.
  static constexpr absl::string_view kEndHeaderMarker = "\r\n\r\n";
  static constexpr absl::string_view kContentLengthHeader = "Content-Length: ";

  int header_marker_len = kEndHeaderMarker.length();
  auto end_of_header = data.find(kEndHeaderMarker);
  if (end_of_header == absl::string_view::npos) return kIncompleteHeader;

  // Very dirty search for header - we don't check if starts with line.
  const absl::string_view header_content(data.data(), end_of_header);
  auto found_ContentLength_header = header_content.find(kContentLengthHeader);
  if (found_ContentLength_header == absl::string_view::npos) {
    return kGarbledHeader;
  }

  size_t end_key = found_ContentLength_header + kContentLengthHeader.size();
  absl::string_view header_value = header_content.substr(end_key);
  auto end_of_digit = std::find_if(header_value.begin(), header_value.end(),
                                   [](char c) { return c < '0' || c > '9'; });
  header_value = header_value.substr(0, end_of_digit - header_value.begin());
  if (!absl::SimpleAtoi(header_value, body_size)) {
    return kGarbledHeader;
  }

  return end_of_header + header_marker_len;
}

// Read from data and process all fully available messages found in data.
// Updates "data" to return the remaining unprocessed data.
// Returns ok() status encountered a corrupted header.
absl::Status MessageStreamSplitter::ProcessContainedMessages(
    absl::string_view *data) {
  while (!data->empty()) {
    int body_size = 0;
    const int body_offset = ParseHeaderGetBodyOffset(*data, &body_size);
    if (body_offset == kGarbledHeader) {
      absl::string_view limited_view(
          data->data(), std::min(data->size(), static_cast<size_t>(256)));
      return absl::InvalidArgumentError(
          absl::StrCat("No `Content-Length:` header. '",
                       absl::CEscape(limited_view), "...'"));
    }

    const int message_size = body_offset + body_size;
    if (body_offset == kIncompleteHeader ||
        message_size > static_cast<int>(data->size())) {
      return absl::OkStatus();  // Only insufficient partial buffer available.
    }

    absl::string_view header(data->data(), body_offset);
    absl::string_view body(data->data() + body_offset, body_size);
    message_processor_(header, body);

    stats_largest_body_ = std::max(stats_largest_body_, body.size());

    *data = {data->data() + message_size, data->size() - message_size};
  }
  return absl::OkStatus();
}

// Read from "read_fun", fill internal buffer and call all available
// complete messages in it.
absl::Status MessageStreamSplitter::ReadInput(const ReadFun &read_fun) {
  size_t write_offset = 0;

  // Move all we had left from last time to the beginning of the buffer.
  // This is in the same buffer, so we need to memmove()
  if (!pending_data_.empty()) {
    memmove(read_buffer_.data(), pending_data_.data(), pending_data_.size());
    write_offset = pending_data_.size();
  }

  if (write_offset == read_buffer_.size()) {
    read_buffer_.resize(2 * read_buffer_.size());
  }

  const int free_space = read_buffer_.size() - write_offset;
  int bytes_read = read_fun(read_buffer_.data() + write_offset, free_space);
  if (bytes_read <= 0) {
    // Got EOF.
    // If we still have data pending, regard this as data loss situation, as
    // we were never able to fully read the last message and send to process.
    // Otherwise, report 'Unavailable' to indicate EOF (meh, this should
    // be a better message).
    if (!pending_data_.empty()) {
      return absl::DataLossError(
          absl::StrCat("Got EOF, but still have incomplete message with ",
                       pending_data_.size(), " bytes read so far."));
    }
    return absl::UnavailableError(absl::StrCat("read() returned ", bytes_read));
  }
  stats_total_bytes_read_ += bytes_read;

  absl::string_view data(read_buffer_.data(), write_offset + bytes_read);
  RETURN_IF_ERROR(ProcessContainedMessages(&data));

  pending_data_ = data;  // Remember for next round.

  return absl::OkStatus();
}
}  // namespace lsp
}  // namespace verible
