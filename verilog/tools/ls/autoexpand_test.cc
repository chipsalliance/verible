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

#include "verilog/tools/ls/autoexpand.h"

#include "common/lsp/lsp-protocol.h"
#include "gtest/gtest.h"
#include "verilog/tools/ls/verible-lsp-adapter.h"

namespace verilog {
namespace {
using verible::lsp::CodeActionParams;
using verible::lsp::EditTextBuffer;
using verible::lsp::Range;
using verible::lsp::TextDocumentContentChangeEvent;
using verible::lsp::TextEdit;

void TestTextEdits(
    const std::function<std::vector<TextEdit>(BufferTracker*)>& editFun,
    absl::string_view textBefore, absl::string_view textGolden,
    bool repeat = true) {
  EditTextBuffer buffer(textBefore);
  BufferTracker tracker;
  tracker.Update("<<test>>", buffer);
  std::vector<TextEdit> edits = editFun(&tracker);
  // Sort the TextEdits from the last one in the buffer to the first one. This
  // way we can apply them one by one and have the following ones still be
  // valid.
  // Note: according to the spec, TextEdits should never overlap.
  std::sort(edits.begin(), edits.end(), [](auto& first, auto& second) {
    if (first.range.start.line == second.range.end.line) {
      return first.range.start.character > second.range.end.character;
    }
    return first.range.start.line > second.range.end.line;
  });
  // Sort the TextEdits from the last one in the buffer to the first one. This
  for (const auto& edit : edits) {
    buffer.ApplyChange(TextDocumentContentChangeEvent{
        .range = edit.range, .has_range = true, .text = edit.newText});
  }
  buffer.RequestContent([&](absl::string_view textAfter) {
    EXPECT_EQ(textAfter, textGolden);
    if (repeat) {
      TestTextEdits(editFun, textGolden, textGolden, false);
    }
  });
}

std::vector<TextEdit> AutoExpandCodeActionToTextEdits(BufferTracker* tracker,
                                                      Range range,
                                                      absl::string_view title) {
  CodeActionParams p = {.textDocument = {tracker->current()->uri()},
                        .range = range};
  nlohmann::json changes;
  for (auto& action : GenerateAutoExpandCodeActions(tracker, p)) {
    if (action.title == title) {
      EXPECT_TRUE(changes.empty());
      changes = action.edit.changes;
    }
  }
  EXPECT_FALSE(changes.empty());
  return changes[p.textDocument.uri];
}

TEST(Autoexpand, ExpandEmpty) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module t1(/*AUTOARG*/);
  input logic clk;
  input logic rst;
  output logic o;
endmodule
module t2(/*AUTOARG*/);
  input logic clk;
  input rst;
  output reg o;
endmodule
)",
                R"(
module t1(/*AUTOARG*/
  // Inputs
  clk,
  rst,
  // Outputs
  o
  );
  input logic clk;
  input logic rst;
  output logic o;
endmodule
module t2(/*AUTOARG*/
  // Inputs
  clk,
  rst,
  // Outputs
  o
  );
  input logic clk;
  input rst;
  output reg o;
endmodule
)");
}

TEST(Autoexpand, NoExpand) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module t();
  /*AUTOARG*/
  input logic clk;
  input logic rst;
  output logic o;
endmodule
)",
                R"(
module t();
  /*AUTOARG*/
  input logic clk;
  input logic rst;
  output logic o;
endmodule
)");
}

TEST(Autoexpand, ExpandReplace) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module t(/*AUTOARG*/
  //Inputs
  clk, rst
// some comment
);
  input logic clk;
  input logic rst;
  inout logic io;
  output logic o;
endmodule)",
                R"(
module t(/*AUTOARG*/
  // Inputs
  clk,
  rst,
  // Inouts
  io,
  // Outputs
  o
  );
  input logic clk;
  input logic rst;
  inout logic io;
  output logic o;
endmodule)");
}

TEST(Autoexpand, SkipPredeclared) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module t(i,
         o1, /*AUTOARG*/
//Inputs
clk, rst
);
  input logic clk;
  input logic rst;
  input logic i;
  output logic o1;
  output logic o2;
endmodule)",
                R"(
module t(i,
         o1, /*AUTOARG*/
  // Inputs
  clk,
  rst,
  // Outputs
  o2
  );
  input logic clk;
  input logic rst;
  input logic i;
  output logic o1;
  output logic o2;
endmodule)");
}

TEST(Autoexpand, CodeActionExpandAll) {
  TestTextEdits(
      [](BufferTracker* tracker) {
        return AutoExpandCodeActionToTextEdits(tracker, {},
                                               "Expand all AUTOs in file");
      },
      R"(
module t1(/*AUTOARG*/);
  input logic clk;
  input logic rst;
  output logic o;
endmodule
module t2(/*AUTOARG*/);
  input logic clk;
  input rst;
  output logic o;
endmodule
)",
      R"(
module t1(/*AUTOARG*/
  // Inputs
  clk,
  rst,
  // Outputs
  o
  );
  input logic clk;
  input logic rst;
  output logic o;
endmodule
module t2(/*AUTOARG*/
  // Inputs
  clk,
  rst,
  // Outputs
  o
  );
  input logic clk;
  input rst;
  output logic o;
endmodule
)");
}

TEST(Autoexpand, CodeActionExpandRange) {
  TestTextEdits(
      [](BufferTracker* tracker) {
        return AutoExpandCodeActionToTextEdits(
            tracker, {.start = {.line = 0}, .end = {.line = 1}},
            "Expand this AUTO");
      },
      R"(
module t1(/*AUTOARG*/);
  input logic clk;
  input logic rst;
  output logic o;
endmodule
module t2(/*AUTOARG*/);
  input logic clk;
  input rst;
  output logic o;
endmodule
)",
      R"(
module t1(/*AUTOARG*/
  // Inputs
  clk,
  rst,
  // Outputs
  o
  );
  input logic clk;
  input logic rst;
  output logic o;
endmodule
module t2(/*AUTOARG*/);
  input logic clk;
  input rst;
  output logic o;
endmodule
)");
}

}  // namespace
}  // namespace verilog
