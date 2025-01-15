// Copyright 2023 The Verible Authors.
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

#include "verible/verilog/tools/ls/autoexpand.h"

#include <algorithm>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "verible/common/lsp/lsp-protocol.h"
#include "verible/common/lsp/lsp-text-buffer.h"
#include "verible/common/text/text-structure.h"
#include "verible/verilog/analysis/verilog-analyzer.h"
#include "verible/verilog/analysis/verilog-project.h"
#include "verible/verilog/formatting/format-style-init.h"
#include "verible/verilog/formatting/format-style.h"
#include "verible/verilog/formatting/formatter.h"
#include "verible/verilog/tools/ls/lsp-parse-buffer.h"
#include "verible/verilog/tools/ls/symbol-table-handler.h"

namespace verilog {
namespace {
using verible::TextStructureView;
using verible::lsp::CodeAction;
using verible::lsp::CodeActionParams;
using verible::lsp::EditTextBuffer;
using verible::lsp::Range;
using verible::lsp::TextDocumentContentChangeEvent;
using verible::lsp::TextEdit;

// Generate a specific code action and extract text edits from it
std::vector<TextEdit> AutoExpandCodeActionToTextEdits(
    SymbolTableHandler *symbol_table_handler, const BufferTracker *tracker,
    Range range, std::string_view title) {
  CodeActionParams p = {.textDocument = {tracker->current()->uri()},
                        .range = range};
  nlohmann::json changes;
  for (const CodeAction &action :
       GenerateAutoExpandCodeActions(symbol_table_handler, tracker, p)) {
    if (action.title == title) {
      EXPECT_TRUE(changes.empty());
      changes = action.edit.changes;
    }
  }
  if (changes.empty()) return {};
  return changes[p.textDocument.uri];
}

// Generate text edits from a full AUTO expansion
std::vector<TextEdit> GenerateFullAutoExpandTextEdits(
    SymbolTableHandler *symbol_table_handler, const BufferTracker *tracker) {
  EXPECT_TRUE(tracker);
  const auto current = tracker->current();
  EXPECT_TRUE(current);
  const TextStructureView &text_structure = current->parser().Data();
  return AutoExpandCodeActionToTextEdits(
      symbol_table_handler, tracker,
      {.start = {.line = 0},
       .end = {.line = static_cast<int>(text_structure.Lines().size())}},
      "Expand all AUTOs in file");
}

// Determines how TextTextEdits* should test a function
struct TestRun {
  using EditFn = std::function<std::vector<TextEdit>(SymbolTableHandler *,
                                                     BufferTracker *)>;
  const EditFn edit_fn = GenerateFullAutoExpandTextEdits;  // Function that
                                                           // generates text
                                                           // edits to check
  bool check_golden = true;      // If true, the edited text is compared against
                                 // golden
  bool check_formatting = true;  // If true, the formatting of the edited text
                                 // is checked
  bool check_syntax = true;      // If true, the syntax of the edited text is
                                 // checked

  std::deque<TestRun> repeat() const { return {*this, *this}; }
  static std::deque<TestRun> defaultRuns() { return {TestRun{}, TestRun{}}; }
};

// Checks if the given Verilog source has correct syntax
void CheckSyntax(const std::string_view filename, const std::string_view text) {
  verilog::VerilogAnalyzer analyzer(text, filename);
  const absl::Status status = analyzer.Analyze();
  ASSERT_TRUE(status.ok());
}

// Helper function that formats the given Verilog string
std::string Format(const std::string_view filename,
                   const std::string_view text_before_formatting) {
  std::stringstream strstr;
  formatter::FormatStyle format_style;
  formatter::InitializeFromFlags(&format_style);
  // AUTO expansion does not handle this alignment
  const absl::Status status = formatter::FormatVerilog(
      text_before_formatting, filename, format_style, strstr);
  EXPECT_TRUE(status.ok());
  return strstr.str();
}

// Generate text edits using the given function and test if they had the desired
// effect
void TestTextEditsWithProject(

    const std::vector<std::string_view> &project_file_contents,
    std::string_view text_before, const std::string_view text_golden,
    std::deque<TestRun> runs = TestRun::defaultRuns()) {
  if (runs.empty()) return;
  const auto &run = runs.front();
  static const char *TESTED_FILENAME = "<<tested-file>>";
  // Create a Verilog project with the given project file contents
  const std::shared_ptr<VerilogProject> proj =
      std::make_shared<VerilogProject>(".", std::vector<std::string>());
  size_t i = 0;
  for (const std::string_view file_contents : project_file_contents) {
    const std::string filename = absl::StrCat("<<project-file-", i, ">>");
    proj->AddVirtualFile(filename, Format(filename, file_contents));
    i++;
  }
  // Init a text buffer which we need for the autoexpand functions
  std::string formatted_text_before;
  if (run.check_formatting) {
    formatted_text_before = Format(TESTED_FILENAME, text_before);
    text_before = formatted_text_before;
  }
  EditTextBuffer buffer(text_before);
  BufferTracker tracker;
  tracker.Update(TESTED_FILENAME, buffer);
  // Init a symbol table handler which is also needed for certain AUTO
  // expansions. This handler also needs a Verilog project to work properly.
  SymbolTableHandler symbol_table_handler;
  symbol_table_handler.SetProject(proj);
  symbol_table_handler.UpdateFileContent(TESTED_FILENAME,
                                         &tracker.current()->parser());
  symbol_table_handler.BuildProjectSymbolTable();
  // Run the tested edit function
  std::vector<TextEdit> edits = run.edit_fn(&symbol_table_handler, &tracker);
  // Sort the TextEdits from the last one in the buffer to the first one. This
  // way we can apply them one by one and have the following ones still be
  // valid.
  // Note: according to the spec, TextEdits should never overlap.
  std::sort(edits.rbegin(), edits.rend(),
            [](const TextEdit &first, const TextEdit &second) {
              if (first.range.end.line == second.range.start.line) {
                return first.range.end.character < second.range.start.character;
              }
              return first.range.end.line < second.range.start.line;
            });
  // Apply the text edits
  for (const TextEdit &edit : edits) {
    buffer.ApplyChange(TextDocumentContentChangeEvent{
        .range = edit.range, .has_range = true, .text = edit.newText});
  }
  // Check the result and (possibly) test again to check idempotence
  buffer.RequestContent([&](const std::string_view text_after) {
    if (run.check_golden) {
      if (run.check_formatting) {
        // TODO: Check multiple formatting styles
        const std::string formatted_text_golden =
            Format(TESTED_FILENAME, text_golden);
        EXPECT_EQ(formatted_text_golden, text_after);
      } else {
        EXPECT_EQ(text_golden, text_after);
      }
      if (run.check_syntax) CheckSyntax(TESTED_FILENAME, text_after);
    }
    if (runs.size() > 1) {
      runs.pop_front();
      TestTextEditsWithProject(project_file_contents, text_golden, text_golden,
                               runs);
    }
  });
}

// Same as above, without the project file parameter
void TestTextEdits(const std::string_view text_before,
                   const std::string_view text_golden,
                   const std::deque<TestRun> &runs = TestRun::defaultRuns()) {
  TestTextEditsWithProject({}, text_before, text_golden, runs);
}

TEST(Autoexpand, AUTOARG_ExpandEmpty) {
  TestTextEdits(
      R"(
module t1 (  /*AUTOARG*/);
  input logic clk;
  input logic rst;
  output logic o;
endmodule
module t2 (  /*AUTOARG*/);
  input logic clk;
  input rst;
  output reg o;
endmodule
)",
      R"(
module t1 (  /*AUTOARG*/
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
module t2 (  /*AUTOARG*/
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

TEST(Autoexpand, AUTOARG_NoExpand) {
  TestTextEdits(
      R"(
module t (
    .o(out  /*AUTOARG*/)
);
  /*AUTOARG*/
  input logic clk;
  input logic rst;
  output logic o;
endmodule
)",
      R"(
module t (
    .o(out  /*AUTOARG*/)
);
  /*AUTOARG*/
  input logic clk;
  input logic rst;
  output logic o;
endmodule
)");
}

TEST(Autoexpand, AUTOARG_Replace) {
  TestTextEdits(
      R"(
module t (  /*AUTOARG*/
    //Inputs
    clk,
    rst
    // some comment
);
  input logic clk;
  input logic rst;
  inout logic io;
  output logic o;
endmodule
)",
      R"(
module t (  /*AUTOARG*/
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
endmodule
)");
}

TEST(Autoexpand, AUTOARG_SkipPredeclared) {
  TestTextEdits(
      R"(
module t (
    input i1,
    i2,
    o1,  /*AUTOARG*/
    //Inputs
    clk,
    rst
);
  input logic clk;
  input logic rst;
  input logic i2;
  output logic o1;
  output logic o2;
endmodule
)",
      R"(
module t (
    input i1,
    i2,
    o1,  /*AUTOARG*/
    // Inputs
    clk,
    rst,
    // Outputs
    o2
);
  input logic clk;
  input logic rst;
  input logic i2;
  output logic o1;
  output logic o2;
endmodule
)");
}

TEST(Autoexpand, AUTOINST_ExpandEmpty) {
  TestTextEdits(
      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule

module foo;
  inout [7:0][7:0] io;

  bar b (  /*AUTOINST*/);
endmodule
)",
      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule

module foo;
  inout [7:0][7:0] io;

  bar b (  /*AUTOINST*/
      // Inputs
      .i1(i1),
      .i2(i2  /*.[4][8]*/),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o1(o1[15:0]),
      .o2(o2  /*[31:0].[8]*/)
  );
endmodule
)");
}

TEST(Autoexpand, AUTOINST_NoExpand) {
  TestTextEdits(
      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  output [31:0] o2[8];
endmodule

module foo;
  inout logic io;

  bar b ();
  /*AUTOINST*/
  bar c (.i1(io  /*AUTOINST*/));
endmodule
)",
      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  output [31:0] o2[8];
endmodule

module foo;
  inout logic io;

  bar b ();
  /*AUTOINST*/
  bar c (.i1(io  /*AUTOINST*/));
endmodule
)");

  TestTextEdits(
      R"(
module foo;
  bar b (  /*AUTOINST*/);
endmodule
)",
      R"(
module foo;
  bar b (  /*AUTOINST*/);
endmodule
)");
}

TEST(Autoexpand, AUTOINST_Replace) {
  TestTextEdits(
      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  output [31:0] o2[8];
endmodule

module foo;
  inout logic io;

  bar b (  /*AUTOINST*/
      .i1(i1),
      // Outputs
      .o1(o1),
      .o2(o2)
  );
endmodule
)",
      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  output [31:0] o2[8];
endmodule

module foo;
  inout logic io;

  bar b (  /*AUTOINST*/
      // Inputs
      .i1(i1),
      .i2(i2  /*.[4][8]*/),
      // Outputs
      .o1(o1[15:0]),
      .o2(o2  /*[31:0].[8]*/)
  );
endmodule
)");
}

TEST(Autoexpand, AUTOINST_SkipPreConnected) {
  TestTextEdits(
      R"(
module foo;
  inout logic io;

  bar b (  // This comment is to get around formatting issues. AUTOINST expansion is currently
      // unable to add a newline at connection list opening param.
      // TODO: fix for formatting stability
      .i1(io),  /*AUTOINST*/
  );
endmodule

module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  output [31:0] o2[8];
endmodule
)",
      R"(
module foo;
  inout logic io;

  bar b (  // This comment is to get around formatting issues. AUTOINST expansion is currently
      // unable to add a newline at connection list opening param.
      // TODO: fix for formatting stability
      .i1(io),  /*AUTOINST*/
      // Inputs
      .i2(i2  /*.[4][8]*/),
      // Outputs
      .o1(o1[15:0]),
      .o2(o2  /*[31:0].[8]*/)
  );
endmodule

module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  output [31:0] o2[8];
endmodule
)");
}

TEST(Autoexpand, AUTOINST_Missing) {
  TestTextEdits(
      R"(
module foo;
  bar b (  /*AUTOINST*/);
endmodule
)",
      R"(
module foo;
  bar b (  /*AUTOINST*/);
endmodule
)");
}

TEST(Autoexpand, AUTOINST_Ambiguous) {
  TestTextEdits(
      R"(
module bar (
    input  i1,
    output o1
);
endmodule

module bar (
    input  i2,
    output o2
);
endmodule

module foo;
  bar b (  /*AUTOINST*/);
endmodule
)",
      R"(
module bar (
    input  i1,
    output o1
);
endmodule

module bar (
    input  i2,
    output o2
);
endmodule

module foo;
  bar b (  /*AUTOINST*/
      // Inputs
      .i1(i1),
      // Outputs
      .o1(o1)
  );
endmodule
)");
}

TEST(Autoexpand, AUTOINST_Chain) {
  TestTextEdits(
      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  inout [7:0][7:0] io;
  output [31:0] o2[8];

  qux q (  /*AUTOINST*/);
endmodule

module foo;
  inout logic io;

  bar b (  /*AUTOINST*/);
endmodule

module qux;
  input i1;
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule
)",
      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  inout [7:0][7:0] io;
  output [31:0] o2[8];

  qux q (  /*AUTOINST*/
      // Inputs
      .i1(i1),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o2(o2  /*[31:0].[8]*/)
  );
endmodule

module foo;
  inout logic io;

  bar b (  /*AUTOINST*/
      // Inputs
      .i1(i1),
      .i2(i2  /*.[4][8]*/),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o1(o1[15:0]),
      .o2(o2  /*[31:0].[8]*/)
  );
endmodule

module qux;
  input i1;
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule
)");
}

TEST(Autoexpand, AUTOINST_MultipleFiles) {
  TestTextEditsWithProject({R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule
)",
                            R"(
module qux;
  input i1;
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule
)"},
                           R"(
module foo;
  bar b (  /*AUTOINST*/);
  qux q (  /*AUTOINST*/);
endmodule
)",
                           R"(
module foo;
  bar b (  /*AUTOINST*/
      // Inputs
      .i1(i1),
      .i2(i2  /*.[4][8]*/),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o1(o1[15:0]),
      .o2(o2  /*[31:0].[8]*/)
  );
  qux q (  /*AUTOINST*/
      // Inputs
      .i1(i1),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o2(o2  /*[31:0].[8]*/)
  );
endmodule
)");
}

TEST(Autoexpand, AUTO_TEMPLATE_Simple) {
  TestTextEdits(
      R"(
module foo;
  /* bar AUTO_TEMPLATE (
         .i1(in_a[]),
         .o2(out_b[])
     ); */
  bar b (  /*AUTOINST*/);
endmodule

module bar;
  input i1;
  input i2[4][8];
  inout [7:0][7:0] io;
  output [15:0] o1;
  output [31:0] o2[8];
endmodule
)",
      R"(
module foo;
  /* bar AUTO_TEMPLATE (
         .i1(in_a[]),
         .o2(out_b[])
     ); */
  bar b (  /*AUTOINST*/
      // Inputs
      .i1(in_a),
      .i2(i2  /*.[4][8]*/),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o1(o1[15:0]),
      .o2(out_b  /*[31:0].[8]*/)
  );
endmodule

module bar;
  input i1;
  input i2[4][8];
  inout [7:0][7:0] io;
  output [15:0] o1;
  output [31:0] o2[8];
endmodule
)");
}

TEST(Autoexpand, AUTO_TEMPLATE_SkipPreConnected) {
  TestTextEdits(
      R"(
module foo;
  /* bar AUTO_TEMPLATE (
         .i1(in_a),
         .o2(out_b)
     ); */
  bar b (  // This comment is to get around formatting issues. AUTOINST expansion is currently
      // unable to add a newline at connection list opening param.
      // TODO: fix for formatting stability
      .i1(input_1),  /*AUTOINST*/
  );
endmodule

module bar;
  input i1;
  input i2[4][8];
  inout [7:0][7:0] io;
  output [15:0] o1;
  output [31:0] o2[8];
endmodule
)",
      R"(
module foo;
  /* bar AUTO_TEMPLATE (
         .i1(in_a),
         .o2(out_b)
     ); */
  bar b (  // This comment is to get around formatting issues. AUTOINST expansion is currently
      // unable to add a newline at connection list opening param.
      // TODO: fix for formatting stability
      .i1(input_1),  /*AUTOINST*/
      // Inputs
      .i2(i2  /*.[4][8]*/),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o1(o1[15:0]),
      .o2(out_b)
  );
endmodule

module bar;
  input i1;
  input i2[4][8];
  inout [7:0][7:0] io;
  output [15:0] o1;
  output [31:0] o2[8];
endmodule
)");
}

TEST(Autoexpand, AUTO_TEMPLATE_MultipleMatches) {
  TestTextEdits(
      R"(
module foo;
  /* qux AUTO_TEMPLATE
     quux AUTO_TEMPLATE
     bar AUTO_TEMPLATE (
         .i1(in_a),
         .o2(out_b[])); */
  qux q (  /*AUTOINST*/);
  bar b (  /*AUTOINST*/);
endmodule

module bar;
  input i1;
  input i2[4][8];
  inout [7:0][7:0] io;
  output [15:0] o1;
  output [31:0] o2[8];
endmodule

module qux;
  input i1;
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule
)",
      R"(
module foo;
  /* qux AUTO_TEMPLATE
     quux AUTO_TEMPLATE
     bar AUTO_TEMPLATE (
         .i1(in_a),
         .o2(out_b[])); */
  qux q (  /*AUTOINST*/
      // Inputs
      .i1(in_a),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o2(out_b  /*[31:0].[8]*/)
  );
  bar b (  /*AUTOINST*/
      // Inputs
      .i1(in_a),
      .i2(i2  /*.[4][8]*/),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o1(o1[15:0]),
      .o2(out_b  /*[31:0].[8]*/)
  );
endmodule

module bar;
  input i1;
  input i2[4][8];
  inout [7:0][7:0] io;
  output [15:0] o1;
  output [31:0] o2[8];
endmodule

module qux;
  input i1;
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule
)");
}

TEST(Autoexpand, AUTO_TEMPLATE_Override) {
  TestTextEdits(
      R"(
module foo;
  /* qux AUTO_TEMPLATE
     bar AUTO_TEMPLATE (
         .i1(in_a[]),
         .o2(out_b[])); */
  qux q (  /*AUTOINST*/);

  /* bar AUTO_TEMPLATE (
         .i1(input_1[]),
         .o2(output_2),
         .i2(input_2[]),
         .io(input_output),
         .o1(output_1[])); */
  bar b (  /*AUTOINST*/);
endmodule

module bar;
  input i1;
  input i2[4][8];
  inout [7:0][7:0] io;
  output [15:0] o1;
  output [31:0] o2[8];
endmodule

module qux;
  input i1;
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule
)",
      R"(
module foo;
  /* qux AUTO_TEMPLATE
     bar AUTO_TEMPLATE (
         .i1(in_a[]),
         .o2(out_b[])); */
  qux q (  /*AUTOINST*/
      // Inputs
      .i1(in_a),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o2(out_b  /*[31:0].[8]*/)
  );

  /* bar AUTO_TEMPLATE (
         .i1(input_1[]),
         .o2(output_2),
         .i2(input_2[]),
         .io(input_output),
         .o1(output_1[])); */
  bar b (  /*AUTOINST*/
      // Inputs
      .i1(input_1),
      .i2(input_2  /*.[4][8]*/),
      // Inouts
      .io(input_output),
      // Outputs
      .o1(output_1[15:0]),
      .o2(output_2)
  );
endmodule

module bar;
  input i1;
  input i2[4][8];
  inout [7:0][7:0] io;
  output [15:0] o1;
  output [31:0] o2[8];
endmodule

module qux;
  input i1;
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule
)");
}

TEST(Autoexpand, AUTO_TEMPLATE_Mismatch) {
  TestTextEdits(
      R"(
module foo;
  /* quux AUTO_TEMPLATE
     bar AUTO_TEMPLATE (
         .i1(in_a[]),
         .o2(out_b[])); */
  qux q (  /*AUTOINST*/);
  bar b (  /*AUTOINST*/);
endmodule

module bar;
  input i1;
  input i2[4][8];
  inout [7:0][7:0] io;
  output [15:0] o1;
  output [31:0] o2[8];
endmodule

module qux;
  input i1;
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule
)",
      R"(
module foo;
  /* quux AUTO_TEMPLATE
     bar AUTO_TEMPLATE (
         .i1(in_a[]),
         .o2(out_b[])); */
  qux q (  /*AUTOINST*/
      // Inputs
      .i1(i1),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o2(o2  /*[31:0].[8]*/)
  );
  bar b (  /*AUTOINST*/
      // Inputs
      .i1(in_a),
      .i2(i2  /*.[4][8]*/),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o1(o1[15:0]),
      .o2(out_b  /*[31:0].[8]*/)
  );
endmodule

module bar;
  input i1;
  input i2[4][8];
  inout [7:0][7:0] io;
  output [15:0] o1;
  output [31:0] o2[8];
endmodule

module qux;
  input i1;
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule
)");
}

TEST(Autoexpand, AUTO_TEMPLATE_Regex) {
  TestTextEdits(
      R"(
module foo;
  /* qux AUTO_TEMPLATE
     bar AUTO_TEMPLATE "[bq]" (
         .i1(in_a[]),
         .o2(out_b[])); */
  qux q (  /*AUTOINST*/);

  /* bar AUTO_TEMPLATE "c" (
         .i1(input_1[]),
         .o2(output_2),
         .i2(input_2[]),
         .io(input_output),
         .o1(output_1[])); */
  bar b (  /*AUTOINST*/);
endmodule

module bar;
  input i1;
  input i2[4][8];
  inout [7:0][7:0] io;
  output [15:0] o1;
  output [31:0] o2[8];
endmodule

module qux;
  input i1;
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule
)",
      R"(
module foo;
  /* qux AUTO_TEMPLATE
     bar AUTO_TEMPLATE "[bq]" (
         .i1(in_a[]),
         .o2(out_b[])); */
  qux q (  /*AUTOINST*/
      // Inputs
      .i1(in_a),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o2(out_b  /*[31:0].[8]*/)
  );

  /* bar AUTO_TEMPLATE "c" (
         .i1(input_1[]),
         .o2(output_2),
         .i2(input_2[]),
         .io(input_output),
         .o1(output_1[])); */
  bar b (  /*AUTOINST*/
      // Inputs
      .i1(in_a),
      .i2(i2  /*.[4][8]*/),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o1(o1[15:0]),
      .o2(out_b /*[31:0].[8]*/)
  );
endmodule

module bar;
  input i1;
  input i2[4][8];
  inout [7:0][7:0] io;
  output [15:0] o1;
  output [31:0] o2[8];
endmodule

module qux;
  input i1;
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule
)");
}

TEST(Autoexpand, AUTO_TEMPLATE_InvalidRegex) {
  TestTextEdits(
      R"(
module foo;
  /* bar AUTO_TEMPLATE "[bq" (
         .i1(in_a[]),
         .o2(out_b[])); */
  bar b (  /*AUTOINST*/);
endmodule

module bar;
  input i1;
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule
)",
      R"(
module foo;
  /* bar AUTO_TEMPLATE "[bq" (
         .i1(in_a[]),
         .o2(out_b[])); */
  bar b (  /*AUTOINST*/
      // Inputs
      .i1(i1),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o2(o2  /*[31:0].[8]*/)
  );
endmodule

module bar;
  input i1;
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule
)");
}

TEST(Autoexpand, AUTO_TEMPLATE_Subst) {
  TestTextEdits(
      R"(
module foo;
  /* qux AUTO_TEMPLATE
     bar AUTO_TEMPLATE "[bq]" (
         .i1(@_in[]),
         .o2(@_out[])); */
  qux q (  /*AUTOINST*/);

  /* bar AUTO_TEMPLATE "c" (
         .i1(input_1[]),
         .o2(output_2),
         .i2(input_2[]),
         .io(input_output),
         .o1(output_1[])); */
  bar b (  /*AUTOINST*/);
endmodule

module bar;
  input i1;
  input i2[4][8];
  inout [7:0][7:0] io;
  output [15:0] o1;
  output [31:0] o2[8];
endmodule

module qux;
  input i1;
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule
)",
      R"(
module foo;
  /* qux AUTO_TEMPLATE
     bar AUTO_TEMPLATE "[bq]" (
         .i1(@_in[]),
         .o2(@_out[])); */
  qux q (  /*AUTOINST*/
      // Inputs
      .i1(q_in),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o2(q_out  /*[31:0].[8]*/)
  );

  /* bar AUTO_TEMPLATE "c" (
         .i1(input_1[]),
         .o2(output_2),
         .i2(input_2[]),
         .io(input_output),
         .o1(output_1[])); */
  bar b (  /*AUTOINST*/
      // Inputs
      .i1(b_in),
      .i2(i2  /*.[4][8]*/),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o1(o1[15:0]),
      .o2(b_out /*[31:0].[8]*/)
  );
endmodule

module bar;
  input i1;
  input i2[4][8];
  inout [7:0][7:0] io;
  output [15:0] o1;
  output [31:0] o2[8];
endmodule

module qux;
  input i1;
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule
)");
}

TEST(Autoexpand, AUTOINPUT_ExpandEmpty) {
  TestTextEdits(
      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule

module foo;
  /*AUTOINPUT*/

  input i3;

  bar b (  /*AUTOINST*/);
endmodule
)",
      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule

module foo;
  /*AUTOINPUT*/
  // Beginning of automatic inputs (from autoinst inputs)
  input i1;  // To b of bar
  input i2[4][8];  // To b of bar
  // End of automatics

  input i3;

  bar b (  /*AUTOINST*/
      // Inputs
      .i1(i1),
      .i2(i2  /*.[4][8]*/),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o1(o1[15:0]),
      .o2(o2  /*[31:0].[8]*/)
  );
endmodule
)");
}

TEST(Autoexpand, AUTOINPUT_NoExpand) {
  TestTextEdits(
      R"(
module bar;
endmodule

module foo;
  input i;
  output o;

  /*AUTOINPUT*/

  bar b (  /*AUTOINST*/);
endmodule

module qux;
  output o  /*AUTOINPUT*/
  ;

  foo f (  /*AUTOINST*/);
endmodule
)",
      R"(
module bar;
endmodule

module foo;
  input i;
  output o;

  /*AUTOINPUT*/

  bar b (  /*AUTOINST*/);
endmodule

module qux;
  output o  /*AUTOINPUT*/
  ;

  foo f (  /*AUTOINST*/
      // Inputs
      .i(i),
      // Outputs
      .o(o)
  );
endmodule
)");
}

TEST(Autoexpand, AUTOINPUT_Replace) {
  TestTextEdits(
      R"(
module bar (
    input  i1,
    output o1
);
  input i2;
  inout [7:0][7:0] io;
  output o2;
endmodule

module foo;
  /*AUTOINPUT*/
  // Beginning of automatic inputs (from autoinst inputs)
  input in_1;  // To b of bar
  input in_2;  // To b of bar
  // End of automatics

  bar b (  /*AUTOINST*/);
endmodule
)",
      R"(
module bar (
    input  i1,
    output o1
);
  input i2;
  inout [7:0][7:0] io;
  output o2;
endmodule

module foo;
  /*AUTOINPUT*/
  // Beginning of automatic inputs (from autoinst inputs)
  input i1;  // To b of bar
  input i2;  // To b of bar
  // End of automatics

  bar b (  /*AUTOINST*/
      // Inputs
      .i1(i1),
      .i2(i2),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o1(o1),
      .o2(o2)
  );
endmodule
)");
}

TEST(Autoexpand, AUTOINOUT_ExpandEmpty) {
  TestTextEdits(
      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  inout io1;
  output [31:0] o2[8];
endmodule

module foo;
  /*AUTOINOUT*/

  inout io2;

  bar b (  /*AUTOINST*/);
endmodule
)",
      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  inout io1;
  output [31:0] o2[8];
endmodule

module foo;
  /*AUTOINOUT*/
  // Beginning of automatic inouts (from autoinst inouts)
  inout io1;  // To/From b of bar
  // End of automatics

  inout io2;

  bar b (  /*AUTOINST*/
      // Inputs
      .i1 (i1),
      .i2 (i2  /*.[4][8]*/),
      // Inouts
      .io1(io1),
      // Outputs
      .o1 (o1[15:0]),
      .o2 (o2  /*[31:0].[8]*/)
  );
endmodule
)");
}

TEST(Autoexpand, AUTOINOUT_NoExpand) {
  TestTextEdits(
      R"(
module bar;
endmodule

module foo;
  input i;
  inout io;
  output o;

  /*AUTOINOUT*/

  bar b (  /*AUTOINST*/);
endmodule

module qux;
  output o  /*AUTOINOUT*/
  ;

  foo f (  /*AUTOINST*/);
endmodule
)",
      R"(
module bar;
endmodule

module foo;
  input i;
  inout io;
  output o;

  /*AUTOINOUT*/

  bar b (  /*AUTOINST*/);
endmodule

module qux;
  output o  /*AUTOINOUT*/
  ;

  foo f (  /*AUTOINST*/
      // Inputs
      .i(i),
      // Inouts
      .io(io),
      // Outputs
      .o(o)
  );
endmodule
)");
}

TEST(Autoexpand, AUTOINOUT_Replace) {
  TestTextEdits(
      R"(
module bar (
    input  i1,
    output o1
);
  input i2;
  inout [7:0][7:0] io;
  output o2;
endmodule

module foo;
  /*AUTOINOUT*/
  // Beginning of automatic inouts (from autoinst inouts)
  input in_out;  // To/From b of bar
  // End of automatics

  bar b (  /*AUTOINST*/);
endmodule
)",
      R"(
module bar (
    input  i1,
    output o1
);
  input i2;
  inout [7:0][7:0] io;
  output o2;
endmodule

module foo;
  /*AUTOINOUT*/
  // Beginning of automatic inouts (from autoinst inouts)
  inout [7:0][7:0] io;  // To/From b of bar
  // End of automatics

  bar b (  /*AUTOINST*/
      // Inputs
      .i1(i1),
      .i2(i2),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o1(o1),
      .o2(o2)
  );
endmodule
)");
}

TEST(Autoexpand, AUTOOUTPUT_ExpandEmpty) {
  TestTextEdits(
      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule

module foo;
  /*AUTOOUTPUT*/

  output o3;

  bar b (  /*AUTOINST*/);
endmodule
)",
      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule

module foo;
  /*AUTOOUTPUT*/
  // Beginning of automatic outputs (from autoinst outputs)
  output [15:0] o1;  // From b of bar
  output [31:0] o2[8];  // From b of bar
  // End of automatics

  output o3;

  bar b (  /*AUTOINST*/
      // Inputs
      .i1(i1),
      .i2(i2  /*.[4][8]*/),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o1(o1[15:0]),
      .o2(o2  /*[31:0].[8]*/)
  );
endmodule
)");
}

TEST(Autoexpand, AUTOOUTPUT_NoExpand) {
  TestTextEdits(
      R"(
module bar;
endmodule

module foo;
  input i;
  output o;

  /*AUTOOUTPUT*/

  bar b (  /*AUTOINST*/);
endmodule

module qux;
  output o  /*AUTOOUTPUT*/
  ;

  foo f (  /*AUTOINST*/);
endmodule
)",
      R"(
module bar;
endmodule

module foo;
  input i;
  output o;

  /*AUTOOUTPUT*/

  bar b (  /*AUTOINST*/);
endmodule

module qux;
  output o  /*AUTOOUTPUT*/
  ;

  foo f (  /*AUTOINST*/
      // Inputs
      .i(i),
      // Outputs
      .o(o)
  );
endmodule
)");
}

TEST(Autoexpand, AUTOOUTPUT_Replace) {
  TestTextEdits(
      R"(
module bar (
    input  i1,
    output o1
);
  input i2;
  inout [7:0][7:0] io;
  output o2;
endmodule

module foo;
  /*AUTOOUTPUT*/
  // Beginning of automatic outputs (from autoinst outputs)
  output out_1;  // From b of bar
  output out_2;  // From b of bar
  // End of automatics

  bar b (  /*AUTOINST*/);
endmodule
)",
      R"(
module bar (
    input  i1,
    output o1
);
  input i2;
  inout [7:0][7:0] io;
  output o2;
endmodule

module foo;
  /*AUTOOUTPUT*/
  // Beginning of automatic outputs (from autoinst outputs)
  output o1;  // From b of bar
  output o2;  // From b of bar
  // End of automatics

  bar b (  /*AUTOINST*/
      // Inputs
      .i1(i1),
      .i2(i2),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o1(o1),
      .o2(o2)
  );
endmodule
)");
}

TEST(Autoexpand, AUTO_ExpandPorts) {
  TestTextEdits(
      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule

module foo (  /*AUTOARG*/);
  /*AUTOINPUT*/
  /*AUTOOUTPUT*/
  /*AUTOINOUT*/

  bar b (  /*AUTOINST*/);
endmodule
)",
      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule

module foo (  /*AUTOARG*/
    // Inputs
    i1,
    i2,
    // Inouts
    io,
    // Outputs
    o1,
    o2
);
  /*AUTOINPUT*/
  // Beginning of automatic inputs (from autoinst inputs)
  input i1;  // To b of bar
  input i2[4][8];  // To b of bar
  // End of automatics
  /*AUTOOUTPUT*/
  // Beginning of automatic outputs (from autoinst outputs)
  output [15:0] o1;  // From b of bar
  output [31:0] o2[8];  // From b of bar
  // End of automatics
  /*AUTOINOUT*/
  // Beginning of automatic inouts (from autoinst inouts)
  inout [7:0][7:0] io;  // To/From b of bar
  // End of automatics

  bar b (  /*AUTOINST*/
      // Inputs
      .i1(i1),
      .i2(i2  /*.[4][8]*/),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o1(o1[15:0]),
      .o2(o2  /*[31:0].[8]*/)
  );
endmodule
)");
}

TEST(Autoexpand, AUTO_ExpandPortsMultipleConnections) {
  TestTextEdits(
      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule

module qux (
    input [3:0] i1,
    output [63:0] o1
);
  input i2[8][4];
  inout [3:0][15:0] io;
  output [15:0] o2[16];
endmodule

module foo (  /*AUTOARG*/);
  /*AUTOINPUT*/
  /*AUTOOUTPUT*/
  /*AUTOINOUT*/

  bar b1 (  /*AUTOINST*/);
  bar b2 (  /*AUTOINST*/);
  qux q (  /*AUTOINST*/);
endmodule
)",
      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule

module qux (
    input [3:0] i1,
    output [63:0] o1
);
  input i2[8][4];
  inout [3:0][15:0] io;
  output [15:0] o2[16];
endmodule

module foo (  /*AUTOARG*/
    // Inputs
    i1,
    i2,
    // Inouts
    io,
    // Outputs
    o1,
    o2
);
  /*AUTOINPUT*/
  // Beginning of automatic inputs (from autoinst inputs)
  input [3:0] i1;  // To b1 of bar, ...
  input i2[4][8];  // To b1 of bar, ...
  // End of automatics
  /*AUTOOUTPUT*/
  // Beginning of automatic outputs (from autoinst outputs)
  output [63:0] o1;  // From b1 of bar, ...
  output [31:0] o2[8];  // From b1 of bar, ...
  // End of automatics
  /*AUTOINOUT*/
  // Beginning of automatic inouts (from autoinst inouts)
  inout [7:0][15:0] io;  // To/From b1 of bar, ...
  // End of automatics

  bar b1 (  /*AUTOINST*/
      // Inputs
      .i1(i1),
      .i2(i2  /*.[4][8]*/),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o1(o1[15:0]),
      .o2(o2  /*[31:0].[8]*/)
  );
  bar b2 (  /*AUTOINST*/
      // Inputs
      .i1(i1),
      .i2(i2  /*.[4][8]*/),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o1(o1[15:0]),
      .o2(o2  /*[31:0].[8]*/)
  );
  qux q (  /*AUTOINST*/
      // Inputs
      .i1(i1[3:0]),
      .i2(i2  /*.[8][4]*/),
      // Inouts
      .io(io  /*[3:0][15:0]*/),
      // Outputs
      .o1(o1[63:0]),
      .o2(o2  /*[15:0].[16]*/)
  );
endmodule
)");
}

TEST(Autoexpand, AUTO_ExpandPortsMultipleConnectionsWithAUTO_TEMPLATE) {
  TestTextEdits(
      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule

module qux (
    input [3:0] i1,
    output [63:0] o1
);
  input i2[8][4];
  inout [3:0][15:0] io;
  output [15:0] o2[16];
endmodule

module foo (  /*AUTOARG*/);
  /*AUTOINPUT*/
  /*AUTOOUTPUT*/
  /*AUTOINOUT*/

  /* bar AUTO_TEMPLATE (
         .i1(in_a[]),
         .o2(out_b[])
     ); */
  bar b1 (  /*AUTOINST*/);
  bar b2 (  /*AUTOINST*/);
  bar b3 (  /*AUTOINST*/);
endmodule
)",
      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule

module qux (
    input [3:0] i1,
    output [63:0] o1
);
  input i2[8][4];
  inout [3:0][15:0] io;
  output [15:0] o2[16];
endmodule

module foo (  /*AUTOARG*/
    // Inputs
    in_a,
    i2,
    // Inouts
    io,
    // Outputs
    o1,
    out_b
);
  /*AUTOINPUT*/
  // Beginning of automatic inputs (from autoinst inputs)
  input in_a;  // To b1 of bar, ...
  input i2[4][8];  // To b1 of bar, ...
  // End of automatics
  /*AUTOOUTPUT*/
  // Beginning of automatic outputs (from autoinst outputs)
  output [15:0] o1;  // From b1 of bar, ...
  output [31:0] out_b[8];  // From b1 of bar, ...
  // End of automatics
  /*AUTOINOUT*/
  // Beginning of automatic inouts (from autoinst inouts)
  inout [7:0][7:0] io;  // To/From b1 of bar, ...
  // End of automatics

  /* bar AUTO_TEMPLATE (
         .i1(in_a[]),
         .o2(out_b[])
     ); */
  bar b1 (  /*AUTOINST*/
      // Inputs
      .i1(in_a),
      .i2(i2  /*.[4][8]*/),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o1(o1[15:0]),
      .o2(out_b  /*[31:0].[8]*/)
  );
  bar b2 (  /*AUTOINST*/
      // Inputs
      .i1(in_a),
      .i2(i2  /*.[4][8]*/),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o1(o1[15:0]),
      .o2(out_b  /*[31:0].[8]*/)
  );
  bar b3 (  /*AUTOINST*/
      // Inputs
      .i1(in_a),
      .i2(i2  /*.[4][8]*/),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o1(o1[15:0]),
      .o2(out_b  /*[31:0].[8]*/)
  );
endmodule
)");
}

TEST(Autoexpand, AUTO_ExpandRemovePorts) {
  TestTextEdits(
      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
endmodule

module foo (  /*AUTOARG*/
    // Inputs
    i1,
    i2,
    // Inouts
    io,
    // Outputs
    o1,
    o2
);
  /*AUTOINPUT*/
  // Beginning of automatic inputs (from autoinst inputs)
  input i1;  // To b of bar
  input i2[4][8];  // To b of bar
  // End of automatics
  /*AUTOOUTPUT*/
  // Beginning of automatic outputs (from autoinst outputs)
  output [15:0] o1;  // From b of bar
  output [31:0] o2[8];  // From b of bar
  // End of automatics
  /*AUTOINOUT*/
  // Beginning of automatic inouts (from autoinst inouts)
  inout [7:0][7:0] io;  // To/From b of bar
  // End of automatics

  bar b (  /*AUTOINST*/
      // Inputs
      .i1(i1),
      .i2(i2  /*.[4][8]*/),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o1(o1[15:0]),
      .o2(o2  /*[31:0].[8]*/)
  );
endmodule
)",
      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
endmodule

module foo (  /*AUTOARG*/
    // Inputs
    i1,
    i2,
    // Outputs
    o1
);
  /*AUTOINPUT*/
  // Beginning of automatic inputs (from autoinst inputs)
  input i1;  // To b of bar
  input i2[4][8];  // To b of bar
  // End of automatics
  /*AUTOOUTPUT*/
  // Beginning of automatic outputs (from autoinst outputs)
  output [15:0] o1;  // From b of bar
  // End of automatics
  /*AUTOINOUT*/

  bar b (  /*AUTOINST*/
      // Inputs
      .i1(i1),
      .i2(i2  /*.[4][8]*/),
      // Outputs
      .o1(o1[15:0])
  );
endmodule
)");
}

TEST(Autoexpand, AUTO_ExpandPorts_AUTOARG_Order) {
  TestTextEdits(

      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  output [31:0] o2[8];
endmodule

module foo (  /*AUTOARG*/);
  /*AUTOINPUT*/
  input i0;
  /*AUTOOUTPUT*/
  output o0;

  bar b (  /*AUTOINST*/);
endmodule
)",
      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  output [31:0] o2[8];
endmodule

module foo (  /*AUTOARG*/
    // Inputs
    i1,
    i2,
    i0,
    // Outputs
    o1,
    o2,
    o0
);
  /*AUTOINPUT*/
  // Beginning of automatic inputs (from autoinst inputs)
  input i1;  // To b of bar
  input i2[4][8];  // To b of bar
  // End of automatics
  input i0;
  /*AUTOOUTPUT*/
  // Beginning of automatic outputs (from autoinst outputs)
  output [15:0] o1;  // From b of bar
  output [31:0] o2[8];  // From b of bar
  // End of automatics
  output o0;

  bar b (  /*AUTOINST*/
      // Inputs
      .i1(i1),
      .i2(i2  /*.[4][8]*/),
      // Outputs
      .o1(o1[15:0]),
      .o2(o2  /*[31:0].[8]*/)
  );
endmodule
)",
      {TestRun{.edit_fn =
                   [](SymbolTableHandler *symbol_table_handler,
                      BufferTracker *tracker) {
                     return AutoExpandCodeActionToTextEdits(
                         symbol_table_handler, tracker,
                         {.start = {.line = 0}, .end = {.line = 16}},
                         "Expand all AUTOs in file");
                   }},
       TestRun{.edit_fn = [](SymbolTableHandler *symbol_table_handler,
                             BufferTracker *tracker) {
         return AutoExpandCodeActionToTextEdits(
             symbol_table_handler, tracker,
             {.start = {.line = 7}, .end = {.line = 8}},
             "Expand all AUTOs in selected range");
       }}});
}

TEST(Autoexpand, AUTO_ExpandPortsInHeader) {
  TestTextEdits(
      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule

module foo (
    /*AUTOINPUT*/
    /*AUTOOUTPUT*/
    /*AUTOINOUT*/
);

  bar b (  /*AUTOINST*/);
endmodule
)",
      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule

module foo (
    /*AUTOINPUT*/
    // Beginning of automatic inputs (from autoinst inputs)
    input i1,  // To b of bar
    input i2[4][8],  // To b of bar
    // End of automatics
    /*AUTOOUTPUT*/
    // Beginning of automatic outputs (from autoinst outputs)
    output [15:0] o1,  // From b of bar
    output [31:0] o2[8],  // From b of bar
    // End of automatics
    /*AUTOINOUT*/
    // Beginning of automatic inouts (from autoinst inouts)
    inout [7:0][7:0] io  // To/From b of bar
    // End of automatics
);

  bar b (  /*AUTOINST*/
      // Inputs
      .i1(i1),
      .i2(i2  /*.[4][8]*/),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o1(o1[15:0]),
      .o2(o2  /*[31:0].[8]*/)
  );
endmodule
)");
}

TEST(Autoexpand, AUTO_ExpandPorts_OutOfOrderModules) {
  TestTextEdits(
      R"(
module foo (  /*AUTOARG*/);
  /*AUTOINPUT*/
  /*AUTOOUTPUT*/
  /*AUTOINOUT*/

  bar b (  /*AUTOINST*/);
endmodule

module bar (
    input i1,
    output [15:0] o1
);
  /*AUTOINPUT*/
  /*AUTOOUTPUT*/

  inout [7:0][7:0] io;
  qux q (  /*AUTOINST*/);
endmodule

module qux (
    input i1,
    input i2[4][8],
    output [15:0] o1,
    output [31:0] o2[8]
);
endmodule
)",
      R"(
module foo (  /*AUTOARG*/
    // Inputs
    i1,
    i2,
    // Inouts
    io,
    // Outputs
    o1,
    o2
);
  /*AUTOINPUT*/
  // Beginning of automatic inputs (from autoinst inputs)
  input i1;  // To b of bar
  input i2[4][8];  // To b of bar
  // End of automatics
  /*AUTOOUTPUT*/
  // Beginning of automatic outputs (from autoinst outputs)
  output [15:0] o1;  // From b of bar
  output [31:0] o2[8];  // From b of bar
  // End of automatics
  /*AUTOINOUT*/
  // Beginning of automatic inouts (from autoinst inouts)
  inout [7:0][7:0] io;  // To/From b of bar
  // End of automatics

  bar b (  /*AUTOINST*/
      // Inputs
      .i1(i1),
      .i2(i2  /*.[4][8]*/),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o1(o1[15:0]),
      .o2(o2  /*[31:0].[8]*/)
  );
endmodule

module bar (
    input i1,
    output [15:0] o1
);
  /*AUTOINPUT*/
  // Beginning of automatic inputs (from autoinst inputs)
  input i2[4][8];  // To q of qux
  // End of automatics
  /*AUTOOUTPUT*/
  // Beginning of automatic outputs (from autoinst outputs)
  output [31:0] o2[8];  // From q of qux
  // End of automatics

  inout [7:0][7:0] io;
  qux q (  /*AUTOINST*/
      // Inputs
      .i1(i1),
      .i2(i2  /*.[4][8]*/),
      // Outputs
      .o1(o1[15:0]),
      .o2(o2  /*[31:0].[8]*/)
  );
endmodule

module qux (
    input i1,
    input i2[4][8],
    output [15:0] o1,
    output [31:0] o2[8]
);
endmodule
)");
}

TEST(Autoexpand, AUTO_ExpandPorts_DependencyLoop) {
  // This test is incorrect Verilog, but it checks that we don't loop forever
  // or do any other unexpected thing
  TestTextEdits(
      R"(
module foo (  /*AUTOARG*/);
  /*AUTOINPUT*/
  /*AUTOOUTPUT*/
  /*AUTOINOUT*/

  bar b (  /*AUTOINST*/);
endmodule

module bar (
    input i1,
    output [15:0] o1
);
  /*AUTOINPUT*/
  /*AUTOOUTPUT*/

  inout [7:0][7:0] io;
  qux q (  /*AUTOINST*/);
endmodule

module qux (
    input i1,
    input i2[4][8],
    output [15:0] o1,
    output [31:0] o2[8]
);

  foo f (  /*AUTOINST*/);
endmodule
)",
      R"(
module foo (  /*AUTOARG*/
    // Inputs
    i1,
    i2,
    // Inouts
    io,
    // Outputs
    o1,
    o2
);
  /*AUTOINPUT*/
  // Beginning of automatic inputs (from autoinst inputs)
  input i1;  // To b of bar
  input i2[4][8];  // To b of bar
  // End of automatics
  /*AUTOOUTPUT*/
  // Beginning of automatic outputs (from autoinst outputs)
  output [15:0] o1;  // From b of bar
  output [31:0] o2[8];  // From b of bar
  // End of automatics
  /*AUTOINOUT*/
  // Beginning of automatic inouts (from autoinst inouts)
  inout [7:0][7:0] io;  // To/From b of bar
  // End of automatics

  bar b (  /*AUTOINST*/
      // Inputs
      .i1(i1),
      .i2(i2  /*.[4][8]*/),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o1(o1[15:0]),
      .o2(o2  /*[31:0].[8]*/)
  );
endmodule

module bar (
    input i1,
    output [15:0] o1
);
  /*AUTOINPUT*/
  // Beginning of automatic inputs (from autoinst inputs)
  input i2[4][8];  // To q of qux
  // End of automatics
  /*AUTOOUTPUT*/
  // Beginning of automatic outputs (from autoinst outputs)
  output [31:0] o2[8];  // From q of qux
  // End of automatics

  inout [7:0][7:0] io;
  qux q (  /*AUTOINST*/
      // Inputs
      .i1(i1),
      .i2(i2  /*.[4][8]*/),
      // Outputs
      .o1(o1[15:0]),
      .o2(o2  /*[31:0].[8]*/)
  );
endmodule

module qux (
    input i1,
    input i2[4][8],
    output [15:0] o1,
    output [31:0] o2[8]
);

  foo f (  /*AUTOINST*/
      // Inputs
      .i1(i1),
      .i2(i2  /*.[4][8]*/),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o1(o1[15:0]),
      .o2(o2  /*[31:0].[8]*/)
  );
endmodule
)",
      {TestRun{.check_golden = false}});
}

TEST(Autoexpand, AUTOWIRE_ExpandEmpty) {
  TestTextEdits(
      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule

module foo;
  wire o1;

  /*AUTOWIRE*/

  bar b (  /*AUTOINST*/);
endmodule
)",
      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule

module foo;
  wire o1;

  /*AUTOWIRE*/
  // Beginning of automatic wires (for undeclared instantiated-module outputs)
  wire [7:0][7:0] io;  // To/From b of bar
  wire [31:0] o2[8];  // From b of bar
  // End of automatics

  bar b (  /*AUTOINST*/
      // Inputs
      .i1(i1),
      .i2(i2  /*.[4][8]*/),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o1(o1[15:0]),
      .o2(o2  /*[31:0].[8]*/)
  );
endmodule
)");
}

TEST(Autoexpand, AUTOWIRE_NoExpand) {
  TestTextEdits(
      R"(
module bar;
endmodule

module foo;
  /*AUTOWIRE*/

  bar b (  /*AUTOINST*/);
endmodule
)",
      R"(
module bar;
endmodule

module foo;
  /*AUTOWIRE*/

  bar b (  /*AUTOINST*/);
endmodule
)");
  TestTextEdits(
      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule

module foo (  /*AUTOWIRE*/
);
  wire o1  /*AUTOWIRE*/
  ;

  bar b (  /*AUTOINST*/);
endmodule
)",
      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule

module foo (  /*AUTOWIRE*/
);
  wire o1  /*AUTOWIRE*/
  ;

  bar b (  /*AUTOINST*/
      // Inputs
      .i1(i1),
      .i2(i2  /*.[4][8]*/),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o1(o1[15:0]),
      .o2(o2  /*[31:0].[8]*/)
  );
endmodule
)");
}

TEST(Autoexpand, AUTOWIRE_Replace) {
  TestTextEdits(
      R"(
module bar (
    input  i1,
    output o1
);
  input i2;
  inout [7:0][7:0] io;
  output o2;
endmodule

module foo;
  wire o1;

  /*AUTOWIRE*/
  // Beginning of automatic wires (for undeclared instantiated-module outputs)
  wire out1;  // From b of bar
  wire [7:0][7:0] in_out;  // To/From b of bar
  wire out2;  // From b of bar
  // End of automatics

  bar b (  /*AUTOINST*/);
endmodule
)",
      R"(
module bar (
    input  i1,
    output o1
);
  input i2;
  inout [7:0][7:0] io;
  output o2;
endmodule

module foo;
  wire o1;

  /*AUTOWIRE*/
  // Beginning of automatic wires (for undeclared instantiated-module outputs)
  wire [7:0][7:0] io;  // To/From b of bar
  wire o2;  // From b of bar
  // End of automatics

  bar b (  /*AUTOINST*/
      // Inputs
      .i1(i1),
      .i2(i2),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o1(o1),
      .o2(o2)
  );
endmodule
)");
}

TEST(Autoexpand, AUTOWIRE_InterInstance) {
  TestTextEdits(
      R"(
module bar (
    output y
);
  input  x;
endmodule

module qux (
    inout[1:0] y
);
endmodule

module quux (
    input[3:2] y
);
  output z;
endmodule

module foo;
  /*AUTOWIRE*/
  /*AUTOINPUT*/
  /*AUTOOUTPUT*/
  /*AUTOREG*/

  bar b (  /*AUTOINST*/);
  qux q (  /*AUTOINST*/);
  quux qu (  /*AUTOINST*/);
endmodule
)",
      R"(
module bar (
    output y
);
  input  x;
endmodule

module qux (
    inout[1:0] y
);
endmodule

module quux (
    input[3:2] y
);
  output z;
endmodule

module foo;
  /*AUTOWIRE*/
  // Beginning of automatic wires (for undeclared instantiated-module outputs)
  wire [3:0] y;  // To/From b of bar, ..
  // End of automatics
  /*AUTOINPUT*/
  // Beginning of automatic inputs (from autoinst inputs)
  input x;  // To b of bar
  // End of automatics
  /*AUTOOUTPUT*/
  // Beginning of automatic outputs (from autoinst outputs)
  output z;  // From qu of quux
  // End of automatics
  /*AUTOREG*/

  bar b (  /*AUTOINST*/
      // Inputs
      .x(x),
      // Outputs
      .y(y)
  );
  qux q (  /*AUTOINST*/
      // Inouts
      .y(y[1:0])
  );
  quux qu (  /*AUTOINST*/
      // Inputs
      .y(y[3:2]),
      // Outputs
      .z(z)
  );
endmodule
)");
  TestTextEdits(
      R"(
module bar;
  output [4] x;
  output y;
  input [7:0] z;
endmodule

module qux;
  localparam MAX = 1;
  inout [4] x;
  inout [MAX:0] y[1];
endmodule

module quux;
  input [3:0] x;
  localparam MAX = 3;
  localparam MIN = 2;
  localparam N = 4;
  input [MAX:MIN] y[N];
  output [8] z;
endmodule

module foo;
  /*AUTOWIRE*/

  bar b (  /*AUTOINST*/);
  qux q (  /*AUTOINST*/);
  quux qu (  /*AUTOINST*/);
endmodule
)",
      R"(
module bar;
  output [4] x;
  output y;
  input [7:0] z;
endmodule

module qux;
  localparam MAX = 1;
  inout [4] x;
  inout [MAX:0] y[1];
endmodule

module quux;
  input [3:0] x;
  localparam MAX = 3;
  localparam MIN = 2;
  localparam N = 4;
  input [MAX:MIN] y[N];
  output [8] z;
endmodule

module foo;
  /*AUTOWIRE*/
  // Beginning of automatic wires (for undeclared instantiated-module outputs)
  wire [3:0] x;  // To/From b of bar, ..
  wire [MAX:0] y[1];  // To/From b of bar, ..
  wire [7:0] z;  // To/From b of bar, ..
  // End of automatics

  bar b (  /*AUTOINST*/
      // Inputs
      .z(z[7:0]),
      // Outputs
      .x(x[4]),
      .y(y)
  );
  qux q (  /*AUTOINST*/
      // Inouts
      .x(x[4]),
      .y(y  /*[MAX:0].[1]*/)
  );
  quux qu (  /*AUTOINST*/
      // Inputs
      .x(x[3:0]),
      .y(y  /*[MAX:MIN].[N]*/),
      // Outputs
      .z(z[8])
  );
endmodule
)");
  TestTextEdits(
      R"(
module bar;
  input [-4:3] x;
endmodule

module qux;
  output [15:0] x;
endmodule

module foo;
  /*AUTOWIRE*/

  bar b (  /*AUTOINST*/);
  qux q (  /*AUTOINST*/);
endmodule
)",
      R"(
module bar;
  input [-4:3] x;
endmodule

module qux;
  output [15:0] x;
endmodule

module foo;
  /*AUTOWIRE*/
  // Beginning of automatic wires (for undeclared instantiated-module outputs)
  wire [-4:15] x;  // To/From b of bar, ..
  // End of automatics

  bar b (  /*AUTOINST*/
      // Inputs
      .x(x[-4:3])
  );
  qux q (  /*AUTOINST*/
      // Outputs
      .x(x[15:0])
  );
endmodule
)");
}

TEST(Autoexpand, AUTOWIRE_InterInstanceWithAUTO_TEMPLATE) {
  TestTextEdits(
      R"(
module bar (
    output a
);
  input  x;
endmodule

module qux (
    inout  b
);
endmodule

module quux (
    input  y
);
  output z;
endmodule

module foo;
  /*AUTOWIRE*/
  /*AUTOINPUT*/
  /*AUTOOUTPUT*/
  /*AUTOREG*/

  /* bar  AUTO_TEMPLATE
     qux  AUTO_TEMPLATE
     quux AUTO_TEMPLATE (
         .a(y),
         .b(y),
     ); */
  bar b (  /*AUTOINST*/);
  qux q (  /*AUTOINST*/);
  quux qu (  /*AUTOINST*/);
endmodule
)",
      R"(
module bar (
    output a
);
  input  x;
endmodule

module qux (
    inout  b
);
endmodule

module quux (
    input  y
);
  output z;
endmodule

module foo;
  /*AUTOWIRE*/
  // Beginning of automatic wires (for undeclared instantiated-module outputs)
  wire y;  // To/From b of bar, ..
  // End of automatics
  /*AUTOINPUT*/
  // Beginning of automatic inputs (from autoinst inputs)
  input x;  // To b of bar
  // End of automatics
  /*AUTOOUTPUT*/
  // Beginning of automatic outputs (from autoinst outputs)
  output z;  // From qu of quux
  // End of automatics
  /*AUTOREG*/

  /* bar  AUTO_TEMPLATE
     qux  AUTO_TEMPLATE
     quux AUTO_TEMPLATE (
         .a(y),
         .b(y),
     ); */
  bar b (  /*AUTOINST*/
      // Inputs
      .x(x),
      // Outputs
      .a(y)
  );
  qux q (  /*AUTOINST*/
      // Inouts
      .b(y)
  );
  quux qu (  /*AUTOINST*/
      // Inputs
      .y(y),
      // Outputs
      .z(z)
  );
endmodule
)");
}

TEST(Autoexpand, AUTOREG_ExpandEmpty) {
  TestTextEdits(
      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule

module foo;
  output [15:0] o1;
  output [31:0] o2[8];
  output [3:0][3:0] o3[16];
  output o4;

  reg o4;

  /*AUTOREG*/

  bar b (  /*AUTOINST*/);
endmodule
)",
      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule

module foo;
  output [15:0] o1;
  output [31:0] o2[8];
  output [3:0][3:0] o3[16];
  output o4;

  reg o4;

  /*AUTOREG*/
  // Beginning of automatic regs (for this module's undeclared outputs)
  reg [3:0][3:0] o3[16];
  // End of automatics

  bar b (  /*AUTOINST*/
      // Inputs
      .i1(i1),
      .i2(i2  /*.[4][8]*/),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o1(o1[15:0]),
      .o2(o2  /*[31:0].[8]*/)
  );
endmodule
)");
}

TEST(Autoexpand, AUTOREG_NoExpand) {
  TestTextEdits(
      R"(
module foo;
  output o;
  reg o;
  /*AUTOREG*/
endmodule
)",
      R"(
module foo;
  output o;
  reg o;
  /*AUTOREG*/
endmodule
)");
  TestTextEdits(
      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule

module foo (  /*AUTOREG*/
);
  output [15:0] o1;
  output [31:0] o2[8];
  output [3:0][3:0] o3[16]  /*AUTOREG*/
  ;

  bar b (  /*AUTOINST*/);
endmodule
)",
      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule

module foo (  /*AUTOREG*/
);
  output [15:0] o1;
  output [31:0] o2[8];
  output [3:0][3:0] o3[16]  /*AUTOREG*/
  ;

  bar b (  /*AUTOINST*/
      // Inputs
      .i1(i1),
      .i2(i2  /*.[4][8]*/),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o1(o1[15:0]),
      .o2(o2  /*[31:0].[8]*/)
  );
endmodule
)");
}

TEST(Autoexpand, AUTOREG_Replace) {
  TestTextEdits(
      R"(
module bar (
    input  i1,
    output o1
);
  input i2;
  inout [7:0][7:0] io;
  output o2;
endmodule

module foo;
  output o1;
  output o2;
  output o3;

  /*AUTOREG*/
  // Beginning of automatic regs (for this module's undeclared outputs)
  reg out_3;
  // End of automatics

  bar b (  /*AUTOINST*/);
endmodule
)",
      R"(
module bar (
    input  i1,
    output o1
);
  input i2;
  inout [7:0][7:0] io;
  output o2;
endmodule

module foo;
  output o1;
  output o2;
  output o3;

  /*AUTOREG*/
  // Beginning of automatic regs (for this module's undeclared outputs)
  reg o3;
  // End of automatics

  bar b (  /*AUTOINST*/
      // Inputs
      .i1(i1),
      .i2(i2),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o1(o1),
      .o2(o2)
  );
endmodule
)");
}

TEST(Autoexpand, AUTO_ExpandVars) {
  TestTextEdits(
      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  inout [7:0][7:0] io;
  output [31:0] o2[8];
  /*AUTOREG*/
endmodule

module foo;
  /*AUTOWIRE*/

  bar b (  /*AUTOINST*/);
endmodule
)",
      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  inout [7:0][7:0] io;
  output [31:0] o2[8];
  /*AUTOREG*/
  // Beginning of automatic regs (for this module's undeclared outputs)
  reg [15:0] o1;
  reg [31:0] o2 [8];
  // End of automatics
endmodule

module foo;
  /*AUTOWIRE*/
  // Beginning of automatic wires (for undeclared instantiated-module outputs)
  wire [15:0] o1;  // From b of bar
  wire [7:0][7:0] io;  // To/From b of bar
  wire [31:0] o2[8];  // From b of bar
  // End of automatics

  bar b (  /*AUTOINST*/
      // Inputs
      .i1(i1),
      .i2(i2  /*.[4][8]*/),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o1(o1[15:0]),
      .o2(o2  /*[31:0].[8]*/)
  );
endmodule
)");
  TestTextEdits(
      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule

module foo;
  output oo;

  /*AUTOREG*/

  /*AUTOWIRE*/

  bar b (  /*AUTOINST*/);
endmodule
)",
      R"(
module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  inout [7:0][7:0] io;
  output [31:0] o2[8];
endmodule

module foo;
  output oo;

  /*AUTOREG*/
  // Beginning of automatic regs (for this module's undeclared outputs)
  reg oo;
  // End of automatics

  /*AUTOWIRE*/
  // Beginning of automatic wires (for undeclared instantiated-module outputs)
  wire [15:0] o1;  // From b of bar
  wire [7:0][7:0] io;  // To/From b of bar
  wire [31:0] o2[8];  // From b of bar
  // End of automatics

  bar b (  /*AUTOINST*/
      // Inputs
      .i1(i1),
      .i2(i2  /*.[4][8]*/),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o1(o1[15:0]),
      .o2(o2  /*[31:0].[8]*/)
  );
endmodule
)");
}

TEST(Autoexpand, AUTO_ExpandPortsWithAUTOVars) {
  TestTextEdits(
      R"(
module qux (
    input [1:0][7:0] ii,
    output [3:0] oo[5][3]
);
endmodule

module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  inout [7:0][7:0] io;
  output [31:0] o2[8];

  /*AUTOWIRE*/

  /*AUTOREG*/

  qux q (  /*AUTOINST*/);
endmodule

module foo (  /*AUTOARG*/);
  /*AUTOINPUT*/
  /*AUTOOUTPUT*/
  /*AUTOINOUT*/

  bar b (  /*AUTOINST*/);
endmodule
)",
      R"(
module qux (
    input [1:0][7:0] ii,
    output [3:0] oo[5][3]
);
endmodule

module bar (
    input i1,
    output [15:0] o1
);
  input i2[4][8];
  inout [7:0][7:0] io;
  output [31:0] o2[8];

  /*AUTOWIRE*/
  // Beginning of automatic wires (for undeclared instantiated-module outputs)
  wire [3:0] oo[5][3];  // From q of qux
  // End of automatics

  /*AUTOREG*/
  // Beginning of automatic regs (for this module's undeclared outputs)
  reg [15:0] o1;
  reg [31:0] o2[8];
  // End of automatics

  qux q (  /*AUTOINST*/
      // Inputs
      .ii(ii  /*[1:0][7:0]*/),
      // Outputs
      .oo(oo  /*[3:0].[5][3]*/)
  );
endmodule

module foo (  /*AUTOARG*/
    // Inputs
    i1,
    i2,
    // Inouts
    io,
    // Outputs
    o1,
    o2
);
  /*AUTOINPUT*/
  // Beginning of automatic inputs (from autoinst inputs)
  input i1;  // To b of bar
  input i2[4][8];  // To b of bar
  // End of automatics
  /*AUTOOUTPUT*/
  // Beginning of automatic outputs (from autoinst outputs)
  output [15:0] o1;  // From b of bar
  output [31:0] o2[8];  // From b of bar
  // End of automatics
  /*AUTOINOUT*/
  // Beginning of automatic inouts (from autoinst inouts)
  inout [7:0][7:0] io;  // To/From b of bar
  // End of automatics

  bar b (  /*AUTOINST*/
      // Inputs
      .i1(i1),
      .i2(i2  /*.[4][8]*/),
      // Inouts
      .io(io  /*[7:0][7:0]*/),
      // Outputs
      .o1(o1[15:0]),
      .o2(o2  /*[31:0].[8]*/)
  );
endmodule
)",
      TestRun{.check_formatting = false}.repeat()
      // Module net variable alignment is not handled
      // correctly due to the formatter looking at the
      // generated code only instead of the entire file
  );
}

TEST(Autoexpand, ExpandRange) {
  TestTextEdits(
      R"(
module foo (  /*AUTOARG*/);
  /*AUTOINPUT*/
  /*AUTOOUTPUT*/

  /* qux AUTO_TEMPLATE
     bar AUTO_TEMPLATE (
         .o1(out_a[]),
         .o2(out_b[])
     ); */
  bar b (  /*AUTOINST*/);
endmodule

module bar (  /*AUTOARG*/);
  input clk;
  input rst;
  output [63:0] o1;
  output o2[16];

  /*AUTOREG*/
endmodule
)",
      R"(
module foo (  /*AUTOARG*/
    // Inputs
    clk,
    rst,
    // Outputs
    out_a,
    out_b
);
  /*AUTOINPUT*/
  // Beginning of automatic inputs (from autoinst inputs)
  input clk;  // To b of bar
  input rst;  // To b of bar
  // End of automatics
  /*AUTOOUTPUT*/
  // Beginning of automatic outputs (from autoinst outputs)
  output [63:0] out_a;  // From b of bar
  output out_b[16];  // From b of bar
  // End of automatics

  /* qux AUTO_TEMPLATE
     bar AUTO_TEMPLATE (
         .o1(out_a[]),
         .o2(out_b[])
     ); */
  bar b (  /*AUTOINST*/
      // Inputs
      .clk(clk),
      .rst(rst),
      // Outputs
      .o1 (out_a[63:0]),
      .o2 (out_b  /*.[16]*/)
  );
endmodule

module bar (  /*AUTOARG*/);
  input clk;
  input rst;
  output [63:0] o1;
  output o2[16];

  /*AUTOREG*/
endmodule
)",
      {TestRun{.edit_fn =
                   [](SymbolTableHandler *symbol_table_handler,
                      BufferTracker *tracker) {
                     return AutoExpandCodeActionToTextEdits(
                         symbol_table_handler, tracker,
                         {.start = {.line = 0}, .end = {.line = 10}},
                         "Expand all AUTOs in selected range");
                   }}}  // Do not repeat: the range is incorrect
                        // after the first expansion
  );
}

TEST(Autoexpand, OverlappingExpansions) {
  TestTextEdits(
      R"(
module foo (
    /*AUTOARG*/
    /*AUTOINPUT*/
    /*AUTOOUTPUT*/
);
  bar b (  /*AUTOINST*/);
endmodule

module bar (
  input clk,
  input rst,
  output [63:0] o1,
  output o2[16]
);
endmodule
)",
      R"(
module foo (
    /*AUTOARG*/
    /*AUTOINPUT*/
    /*AUTOOUTPUT*/
);
  bar b (  /*AUTOINST*/
      // Inputs
      .clk(clk),
      .rst(rst),
      // Outputs
      .o1(o1[63:0]),
      .o2(o2  /*.[16]*/)
  );
endmodule

module bar (
  input clk,
  input rst,
  output [63:0] o1,
  output o2[16]
);
endmodule
)",
      {TestRun{}});
}

TEST(Autoexpand, ExpandKind) {
  TestTextEdits(
      R"(
module foo (  /*AUTOARG*/);
  /*AUTOINPUT*/
  /*AUTOOUTPUT*/

  /* qux AUTO_TEMPLATE
     bar AUTO_TEMPLATE (
         .o1(out_a[]),
         .o2(out_b[])
     ); */
  bar b (  /*AUTOINST*/);
endmodule

module bar (  /*AUTOARG*/);
  input clk;
  input rst;
  output [63:0] o1;
  output o2[16];

  /*AUTOREG*/
endmodule
)",
      R"(
module foo (  /*AUTOARG*/);
  /*AUTOINPUT*/
  /*AUTOOUTPUT*/

  /* qux AUTO_TEMPLATE
     bar AUTO_TEMPLATE (
         .o1(out_a[]),
         .o2(out_b[])
     ); */
  bar b (  /*AUTOINST*/);
endmodule

module bar (  /*AUTOARG*/
    // Inputs
    clk,
    rst,
    // Outputs
    o1,
    o2
);
  input clk;
  input rst;
  output [63:0] o1;
  output o2[16];

  /*AUTOREG*/
endmodule
)",
      TestRun{.edit_fn = [](SymbolTableHandler *symbol_table_handler,
                            BufferTracker *tracker) {
        return AutoExpandCodeActionToTextEdits(
            symbol_table_handler, tracker,
            {.start = {.line = 1}, .end = {.line = 1}},
            "Expand all AUTOs of same kind as this one");
      }}.repeat());
  TestTextEdits(
      R"(
module foo (  /*AUTOARG*/);
  bar b (  /*AUTOINST*/);
  /*AUTOINPUT*//*AUTOOUTPUT*/
endmodule

module bar;
  /*AUTOINPUT*/
  /*AUTOOUTPUT*/
  qux q (  /*AUTOINST*/);
endmodule

module qux (
    input clk,
    input rst,
    output [63:0] o1,
    output o2[16]
);
endmodule
)",
      R"(
module foo (  /*AUTOARG*/
    // Inputs
    clk,
    rst,
    // Outputs
    o1,
    o2
);
  bar b (  /*AUTOINST*/
      // Inputs
      .clk(clk),
      .rst(rst),
      // Outputs
      .o1(o1[63:0]),
      .o2(o2  /*.[16]*/)
  );
  /*AUTOINPUT*/
  // Beginning of automatic inputs (from autoinst inputs)
  input clk;  // To b of bar
  input rst;  // To b of bar
  // End of automatics  /*AUTOOUTPUT*/
  // Beginning of automatic outputs (from autoinst outputs)
  output [63:0] o1;  // From b of bar
  output o2[16];  // From b of bar
  // End of automatics
endmodule

module bar;
  /*AUTOINPUT*/
  // Beginning of automatic inputs (from autoinst inputs)
  input clk;  // To q of qux
  input rst;  // To q of qux
  // End of automatics
  /*AUTOOUTPUT*/
  // Beginning of automatic outputs (from autoinst outputs)
  output [63:0] o1;  // From q of qux
  output o2[16];  // From q of qux
  // End of automatics
  qux q (  /*AUTOINST*/
      // Inputs
      .clk(clk),
      .rst(rst),
      // Outputs
      .o1 (o1[63:0]),
      .o2 (o2  /*.[16]*/)
  );
endmodule

module qux (
    input clk,
    input rst,
    output [63:0] o1,
    output o2[16]
);
endmodule
)",
      {TestRun{.edit_fn =
                   [](SymbolTableHandler *symbol_table_handler,
                      BufferTracker *tracker) {
                     return AutoExpandCodeActionToTextEdits(
                         symbol_table_handler, tracker,
                         {.start = {.line = 1}, .end = {.line = 3}},
                         "Expand all AUTOs of same kinds as selected");
                   }}}  // Do not repeat: the range is incorrect
                        // after the first expansion
  );
}

TEST(Autoexpand, InstanceNotModule) {
  TestTextEdits(
      R"(
module foo;
  wire bar;
  bar b (  /*AUTOINST*/);
endmodule
)",
      R"(
module foo;
  wire bar;
  bar b (  /*AUTOINST*/);
endmodule
)");
  // The case below should be expanded, but it's not handled yet. That's
  // because the symbol table returns the instance node when queried for the
  // module type. TODO: Either get a list of definitions from the symbol table
  // and use the one that is a module, or query the symbol table in a smarter
  // way
  TestTextEdits(
      R"(
module foo (
    /*AUTOINPUT*/
    /*AUTOOUTPUT*/
);
  bar bar (  /*AUTOINST*/);
endmodule

module bar (
    input clk,
    input rst,
    output [63:0] o1,
    output o2[16]
);
endmodule
)",
      R"(
module foo (
    /*AUTOINPUT*/
    /*AUTOOUTPUT*/
);
  bar bar (  /*AUTOINST*/);
endmodule

module bar (
    input clk,
    input rst,
    output [63:0] o1,
    output o2[16]
);
endmodule
)");
}

TEST(Autoexpand, PreserveUserCode) {
  // Check that AUTO expand's formatter invocation does not affect lines not
  // generated by it
  TestTextEdits(
      R"(
module foo (  /*AUTOARG*/);

always @(posedge
clk) $print(
  "i'm at clk posedge!"
);

  /*AUTOINPUT*/
  /*AUTOOUTPUT*/
  reg     [7:0]                   mem[0:255];
  /* qux AUTO_TEMPLATE
     bar AUTO_TEMPLATE (
         .o1(out_a[]),
         .o2(out_b[])
     ); */
  bar b (  /*AUTOINST*/);
endmodule

module bar (  /*AUTOARG*/);
    input clk;
  input   rst ;
  output [ 63 : 0 ] o1;
  output   o2  [ 16 ];

  /*AUTOREG*/
endmodule
)",
      R"(
module foo (  /*AUTOARG*/
    // Inputs
    clk,
    rst,
    // Outputs
    out_a,
    out_b
);

always @(posedge
clk) $print(
  "i'm at clk posedge!"
);

  /*AUTOINPUT*/
  // Beginning of automatic inputs (from autoinst inputs)
  input clk;  // To b of bar
  input rst;  // To b of bar
  // End of automatics
  /*AUTOOUTPUT*/
  // Beginning of automatic outputs (from autoinst outputs)
  output [63:0] out_a;  // From b of bar
  output out_b[16];  // From b of bar
  // End of automatics
  reg     [7:0]                   mem[0:255];
  /* qux AUTO_TEMPLATE
     bar AUTO_TEMPLATE (
         .o1(out_a[]),
         .o2(out_b[])
     ); */
  bar b (  /*AUTOINST*/
      // Inputs
      .clk(clk),
      .rst(rst),
      // Outputs
      .o1 (out_a[63:0]),
      .o2 (out_b  /*.[16]*/)
  );
endmodule

module bar (  /*AUTOARG*/
    // Inputs
    clk,
    rst,
    // Outputs
    o1,
    o2
);
    input clk;
  input   rst ;
  output [ 63 : 0 ] o1;
  output   o2  [ 16 ];

  /*AUTOREG*/
  // Beginning of automatic regs (for this module's undeclared outputs)
  reg [63:0] o1;
  reg o2[16];
  // End of automatics
endmodule
)",
      TestRun{.check_formatting = false}.repeat()
      // Do not check formatting, as the test intentionally does not
      // follow formatting rules
  );
}

}  // namespace
}  // namespace verilog
