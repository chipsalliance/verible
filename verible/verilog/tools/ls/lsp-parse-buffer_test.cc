// Copyright 2024 The Verible Authors.
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

#include "verible/verilog/tools/ls/lsp-parse-buffer.h"

#include <cstdint>
#include <string>

#include "absl/strings/match.h"
#include "gtest/gtest.h"
#include "verible/common/lsp/lsp-text-buffer.h"
#include "verible/common/text/text-structure.h"

namespace verilog {
namespace {
TEST(BufferTraccker, ParseGoodDocument) {
  verible::lsp::EditTextBuffer good_document("module foo(); endmodule");
  BufferTracker tracker;
  EXPECT_EQ(tracker.current().get(), nullptr);
  tracker.Update("foo.sv", good_document);

  ASSERT_NE(tracker.current().get(), nullptr);

  // Successfully parsed, so last_good() == current()
  EXPECT_EQ(tracker.last_good().get(), tracker.current().get());
}

TEST(BufferTraccker, ParseParseErrorDocument) {
  verible::lsp::EditTextBuffer bad_document("moduleinvalid foo(); endmodule");
  BufferTracker tracker;
  tracker.Update("foo.sv", bad_document);

  ASSERT_NE(tracker.current().get(), nullptr);  // a current document, maybe bad

  // Not successfully parsed, so last_good() is still null.
  ASSERT_EQ(tracker.last_good().get(), nullptr);
}

TEST(BufferTrackerConatainer, PopulateBufferCollection) {
  BufferTrackerContainer container;
  auto feed_callback = container.GetSubscriptionCallback();

  verible::lsp::EditTextBuffer foo_doc("module foo(); endmodule");
  feed_callback("foo.sv", &foo_doc);
  const BufferTracker *tracker = container.FindBufferTrackerOrNull("foo.sv");
  ASSERT_NE(tracker, nullptr);
  EXPECT_NE(tracker->last_good().get(), nullptr);

  verible::lsp::EditTextBuffer bar_doc("modulebroken bar(); endmodule");
  feed_callback("bar.sv", &bar_doc);
  tracker = container.FindBufferTrackerOrNull("bar.sv");
  ASSERT_NE(tracker, nullptr);
  EXPECT_NE(tracker->current().get(), nullptr);    // Current always there
  EXPECT_EQ(tracker->last_good().get(), nullptr);  // Was a bad parse

  feed_callback("bar.sv", nullptr);  // Remove.
  tracker = container.FindBufferTrackerOrNull("bar.sv");
  EXPECT_EQ(tracker, nullptr);
}

TEST(BufferTrackerConatainer, ParseUpdateNotification) {
  BufferTrackerContainer container;

  const verible::TextStructureView *last_text_structure = nullptr;
  int update_remove_count = 0;  // track these
  container.AddChangeListener([&update_remove_count, &last_text_structure](
                                  const std::string &,
                                  const BufferTracker *tracker) {
    if (tracker != nullptr) {
      ++update_remove_count;
    } else {
      --update_remove_count;
    };

    // attempt to access the previous text structure if it was not null to
    // make sure it was not deleted. If it was, then this UB might only be
    // detected in the ASAN test.
    if (last_text_structure) {
      // Accessing the content and see if it is alive and well, not corrupted.
      EXPECT_TRUE(absl::StartsWith(last_text_structure->Contents(), "module"));
    }

    // Remember current text structure for last time.
    if (tracker) {
      last_text_structure = &tracker->current()->parser().Data();
    } else {
      last_text_structure = nullptr;
    }
  });

  EXPECT_EQ(update_remove_count, 0);

  int64_t version_number = 0;
  auto feed_callback = container.GetSubscriptionCallback();
  // Put one document in there.
  verible::lsp::EditTextBuffer foo_doc("module foo(); endmodule");
  foo_doc.set_last_global_version(++version_number);
  feed_callback("foo.sv", &foo_doc);

  EXPECT_EQ(update_remove_count, 1);

  const BufferTracker *tracker = container.FindBufferTrackerOrNull("foo.sv");
  ASSERT_NE(tracker, nullptr);
  EXPECT_NE(tracker->last_good().get(), nullptr);

  verible::lsp::EditTextBuffer updated_foo_doc("module foobar(); endmodule");
  updated_foo_doc.set_last_global_version(++version_number);
  feed_callback("foo.sv", &updated_foo_doc);
  EXPECT_EQ(update_remove_count, 2);

  // Remove doc
  feed_callback("foo.sv", nullptr);
  EXPECT_EQ(update_remove_count, 1);
}

}  // namespace
}  // namespace verilog
