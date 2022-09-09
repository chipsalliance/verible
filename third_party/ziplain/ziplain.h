/* -*- mode: c++ -*- */
#ifndef ZIPLAIN_H
#define ZIPLAIN_H

#include <time.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace ziplain {
// A ByteSource is a generator function that returns content, possibly chunked
// in multiple pieces.
//
// Each call to a ByteSink yields more content, non-empty string_views.
// End-of-data is signified by an empty string_view.
//
// The return values are only valid until the next call.
//
// The fact that ByteSource returns different results on each call implies that
// it has a state (e.g. successive read() calls); implementations need to make
// sure that even partial reads don't result in leaked resources.
using ByteSource = std::function<std::string_view()>;

// A function that receives bytes. Consecutive calls concatenate. Return
// value 'true' indicates operation succeeded.
using ByteSink = std::function<bool(std::string_view out)>;

// Utility function that wraps a string_view and provides a ByteSource. Use
// this if you have an in-memory representation of your content.
ByteSource MemoryByteSource(std::string_view input);

// Utility function that reads the content of a file and provides a ByteSource.
// May return an empty function if file could not be opened
// (no other errors are reported. If you need error handling, write your own).
ByteSource FileByteSource(const char *filename);

// Encode a zip file. Call AddFile() 0..n times, then finalize with Finish()
// No more files can be added after Finish().
class Encoder {
 public:
  // Create an encoder writing to ByteSink.
  // No compression on "compression_level" zero, otherwise deflate
  Encoder(int compression_level, ByteSink out);
  ~Encoder();

  // Add a file with given filename and content.
  bool AddFile(std::string_view filename, ByteSource content);

  // Finalize container. Note if your byte-sink is wrapping a file, you
  // might need to close it after Finish() returns.
  bool Finish();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
}  // namespace ziplain
#endif /* ZIPLAIN_H */
