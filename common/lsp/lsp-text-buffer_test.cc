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

#include "common/lsp/lsp-text-buffer.h"

#include "absl/strings/str_cat.h"
#include "common/lsp/json-rpc-dispatcher.h"
#include "gtest/gtest.h"

namespace verible {
namespace lsp {
TEST(TextBufferTest, RecreateEmptyFile) {
  EditTextBuffer buffer("");
  EXPECT_EQ(buffer.lines(), 0);
  EXPECT_EQ(buffer.document_length(), 0);
  buffer.RequestContent([](absl::string_view s) {  //
    EXPECT_TRUE(s.empty());
  });
}

TEST(TextBufferTest, RequestParticularLine) {
  EditTextBuffer buffer("foo\nbar\nbaz\n");
  ASSERT_EQ(buffer.lines(), 3);
  buffer.RequestLine(0, [](absl::string_view s) {  //
    EXPECT_EQ(std::string(s), "foo\n");
  });
  buffer.RequestLine(1, [](absl::string_view s) {  //
    EXPECT_EQ(std::string(s), "bar\n");
  });

  // Be graceful with out-of-range requests.
  buffer.RequestLine(-1, [](absl::string_view s) {  //
    EXPECT_TRUE(s.empty());
  });
  buffer.RequestLine(100, [](absl::string_view s) {  //
    EXPECT_TRUE(s.empty());
  });
}

TEST(TextBufferTest, RecreateFileWithAndWithoutNewlineAtEOF) {
  static constexpr absl::string_view kBaseFile =
      "Hello World\n"
      "\n"
      "Foo";

  for (const absl::string_view append : {"", "\n", "\r\n"}) {
    const std::string &content = absl::StrCat(kBaseFile, append);
    EditTextBuffer buffer(content);
    EXPECT_EQ(buffer.lines(), 3);

    buffer.RequestContent([&](absl::string_view s) {  //
      EXPECT_EQ(std::string(s), content);
    });
  }
}

TEST(TextBufferTest, RecreateCRLFFiles) {
  EditTextBuffer buffer("Foo\r\nBar\r\n");
  EXPECT_EQ(buffer.lines(), 2);
  buffer.RequestContent([&](absl::string_view s) {
    EXPECT_EQ("Foo\r\nBar\r\n", std::string(s));
  });
}

TEST(TextBufferTest, ChangeApplyFullContent) {
  EditTextBuffer buffer("Foo\nBar\n");
  const TextDocumentContentChangeEvent change = {
      .range = {},
      .has_range = false,
      .text = "NewFile",
  };
  EXPECT_TRUE(buffer.ApplyChange(change));
  buffer.RequestContent([&](absl::string_view s) {  //
    EXPECT_EQ("NewFile", std::string(s));
  });
}

TEST(TextBufferTest, ChangeApplySingleLine_Insert) {
  EditTextBuffer buffer("Hello World");
  const TextDocumentContentChangeEvent change = {
      .range =
          {
              .start = {0, 6},
              .end = {0, 6},
          },
      .has_range = true,
      .text = "brave ",
  };
  EXPECT_TRUE(buffer.ApplyChange(change));
  EXPECT_EQ(buffer.document_length(), 17);
  buffer.RequestContent([&](absl::string_view s) {
    EXPECT_EQ("Hello brave World", std::string(s));
  });
}

TEST(TextBufferTest, ChangeApplySingleLine_InsertFromEmptyFile) {
  EditTextBuffer buffer("");
  const TextDocumentContentChangeEvent change = {
      .range =
          {
              .start = {0, 0},
              .end = {0, 0},
          },
      .has_range = true,
      .text = "New File!",
  };
  EXPECT_TRUE(buffer.ApplyChange(change));
  buffer.RequestContent([&](absl::string_view s) {  //
    EXPECT_EQ("New File!", std::string(s));
  });
}

TEST(TextBufferTest, ChangeApplySingleLine_Replace) {
  EditTextBuffer buffer("Hello World\n");
  const TextDocumentContentChangeEvent change = {
      .range =
          {
              .start = {0, 6},
              .end = {0, 11},
          },
      .has_range = true,
      .text = "Planet",
  };
  EXPECT_TRUE(buffer.ApplyChange(change));
  buffer.RequestContent([&](absl::string_view s) {
    EXPECT_EQ("Hello Planet\n", std::string(s));
  });
}

TEST(TextBufferTest, ChangeApplySingleLine_ReplaceNotFirstLine) {
  // Make sure we properly access the right line.
  EditTextBuffer buffer("Hello World\nFoo\n");
  const TextDocumentContentChangeEvent change = {
      .range =
          {
              .start = {1, 0},
              .end = {1, 3},
          },
      .has_range = true,
      .text = "Bar",
  };
  EXPECT_TRUE(buffer.ApplyChange(change));
  buffer.RequestContent([&](absl::string_view s) {
    EXPECT_EQ("Hello World\nBar\n", std::string(s));
  });
}

TEST(TextBufferTest, ChangeApplySingleLine_Erase) {
  EditTextBuffer buffer("Hello World\n");
  const TextDocumentContentChangeEvent change = {
      .range =
          {
              .start = {0, 5},
              .end = {0, 11},
          },
      .has_range = true,
      .text = "",
  };
  EXPECT_TRUE(buffer.ApplyChange(change));
  EXPECT_EQ(buffer.document_length(), 6);
  buffer.RequestContent([&](absl::string_view s) {  //
    EXPECT_EQ("Hello\n", std::string(s));
  });
}

TEST(TextBufferTest, ChangeApplySingleLine_ReplaceCorrectOverlongEnd) {
  const TextDocumentContentChangeEvent change = {
      .range =
          {
              .start = {0, 6}, .end = {0, 42},  // Too long end shall be trimmed
          },
      .has_range = true,
      .text = "Planet",
  };

  {
    EditTextBuffer buffer("Hello World\n");
    EXPECT_TRUE(buffer.ApplyChange(change));
    buffer.RequestContent([&](absl::string_view s) {
      EXPECT_EQ("Hello Planet\n", std::string(s));
    });
  }

  {
    EditTextBuffer buffer("Hello World");
    EXPECT_TRUE(buffer.ApplyChange(change));
    buffer.RequestContent([&](absl::string_view s) {
      EXPECT_EQ("Hello Planet", std::string(s));
    });
  }
}

TEST(TextBufferTest, ChangeApplyMultiLine_EraseBetweenLines) {
  EditTextBuffer buffer("Hello\nWorld\n");
  const TextDocumentContentChangeEvent change = {
      .range =
          {
              .start = {0, 2},  // From here to end of line
              .end = {1, 0},
          },
      .has_range = true,
      .text = "y ",
  };
  EXPECT_TRUE(buffer.ApplyChange(change));
  buffer.RequestContent([&](absl::string_view s) {  //
    EXPECT_EQ("Hey World\n", std::string(s));
  });
  EXPECT_EQ(buffer.document_length(), 10);  // won't work yet.
}

TEST(TextBufferTest, ChangeApplyMultiLine_InsertMoreLines) {
  EditTextBuffer buffer("Hello\nbrave World\n");
  const TextDocumentContentChangeEvent change = {
      .range =
          {
              .start = {0, 2},  // From here to end of line
              .end = {1, 5},
          },
      .has_range = true,
      .text = "y!\nThis will be a new line\nand more in this",
  };
  EXPECT_EQ(buffer.lines(), 2);
  EXPECT_TRUE(buffer.ApplyChange(change));
  EXPECT_EQ(buffer.lines(), 3);
  static constexpr absl::string_view kExpected =
      "Hey!\nThis will be a new line\nand more in this World\n";
  buffer.RequestContent([&](absl::string_view s) {  //
    EXPECT_EQ(kExpected, std::string(s));
  });
  EXPECT_EQ(buffer.document_length(), kExpected.length());
}

TEST(TextBufferTest, ChangeApplyMultiLine_InsertFromStart) {
  EditTextBuffer buffer("");
  const TextDocumentContentChangeEvent change = {
      .range =
          {
              .start = {0, 0},
              .end = {0, 0},
          },
      .has_range = true,
      .text = "This is now\na multiline\nfile\n",
  };
  EXPECT_EQ(buffer.lines(), 0);
  EXPECT_TRUE(buffer.ApplyChange(change));
  EXPECT_EQ(buffer.lines(), 3);
  buffer.RequestContent([&](absl::string_view s) {
    EXPECT_EQ("This is now\na multiline\nfile\n", std::string(s));
  });
  EXPECT_EQ(buffer.document_length(), change.text.length());
}

TEST(TextBufferTest, ChangeApplyMultiLine_RemoveLines) {
  EditTextBuffer buffer("Foo\nBar\nBaz\nQuux");
  const TextDocumentContentChangeEvent change = {
      .range =
          {
              .start = {1, 0},
              .end = {3, 0},
          },
      .has_range = true,
      .text = "",
  };
  EXPECT_EQ(buffer.lines(), 4);
  EXPECT_TRUE(buffer.ApplyChange(change));
  EXPECT_EQ(buffer.lines(), 2);
  buffer.RequestContent([&](absl::string_view s) {  //
    EXPECT_EQ("Foo\nQuux", std::string(s));
  });
  EXPECT_EQ(buffer.document_length(), 8);
}

TEST(BufferCollection, SimulateDocumentLifecycleThroughRPC) {
  // Let's walk a BufferCollection through the lifecycle of a document
  // by sending it the JSON RPC notifications for open, change and close.
  //
  // Inspect at each step that the content and number of documents is what
  // we expect.

  // Notifications don't send any responses; write function not relevant.
  JsonRpcDispatcher rpc_dispatcher([](absl::string_view) {});
  BufferCollection collection(&rpc_dispatcher);

  EXPECT_EQ(collection.documents_open(), 0);

  int64_t last_global_version = 0;

  // ------ Opening new document
  // Simulate an incoming event from the client
  rpc_dispatcher.DispatchMessage(R"({
    "jsonrpc":"2.0",
    "method":"textDocument/didOpen",
    "params":{
        "textDocument":{
           "uri": "file:///foo.cc",
           "text": "Hello\nworld",
           "languageId": "cpp",
           "version": 1
         }
    }})");

  // We now expect one document to be open.
  EXPECT_EQ(collection.documents_open(), 1);

  // Let's check that the buffer we have now is indeed stored with that
  // uri and content. Request all buffers since the last remembered version.
  int uri_content_validation_called = 0;
  const int count_changed = collection.MapBuffersChangedSince(
      last_global_version,
      [&](const std::string &uri, const EditTextBuffer &buffer) {
        EXPECT_EQ(uri, "file:///foo.cc");
        buffer.RequestContent([](absl::string_view s) {
          EXPECT_EQ(std::string(s), "Hello\nworld");
        });
        uri_content_validation_called++;
      });

  EXPECT_EQ(count_changed, 1);
  EXPECT_EQ(uri_content_validation_called, 1);

  EXPECT_GT(collection.global_version(), last_global_version);

  // Remember the current global version and confirm that indeed there are
  // no changes since.
  last_global_version = collection.global_version();
  EXPECT_EQ(0, collection.MapBuffersChangedSince(last_global_version, nullptr));

  // ------ Editing document: receive content changes
  rpc_dispatcher.DispatchMessage(R"({
    "jsonrpc":"2.0",
    "method":"textDocument/didChange",
    "params":{
        "textDocument":   { "uri": "file:///foo.cc" },
        "contentChanges": [ {"text":"Hey"} ]
     }})");

  // We expect one buffer to have changed now.
  EXPECT_EQ(1, collection.MapBuffersChangedSince(last_global_version, nullptr));

  collection.findBufferByUri("file:///foo.cc")
      ->RequestContent(
          [](absl::string_view s) { EXPECT_EQ(std::string(s), "Hey"); });

  // ------ Closing document
  rpc_dispatcher.DispatchMessage(R"({
    "jsonrpc":"2.0",
    "method":"textDocument/didClose",
    "params":{
        "textDocument":{ "uri": "file:///foo.cc" }
     }})");

  // No document open anymore
  EXPECT_EQ(collection.documents_open(), 0);
}

}  // namespace lsp
}  // namespace verible
