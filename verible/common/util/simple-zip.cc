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

#include "verible/common/util/simple-zip.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "third_party/portable_endian/portable_endian.h"
#include "zlib/include/zlib.h"

namespace verible {
namespace zip {

ByteSource MemoryByteSource(std::string_view input) {
  auto is_called = std::make_shared<bool>(false);
  return [is_called, input]() -> std::string_view {
    if (!*is_called) {
      *is_called = true;
      return input;
    }
    return {};
  };
}

ByteSource FileByteSource(const char *filename) {
  FILE *f = fopen(filename, "rb");
  if (!f) return nullptr;
  struct State {
    explicit State(FILE *f) : file(f) {}
    ~State() { fclose(file); }
    FILE *file;
    char buffer[65536];
  };
  auto state = std::make_shared<State>(f);  // capture req. copy-constructable
  return [state]() -> std::string_view {
    size_t r = fread(state->buffer, 1, sizeof(State::buffer), state->file);
    return {state->buffer, r};
  };
}

// Header writer, taking care of little-endianness writing of fields.
namespace {

class HeaderWriter {
 public:
  explicit HeaderWriter(char *buffer) : begin_(buffer), pos_(buffer) {}

  HeaderWriter &AddInt16(uint16_t value) {
    uint16_t le = htole16(value);  // NOP on most platforms
    memcpy(pos_, &le, 2);
    pos_ += 2;
    return *this;
  }
  HeaderWriter &AddInt32(uint32_t value) {
    uint32_t le = htole32(value);  // NOP on most platforms
    memcpy(pos_, &le, 4);
    pos_ += 4;
    return *this;
  }
  HeaderWriter &AddLiteral(std::string_view str) {
    memcpy(pos_, str.data(), str.size());
    pos_ += str.size();
    return *this;
  }

  bool Write(const ByteSink &out) {
    return out({begin_, static_cast<size_t>(pos_ - begin_)});
  }

  void Append(std::string *buffer) { buffer->append(begin_, pos_ - begin_); }

 private:
  const char *begin_;
  char *pos_;
};
}  // namespace

struct Encoder::Impl {
  static constexpr int16_t kPkZipVersion = 20;  // 2.0, pretty basic.
  struct CompressResult {
    uint32_t input_crc;
    size_t input_size;
    size_t output_size;
  };

  Impl(int compression_level, ByteSink out)
      : compression_level_(std::clamp(compression_level, 0, 9)),
        delegate_write_(std::move(out)),
        out_([this](std::string_view s) {
          output_file_offset_ += s.size();  // Keep track of offsets.
          return delegate_write_(s);
        }) {}

  bool AddFile(std::string_view filename, const ByteSource &content_generator) {
    if (is_finished_) return false;  // Can't add more files.
    if (!content_generator) return false;

    ++file_count_;
    const size_t start_offset = output_file_offset_;

    const uint16_t mod_time = 0;  // TODO: accept time_t and convert ?
    const uint16_t mod_date = 0;

    bool success =  // Assemble local file header
        HeaderWriter(scratch_space_)
            .AddLiteral("PK\x03\x04")
            .AddInt16(kPkZipVersion)  // Minimum version needed
            .AddInt16(0x08)  // Flags. Sizes and CRC in data descriptor.
            .AddInt16(compression_level_ == 0 ? 0 : 8)
            .AddInt16(mod_time)
            .AddInt16(mod_date)
            .AddInt32(0)  // CRC32. Known later.
            .AddInt32(0)  // Compressed size: known later.
            .AddInt32(0)  // Uncompressed size: known later.
            .AddInt16(filename.length())
            .AddInt16(0)  // Extra field length
            .AddLiteral(filename)
            .Write(out_);
    if (!success) return false;

    // Data output
    const CompressResult compress_result =
        compression_level_ == 0 ? CopyDataToOutput(content_generator)
                                : CompressDataToOutput(content_generator);

    success =  // Assemble Data Descriptor after file with known CRC and size.
        HeaderWriter(scratch_space_)
            .AddInt32(compress_result.input_crc)
            .AddInt32(compress_result.output_size)
            .AddInt32(compress_result.input_size)
            .Write(out_);

    // Append directory file header entry to be written in Finish()
    HeaderWriter(scratch_space_)
        .AddLiteral("PK\x01\x02")
        .AddInt16(kPkZipVersion)  // Our Version
        .AddInt16(kPkZipVersion)  // Readable by version
        .AddInt16(0x08)           // Flag
        .AddInt16(compression_level_ == 0 ? 0 : 8)
        .AddInt16(mod_time)
        .AddInt16(mod_date)
        .AddInt32(compress_result.input_crc)
        .AddInt32(compress_result.output_size)
        .AddInt32(compress_result.input_size)
        .AddInt16(filename.length())
        .AddInt16(0)  // Extra field length
        .AddInt16(0)  // file comment length
        .AddInt16(0)  // disk number
        .AddInt16(0)  // Internal file attr
        .AddInt32(0)  // External file attr
        .AddInt32(start_offset)
        .AddLiteral(filename)
        .Append(&central_dir_data_);

    return success;
  }

  bool Finish() {
    if (is_finished_) return false;
    is_finished_ = true;
    const size_t start_offset = output_file_offset_;
    if (!out_({central_dir_data_.data(), central_dir_data_.size()})) {
      return false;
    }

    // End of central directory record
    constexpr std::string_view comment("Created with Verible simple zip");
    return HeaderWriter(scratch_space_)
        .AddLiteral("PK\x05\x06")  // End of central directory signature
        .AddInt16(0)               // our disk number
        .AddInt16(0)               // disk where it all starts
        .AddInt16(file_count_)     // Number of directory records on this disk
        .AddInt16(file_count_)     // ... and overall
        .AddInt32(central_dir_data_.size())
        .AddInt32(start_offset)
        .AddInt16(comment.length())
        .AddLiteral(comment)
        .Write(out_);
  }

  CompressResult CopyDataToOutput(const ByteSource &generator) {
    uint32_t crc = 0;
    size_t processed_size = 0;
    std::string_view chunk;
    while (!(chunk = generator()).empty()) {
      crc = crc32(crc, reinterpret_cast<const uint8_t *>(chunk.data()),
                  chunk.size());
      processed_size += chunk.size();
      out_(chunk);
    }
    return {crc, processed_size, processed_size};
  }

  CompressResult CompressDataToOutput(const ByteSource &generator) {
    uint32_t crc = 0;
    std::string_view chunk;
    z_stream stream;
    memset(&stream, 0x00, sizeof(stream));

    // Need negative window bits to tell zlib not to create a header.
    deflateInit2(&stream, compression_level_, Z_DEFLATED, -15 /*window bits*/,
                 9 /* memlevel*/, 0);

    const size_t kScratchSize = sizeof(scratch_space_);
    do {
      chunk = generator();
      const int flush_setting = chunk.empty() ? Z_FINISH : Z_NO_FLUSH;
      if (!chunk.empty()) {
        crc = crc32(crc, reinterpret_cast<const uint8_t *>(chunk.data()),
                    chunk.size());
      }
      stream.avail_in = chunk.size();
      // Nasty C-API without 'const' input.
      stream.next_in =
          reinterpret_cast<uint8_t *>(const_cast<char *>(chunk.data()));
      do {
        stream.avail_out = kScratchSize;
        stream.next_out = reinterpret_cast<uint8_t *>(scratch_space_);
        deflate(&stream, flush_setting);
        const size_t output_size = kScratchSize - stream.avail_out;
        if (output_size) out_({scratch_space_, output_size});
      } while (stream.avail_out == 0);
    } while (!chunk.empty());

    CompressResult result = {crc, stream.total_in, stream.total_out};
    deflateEnd(&stream);
    return result;
  }

  const int compression_level_;
  const ByteSink delegate_write_;
  const ByteSink out_;

  size_t file_count_ = 0;
  size_t output_file_offset_ = 0;
  std::string central_dir_data_;
  bool is_finished_ = false;
  char scratch_space_[1 << 20];  // to assemble headers and compression data
};

Encoder::Encoder(int compression_level, ByteSink out)
    : impl_(new Impl(compression_level, std::move(out))) {}

Encoder::~Encoder() { Finish(); }

bool Encoder::AddFile(std::string_view filename,
                      const ByteSource &content_generator) {
  return impl_->AddFile(filename, content_generator);
}
bool Encoder::Finish() { return impl_->Finish(); }

}  // namespace zip
}  // namespace verible
