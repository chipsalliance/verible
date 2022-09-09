Forked from https://github.com/hzeller/ziplain

# ZIPlain

A plain, no-frills [ZIP-file] encoder.

Provides class `ziplain::Encoder` to create a ZIP file. Abstractions
for `ByteSource` and `ByteSink` allow to adapt it to local IO requirements.

In the following example, we provide the output `ByteSink` by implementing
a closure wrapping a `FILE*` stream.
As input `ByteSource`, ziplain-provided wrappers for files and in-memory
representations are used.

```c++
constexpr int kCompressionLevel = 9;
FILE *out = fopen("test-output.zip", "wb");
ziplain::Encoder zipper(kCompressionLevel, [out](std::string_view s) {
  return fwrite(s.data(), 1, s.size(), out) == s.size();
});

zipper.AddFile("ziplain.h", ziplain::FileByteSource("ziplain.h"));
zipper.AddFile("ziplain.cc", ziplain::FileByteSource("ziplain.cc"));
zipper.AddFile("readme.txt", ziplain::MemoryByteSource("Hello world!\n"));
zipper.Finish();

fclose(out);
```

[ZIP-file]: https://en.wikipedia.org/wiki/ZIP_(file_format)
