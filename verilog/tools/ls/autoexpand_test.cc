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

#include "verilog/tools/ls/autoexpand.h"

#include "common/lsp/lsp-protocol.h"
#include "gtest/gtest.h"
#include "verilog/tools/ls/verible-lsp-adapter.h"

namespace verilog {
namespace {
using verible::lsp::CodeAction;
using verible::lsp::CodeActionParams;
using verible::lsp::EditTextBuffer;
using verible::lsp::Range;
using verible::lsp::TextDocumentContentChangeEvent;
using verible::lsp::TextEdit;

// Generate text edits using the given function and test if they had the desired
// effect
void TestTextEdits(const std::function<std::vector<TextEdit>(
                       SymbolTableHandler*, BufferTracker*)>& edit_fun,
                   const std::vector<absl::string_view>& project_file_contents,
                   const absl::string_view text_before,
                   const absl::string_view text_golden,
                   const bool repeat = true) {
  static const char* TESTED_FILENAME = "<<tested-file>>";
  // Init a text buffer which we need for the autoepxand functions
  EditTextBuffer buffer(text_before);
  BufferTracker tracker;
  tracker.Update(TESTED_FILENAME, buffer);
  // Create a Verilog project with the given project file contents
  const std::shared_ptr<VerilogProject> proj =
      std::make_shared<VerilogProject>(".", std::vector<std::string>());
  size_t i = 0;
  for (const absl::string_view file_contents : project_file_contents) {
    auto filename = absl::StrCat("<<project-file-", i, ">>");
    proj->AddVirtualFile(filename, file_contents);
    i++;
  }
  // Init a symbol table handler which is also needed for certain AUTO
  // expansions. This handler also needs a Verilog project to work properly.
  SymbolTableHandler symbol_table_handler;
  symbol_table_handler.SetProject(proj);
  symbol_table_handler.UpdateFileContent(TESTED_FILENAME,
                                         &tracker.current()->parser().Data());
  symbol_table_handler.BuildProjectSymbolTable();
  // Run the tested edit function
  std::vector<TextEdit> edits = edit_fun(&symbol_table_handler, &tracker);
  // Sort the TextEdits from the last one in the buffer to the first one. This
  // way we can apply them one by one and have the following ones still be
  // valid.
  // Note: according to the spec, TextEdits should never overlap.
  std::sort(edits.begin(), edits.end(),
            [](const TextEdit& first, const TextEdit& second) {
              if (first.range.start.line == second.range.end.line) {
                return first.range.start.character > second.range.end.character;
              }
              return first.range.start.line > second.range.end.line;
            });
  // Apply the text edits
  for (const TextEdit& edit : edits) {
    buffer.ApplyChange(TextDocumentContentChangeEvent{
        .range = edit.range, .has_range = true, .text = edit.newText});
  }
  // Check the result and test again to check idempotence
  buffer.RequestContent([&](const absl::string_view text_after) {
    EXPECT_EQ(text_after, text_golden);
    if (repeat) {
      TestTextEdits(edit_fun, project_file_contents, text_golden, text_golden,
                    false);
    }
  });
}

// Same as above, without the project file parameter
void TestTextEdits(const std::function<std::vector<TextEdit>(
                       SymbolTableHandler*, BufferTracker*)>& edit_fun,
                   const absl::string_view text_before,
                   const absl::string_view text_golden,
                   const bool repeat = true) {
  TestTextEdits(edit_fun, {}, text_before, text_golden, repeat);
}

// Generate a specific code action and extract text edits from it
std::vector<TextEdit> AutoExpandCodeActionToTextEdits(
    SymbolTableHandler* symbol_table_handler, BufferTracker* tracker,
    Range range, absl::string_view title) {
  CodeActionParams p = {.textDocument = {tracker->current()->uri()},
                        .range = range};
  nlohmann::json changes;
  for (const CodeAction& action :
       GenerateAutoExpandCodeActions(symbol_table_handler, tracker, p)) {
    if (action.title == title) {
      EXPECT_TRUE(changes.empty());
      changes = action.edit.changes;
    }
  }
  EXPECT_FALSE(changes.empty());
  return changes[p.textDocument.uri];
}

TEST(Autoexpand, AUTOARG_ExpandEmpty) {
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
  clk, rst,
  // Outputs
  o
  );
  input logic clk;
  input logic rst;
  output logic o;
endmodule
module t2(/*AUTOARG*/
  // Inputs
  clk, rst,
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

TEST(Autoexpand, AUTOARG_Replace) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module t(/*AUTOARG*/
  //Inputs
  clk,rst
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
  clk, rst,
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

TEST(Autoexpand, AUTOARG_SkipPredeclared) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module t(input i1, i2,
         o1, /*AUTOARG*/
//Inputs
clk, rst
);
  input logic clk;
  input logic rst;
  input logic i2;
  output logic o1;
  output logic o2;
endmodule)",
                R"(
module t(input i1, i2,
         o1, /*AUTOARG*/
  // Inputs
  clk, rst,
  // Outputs
  o2
  );
  input logic clk;
  input logic rst;
  input logic i2;
  output logic o1;
  output logic o2;
endmodule)");
}

TEST(Autoexpand, AUTOINST_ExpandEmpty) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module bar(input i1, output o1);
  input i2;
  inout io;
  output o2;
endmodule

module foo;
  inout logic io;

  bar b(/*AUTOINST*/);
endmodule
)",
                R"(
module bar(input i1, output o1);
  input i2;
  inout io;
  output o2;
endmodule

module foo;
  inout logic io;

  bar b(/*AUTOINST*/
    // Inputs
    .i1(i1),
    .i2(i2),
    // Inouts
    .io(io),
    // Outputs
    .o1(o1),
    .o2(o2));
endmodule
)");
}

TEST(Autoexpand, AUTOINST_NoExpand) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module bar(input i1, output o1);
  input i2;
  output o2;
endmodule

module foo;
  inout logic io;

  bar b();
  /*AUTOINST*/
endmodule
)",
                R"(
module bar(input i1, output o1);
  input i2;
  output o2;
endmodule

module foo;
  inout logic io;

  bar b();
  /*AUTOINST*/
endmodule
)");

  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module foo;
  bar b(/*AUTOINST*/);
endmodule
)",
                R"(
module foo;
  bar b(/*AUTOINST*/);
endmodule
)");
}

TEST(Autoexpand, AUTOINST_Replace) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module bar(input i1, output o1);
  input i2;
  output o2;
endmodule

module foo;
  inout logic io;

  bar b(/*AUTOINST*/ .i1(i1),
    // Outputs
    .o1(o1), .o2(o2));
endmodule
)",
                R"(
module bar(input i1, output o1);
  input i2;
  output o2;
endmodule

module foo;
  inout logic io;

  bar b(/*AUTOINST*/
    // Inputs
    .i1(i1),
    .i2(i2),
    // Outputs
    .o1(o1),
    .o2(o2));
endmodule
)");
}

TEST(Autoexpand, AUTOINST_SkipPreConnected) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module foo;
  inout logic io;

  bar b(.i1(io), /*AUTOINST*/);
endmodule

module bar(input i1, output o1);
  input i2;
  output o2;
endmodule
)",
                R"(
module foo;
  inout logic io;

  bar b(.i1(io), /*AUTOINST*/
    // Inputs
    .i2(i2),
    // Outputs
    .o1(o1),
    .o2(o2));
endmodule

module bar(input i1, output o1);
  input i2;
  output o2;
endmodule
)");
}

TEST(Autoexpand, AUTOINST_Missing) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module foo;
  bar b(/*AUTOINST*/);
endmodule
)",
                R"(
module foo;
  bar b(/*AUTOINST*/);
endmodule
)");
}

TEST(Autoexpand, AUTOINST_Ambiguous) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module bar(input i1, output o1);
endmodule

module bar(input i2, output o2);
endmodule

module foo;
  bar b(/*AUTOINST*/);
endmodule
)",
                R"(
module bar(input i1, output o1);
endmodule

module bar(input i2, output o2);
endmodule

module foo;
  bar b(/*AUTOINST*/
    // Inputs
    .i1(i1),
    // Outputs
    .o1(o1));
endmodule
)");
}

TEST(Autoexpand, AUTOINST_Chain) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module bar(input i1, output o1);
  input i2;
  inout io;
  output o2;

  qux q(/*AUTOINST*/);
endmodule

module foo;
  inout logic io;

  bar b(/*AUTOINST*/);
endmodule

module qux;
  input i1;
  inout io;
  output o2;
endmodule
)",
                R"(
module bar(input i1, output o1);
  input i2;
  inout io;
  output o2;

  qux q(/*AUTOINST*/
    // Inputs
    .i1(i1),
    // Inouts
    .io(io),
    // Outputs
    .o2(o2));
endmodule

module foo;
  inout logic io;

  bar b(/*AUTOINST*/
    // Inputs
    .i1(i1),
    .i2(i2),
    // Inouts
    .io(io),
    // Outputs
    .o1(o1),
    .o2(o2));
endmodule

module qux;
  input i1;
  inout io;
  output o2;
endmodule
)");
}

TEST(Autoexpand, AUTOINST_MultipleFiles) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                {R"(
module bar(input i1, output o1);
  input i2;
  inout io;
  output o2;
endmodule
    )",
                 R"(
module qux;
  input i1;
  inout io;
  output o2;
endmodule
   )"},
                R"(
module foo;
  bar b(/*AUTOINST*/);
  qux q(/*AUTOINST*/);
endmodule
)",
                R"(
module foo;
  bar b(/*AUTOINST*/
    // Inputs
    .i1(i1),
    .i2(i2),
    // Inouts
    .io(io),
    // Outputs
    .o1(o1),
    .o2(o2));
  qux q(/*AUTOINST*/
    // Inputs
    .i1(i1),
    // Inouts
    .io(io),
    // Outputs
    .o2(o2));
endmodule
)");
}

TEST(Autoexpand, AUTO_TEMPLATE_Simple) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module foo;
  /* bar AUTO_TEMPLATE "some_regex_ignored_for_now" (
       .i1(in_a),
       .o2(out_b)
     ); */
  bar b(/*AUTOINST*/);
endmodule

module bar;
  input i1;
  input i2;
  inout io;
  output o1;
  output o2;
endmodule
)",
                R"(
module foo;
  /* bar AUTO_TEMPLATE "some_regex_ignored_for_now" (
       .i1(in_a),
       .o2(out_b)
     ); */
  bar b(/*AUTOINST*/
    // Inputs
    .i1(in_a),
    .i2(i2),
    // Inouts
    .io(io),
    // Outputs
    .o1(o1),
    .o2(out_b));
endmodule

module bar;
  input i1;
  input i2;
  inout io;
  output o1;
  output o2;
endmodule
)");
}

TEST(Autoexpand, AUTO_TEMPLATE_SkipPreConnected) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module foo;
  /* bar AUTO_TEMPLATE "some_regex_ignored_for_now" (
       .i1(in_a),
       .o2(out_b)
     ); */
  bar b(.i1(input_1),
    /*AUTOINST*/);
endmodule

module bar;
  input i1;
  input i2;
  inout io;
  output o1;
  output o2;
endmodule
)",
                R"(
module foo;
  /* bar AUTO_TEMPLATE "some_regex_ignored_for_now" (
       .i1(in_a),
       .o2(out_b)
     ); */
  bar b(.i1(input_1),
    /*AUTOINST*/
    // Inputs
    .i2(i2),
    // Inouts
    .io(io),
    // Outputs
    .o1(o1),
    .o2(out_b));
endmodule

module bar;
  input i1;
  input i2;
  inout io;
  output o1;
  output o2;
endmodule
)");
}

TEST(Autoexpand, AUTO_TEMPLATE_MultipleMatches) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module foo;
  /* qux AUTO_TEMPLATE
     quux AUTO_TEMPLATE
     bar AUTO_TEMPLATE "some_regex_ignored_for_now" (
       .i1(in_a),
       .o2(out_b)); */
  qux q(/*AUTOINST*/);
  bar b(/*AUTOINST*/);
endmodule

module bar;
  input i1;
  input i2;
  inout io;
  output o1;
  output o2;
endmodule

module qux;
  input i1;
  inout io;
  output o2;
endmodule
)",
                R"(
module foo;
  /* qux AUTO_TEMPLATE
     quux AUTO_TEMPLATE
     bar AUTO_TEMPLATE "some_regex_ignored_for_now" (
       .i1(in_a),
       .o2(out_b)); */
  qux q(/*AUTOINST*/
    // Inputs
    .i1(in_a),
    // Inouts
    .io(io),
    // Outputs
    .o2(out_b));
  bar b(/*AUTOINST*/
    // Inputs
    .i1(in_a),
    .i2(i2),
    // Inouts
    .io(io),
    // Outputs
    .o1(o1),
    .o2(out_b));
endmodule

module bar;
  input i1;
  input i2;
  inout io;
  output o1;
  output o2;
endmodule

module qux;
  input i1;
  inout io;
  output o2;
endmodule
)");
}

TEST(Autoexpand, AUTO_TEMPLATE_Override) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module foo;
  /* qux AUTO_TEMPLATE
     bar AUTO_TEMPLATE "some_regex_ignored_for_now" (
       .i1(in_a),
       .o2(out_b)); */
  qux q(/*AUTOINST*/);

  /* bar AUTO_TEMPLATE "some_regex_ignored_for_now" (
       .i1(input_1),
       .o2(output_2),
       .i2(input_2),
       .io(input_output),
       .o1(output_1)); */
  bar b(/*AUTOINST*/);
endmodule

module bar;
  input i1;
  input i2;
  inout io;
  output o1;
  output o2;
endmodule

module qux;
  input i1;
  inout io;
  output o2;
endmodule
)",
                R"(
module foo;
  /* qux AUTO_TEMPLATE
     bar AUTO_TEMPLATE "some_regex_ignored_for_now" (
       .i1(in_a),
       .o2(out_b)); */
  qux q(/*AUTOINST*/
    // Inputs
    .i1(in_a),
    // Inouts
    .io(io),
    // Outputs
    .o2(out_b));

  /* bar AUTO_TEMPLATE "some_regex_ignored_for_now" (
       .i1(input_1),
       .o2(output_2),
       .i2(input_2),
       .io(input_output),
       .o1(output_1)); */
  bar b(/*AUTOINST*/
    // Inputs
    .i1(input_1),
    .i2(input_2),
    // Inouts
    .io(input_output),
    // Outputs
    .o1(output_1),
    .o2(output_2));
endmodule

module bar;
  input i1;
  input i2;
  inout io;
  output o1;
  output o2;
endmodule

module qux;
  input i1;
  inout io;
  output o2;
endmodule
)");
}

TEST(Autoexpand, AUTO_TEMPLATE_Mismatch) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module foo;
  /* quux AUTO_TEMPLATE
     bar AUTO_TEMPLATE "some_regex_ignored_for_now" (
       .i1(in_a),
       .o2(out_b)); */
  qux q(/*AUTOINST*/);
  bar b(/*AUTOINST*/);
endmodule

module bar;
  input i1;
  input i2;
  inout io;
  output o1;
  output o2;
endmodule

module qux;
  input i1;
  inout io;
  output o2;
endmodule
)",
                R"(
module foo;
  /* quux AUTO_TEMPLATE
     bar AUTO_TEMPLATE "some_regex_ignored_for_now" (
       .i1(in_a),
       .o2(out_b)); */
  qux q(/*AUTOINST*/
    // Inputs
    .i1(i1),
    // Inouts
    .io(io),
    // Outputs
    .o2(o2));
  bar b(/*AUTOINST*/
    // Inputs
    .i1(in_a),
    .i2(i2),
    // Inouts
    .io(io),
    // Outputs
    .o1(o1),
    .o2(out_b));
endmodule

module bar;
  input i1;
  input i2;
  inout io;
  output o1;
  output o2;
endmodule

module qux;
  input i1;
  inout io;
  output o2;
endmodule
)");
}

TEST(Autoexpand, AUTOINPUT_ExpandEmpty) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module bar(input i1, output o1);
  input i2;
  inout io;
  output o2;
endmodule

module foo;
  /*AUTOINPUT*/

  input i3;

  bar b(/*AUTOINST*/);
endmodule
)",
                R"(
module bar(input i1, output o1);
  input i2;
  inout io;
  output o2;
endmodule

module foo;
  /*AUTOINPUT*/
  // Beginning of automatic inputs (from autoinst inputs)
  input i1;  // To b of bar
  input i2;  // To b of bar
  // End of automatics

  input i3;

  bar b(/*AUTOINST*/
    // Inputs
    .i1(i1),
    .i2(i2),
    // Inouts
    .io(io),
    // Outputs
    .o1(o1),
    .o2(o2));
endmodule
)");
}

TEST(Autoexpand, AUTOINPUT_NoExpand) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module bar;
endmodule

module foo;
  /*AUTOINPUT*/

  bar b(/*AUTOINST*/);
endmodule
)",
                R"(
module bar;
endmodule

module foo;
  /*AUTOINPUT*/

  bar b(/*AUTOINST*/);
endmodule
)");
}

TEST(Autoexpand, AUTOINPUT_Replace) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module bar(input i1, output o1);
  input i2;
  inout io;
  output o2;
endmodule

module foo;
  /*AUTOINPUT*/
  // Beginning of automatic inputs (from autoinst inputs)
  input in_1;  // To b of bar
  input in_2;  // To b of bar
  // End of automatics

  bar b(/*AUTOINST*/);
endmodule
)",
                R"(
module bar(input i1, output o1);
  input i2;
  inout io;
  output o2;
endmodule

module foo;
  /*AUTOINPUT*/
  // Beginning of automatic inputs (from autoinst inputs)
  input i1;  // To b of bar
  input i2;  // To b of bar
  // End of automatics

  bar b(/*AUTOINST*/
    // Inputs
    .i1(i1),
    .i2(i2),
    // Inouts
    .io(io),
    // Outputs
    .o1(o1),
    .o2(o2));
endmodule
)");
}

TEST(Autoexpand, AUTOINOUT_ExpandEmpty) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module bar(input i1, output o1);
  input i2;
  inout io1;
  output o2;
endmodule

module foo;
  /*AUTOINOUT*/

  inout io2;

  bar b(/*AUTOINST*/);
endmodule
)",
                R"(
module bar(input i1, output o1);
  input i2;
  inout io1;
  output o2;
endmodule

module foo;
  /*AUTOINOUT*/
  // Beginning of automatic inouts (from autoinst inouts)
  inout io1;  // To/From b of bar
  // End of automatics

  inout io2;

  bar b(/*AUTOINST*/
    // Inputs
    .i1(i1),
    .i2(i2),
    // Inouts
    .io1(io1),
    // Outputs
    .o1(o1),
    .o2(o2));
endmodule
)");
}

TEST(Autoexpand, AUTOINOUT_NoExpand) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module bar;
endmodule

module foo;
  /*AUTOINOUT*/

  bar b(/*AUTOINST*/);
endmodule
)",
                R"(
module bar;
endmodule

module foo;
  /*AUTOINOUT*/

  bar b(/*AUTOINST*/);
endmodule
)");
}

TEST(Autoexpand, AUTOINOUT_Replace) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module bar(input i1, output o1);
  input i2;
  inout io;
  output o2;
endmodule

module foo;
  /*AUTOINOUT*/
  // Beginning of automatic inouts (from autoinst inouts)
  input in_out;  // To/From b of bar
  // End of automatics

  bar b(/*AUTOINST*/);
endmodule
)",
                R"(
module bar(input i1, output o1);
  input i2;
  inout io;
  output o2;
endmodule

module foo;
  /*AUTOINOUT*/
  // Beginning of automatic inouts (from autoinst inouts)
  inout io;  // To/From b of bar
  // End of automatics

  bar b(/*AUTOINST*/
    // Inputs
    .i1(i1),
    .i2(i2),
    // Inouts
    .io(io),
    // Outputs
    .o1(o1),
    .o2(o2));
endmodule
)");
}

TEST(Autoexpand, AUTOOUTPUT_ExpandEmpty) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module bar(input i1, output o1);
  input i2;
  inout io;
  output o2;
endmodule

module foo;
  /*AUTOOUTPUT*/

  output o3;

  bar b(/*AUTOINST*/);
endmodule
)",
                R"(
module bar(input i1, output o1);
  input i2;
  inout io;
  output o2;
endmodule

module foo;
  /*AUTOOUTPUT*/
  // Beginning of automatic outputs (from autoinst outputs)
  output o1;  // From b of bar
  output o2;  // From b of bar
  // End of automatics

  output o3;

  bar b(/*AUTOINST*/
    // Inputs
    .i1(i1),
    .i2(i2),
    // Inouts
    .io(io),
    // Outputs
    .o1(o1),
    .o2(o2));
endmodule
)");
}

TEST(Autoexpand, AUTOOUTPUT_NoExpand) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module bar;
endmodule

module foo;
  /*AUTOOUTPUT*/

  bar b(/*AUTOINST*/);
endmodule
)",
                R"(
module bar;
endmodule

module foo;
  /*AUTOOUTPUT*/

  bar b(/*AUTOINST*/);
endmodule
)");
}

TEST(Autoexpand, AUTOOUTPUT_Replace) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module bar(input i1, output o1);
  input i2;
  inout io;
  output o2;
endmodule

module foo;
  /*AUTOOUTPUT*/
  // Beginning of automatic outputs (from autoinst outputs)
  output out_1;  // From b of bar
  output out_2;  // From b of bar
  // End of automatics

  bar b(/*AUTOINST*/);
endmodule
)",
                R"(
module bar(input i1, output o1);
  input i2;
  inout io;
  output o2;
endmodule

module foo;
  /*AUTOOUTPUT*/
  // Beginning of automatic outputs (from autoinst outputs)
  output o1;  // From b of bar
  output o2;  // From b of bar
  // End of automatics

  bar b(/*AUTOINST*/
    // Inputs
    .i1(i1),
    .i2(i2),
    // Inouts
    .io(io),
    // Outputs
    .o1(o1),
    .o2(o2));
endmodule
)");
}

TEST(Autoexpand, AUTO_ExpandPorts) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module bar(input i1, output o1);
  input i2;
  inout io;
  output o2;
endmodule

module foo(/*AUTOARG*/);
  /*AUTOINPUT*/
  /*AUTOOUTPUT*/
  /*AUTOINOUT*/

  bar b(/*AUTOINST*/);
endmodule
)",
                R"(
module bar(input i1, output o1);
  input i2;
  inout io;
  output o2;
endmodule

module foo(/*AUTOARG*/
  // Inputs
  i1, i2,
  // Inouts
  io,
  // Outputs
  o1, o2
  );
  /*AUTOINPUT*/
  // Beginning of automatic inputs (from autoinst inputs)
  input i1;  // To b of bar
  input i2;  // To b of bar
  // End of automatics
  /*AUTOOUTPUT*/
  // Beginning of automatic outputs (from autoinst outputs)
  output o1;  // From b of bar
  output o2;  // From b of bar
  // End of automatics
  /*AUTOINOUT*/
  // Beginning of automatic inouts (from autoinst inouts)
  inout io;  // To/From b of bar
  // End of automatics

  bar b(/*AUTOINST*/
    // Inputs
    .i1(i1),
    .i2(i2),
    // Inouts
    .io(io),
    // Outputs
    .o1(o1),
    .o2(o2));
endmodule
)");
}

TEST(Autoexpand, AUTO_ExpandPorts_OutOfOrderModules) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module foo(/*AUTOARG*/);
  /*AUTOINPUT*/
  /*AUTOOUTPUT*/
  /*AUTOINOUT*/

  bar b(/*AUTOINST*/);
endmodule

module bar(input i1, output o1);
  /*AUTOINPUT*/
  /*AUTOOUTPUT*/

  inout io;
  qux q(/*AUTOINST*/);
endmodule

module qux(
  input i1,
  input i2,
  output o1,
  output o2);
endmodule
)",
                R"(
module foo(/*AUTOARG*/
  // Inputs
  i1, i2,
  // Inouts
  io,
  // Outputs
  o1, o2
  );
  /*AUTOINPUT*/
  // Beginning of automatic inputs (from autoinst inputs)
  input i1;  // To b of bar
  input i2;  // To b of bar
  // End of automatics
  /*AUTOOUTPUT*/
  // Beginning of automatic outputs (from autoinst outputs)
  output o1;  // From b of bar
  output o2;  // From b of bar
  // End of automatics
  /*AUTOINOUT*/
  // Beginning of automatic inouts (from autoinst inouts)
  inout io;  // To/From b of bar
  // End of automatics

  bar b(/*AUTOINST*/
    // Inputs
    .i1(i1),
    .i2(i2),
    // Inouts
    .io(io),
    // Outputs
    .o1(o1),
    .o2(o2));
endmodule

module bar(input i1, output o1);
  /*AUTOINPUT*/
  // Beginning of automatic inputs (from autoinst inputs)
  input i2;  // To q of qux
  // End of automatics
  /*AUTOOUTPUT*/
  // Beginning of automatic outputs (from autoinst outputs)
  output o2;  // From q of qux
  // End of automatics

  inout io;
  qux q(/*AUTOINST*/
    // Inputs
    .i1(i1),
    .i2(i2),
    // Outputs
    .o1(o1),
    .o2(o2));
endmodule

module qux(
  input i1,
  input i2,
  output o1,
  output o2);
endmodule
)");
}

TEST(Autoexpand, AUTOWIRE_ExpandEmpty) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module bar(input i1, output o1);
  input i2;
  inout io;
  output o2;
endmodule

module foo;
  wire o1;

  /*AUTOWIRE*/

  bar b(/*AUTOINST*/);
endmodule
)",
                R"(
module bar(input i1, output o1);
  input i2;
  inout io;
  output o2;
endmodule

module foo;
  wire o1;

  /*AUTOWIRE*/
  // Beginning of automatic wires (for undeclared instantiated-module outputs)
  wire io;  // To/From b of bar
  wire o2;  // From b of bar
  // End of automatics

  bar b(/*AUTOINST*/
    // Inputs
    .i1(i1),
    .i2(i2),
    // Inouts
    .io(io),
    // Outputs
    .o1(o1),
    .o2(o2));
endmodule
)");
}

TEST(Autoexpand, AUTOWIRE_NoExpand) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module bar;
endmodule

module foo;
  /*AUTOWIRE*/

  bar b(/*AUTOINST*/);
endmodule
)",
                R"(
module bar;
endmodule

module foo;
  /*AUTOWIRE*/

  bar b(/*AUTOINST*/);
endmodule
)");
}

TEST(Autoexpand, AUTOWIRE_Replace) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module bar(input i1, output o1);
  input i2;
  inout io;
  output o2;
endmodule

module foo;
  wire o1;

  /*AUTOWIRE*/
  // Beginning of automatic wires (for undeclared instantiated-module outputs)
  wire in_out;  // To/From b of bar
  wire out1;  // From b of bar
  wire out2;  // From b of bar
  // End of automatics

  bar b(/*AUTOINST*/);
endmodule
)",
                R"(
module bar(input i1, output o1);
  input i2;
  inout io;
  output o2;
endmodule

module foo;
  wire o1;

  /*AUTOWIRE*/
  // Beginning of automatic wires (for undeclared instantiated-module outputs)
  wire io;  // To/From b of bar
  wire o2;  // From b of bar
  // End of automatics

  bar b(/*AUTOINST*/
    // Inputs
    .i1(i1),
    .i2(i2),
    // Inouts
    .io(io),
    // Outputs
    .o1(o1),
    .o2(o2));
endmodule
)");
}

TEST(Autoexpand, AUTOREG_ExpandEmpty) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module bar(input i1, output o1);
  input i2;
  inout io;
  output o2;
endmodule

module foo;
  output o1;
  output o2;
  output o3;
  output o4;

  reg o4;

  /*AUTOREG*/

  bar b(/*AUTOINST*/);
endmodule
)",
                R"(
module bar(input i1, output o1);
  input i2;
  inout io;
  output o2;
endmodule

module foo;
  output o1;
  output o2;
  output o3;
  output o4;

  reg o4;

  /*AUTOREG*/
  // Beginning of automatic regs (for this module's undeclared outputs)
  reg o3;
  // End of automatics

  bar b(/*AUTOINST*/
    // Inputs
    .i1(i1),
    .i2(i2),
    // Inouts
    .io(io),
    // Outputs
    .o1(o1),
    .o2(o2));
endmodule
)");
}

TEST(Autoexpand, AUTOREG_NoExpand) {
  TestTextEdits(GenerateAutoExpandTextEdits,
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
}

TEST(Autoexpand, AUTOREG_Replace) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module bar(input i1, output o1);
  input i2;
  inout io;
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

  bar b(/*AUTOINST*/);
endmodule
)",
                R"(
module bar(input i1, output o1);
  input i2;
  inout io;
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

  bar b(/*AUTOINST*/
    // Inputs
    .i1(i1),
    .i2(i2),
    // Inouts
    .io(io),
    // Outputs
    .o1(o1),
    .o2(o2));
endmodule
)");
}

TEST(Autoexpand, AUTO_ExpandVars) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module bar(input i1, output o1);
  input i2;
  inout io;
  output o2;
  /*AUTOREG*/
endmodule

module foo;
  /*AUTOWIRE*/

  bar b(/*AUTOINST*/);
endmodule
)",
                R"(
module bar(input i1, output o1);
  input i2;
  inout io;
  output o2;
  /*AUTOREG*/
  // Beginning of automatic regs (for this module's undeclared outputs)
  reg o1;
  reg o2;
  // End of automatics
endmodule

module foo;
  /*AUTOWIRE*/
  // Beginning of automatic wires (for undeclared instantiated-module outputs)
  wire io;  // To/From b of bar
  wire o1;  // From b of bar
  wire o2;  // From b of bar
  // End of automatics

  bar b(/*AUTOINST*/
    // Inputs
    .i1(i1),
    .i2(i2),
    // Inouts
    .io(io),
    // Outputs
    .o1(o1),
    .o2(o2));
endmodule
)");
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module bar(input i1, output o1);
  input i2;
  inout io;
  output o2;
endmodule

module foo;
  output oo;

  /*AUTOREG*/

  /*AUTOWIRE*/

  bar b(/*AUTOINST*/);
endmodule
)",
                R"(
module bar(input i1, output o1);
  input i2;
  inout io;
  output o2;
endmodule

module foo;
  output oo;

  /*AUTOREG*/
  // Beginning of automatic regs (for this module's undeclared outputs)
  reg oo;
  // End of automatics

  /*AUTOWIRE*/
  // Beginning of automatic wires (for undeclared instantiated-module outputs)
  wire io;  // To/From b of bar
  wire o1;  // From b of bar
  wire o2;  // From b of bar
  // End of automatics

  bar b(/*AUTOINST*/
    // Inputs
    .i1(i1),
    .i2(i2),
    // Inouts
    .io(io),
    // Outputs
    .o1(o1),
    .o2(o2));
endmodule
)");
}

TEST(Autoexpand, AUTO_ExpandPortsWithAUTOVars) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module qux(input ii, output oo);
endmodule

module bar(input i1, output o1);
  input i2;
  inout io;
  output o2;

  /*AUTOWIRE*/

  /*AUTOREG*/

  qux q(/*AUTOINST*/);
endmodule

module foo(/*AUTOARG*/);
  /*AUTOINPUT*/
  /*AUTOOUTPUT*/
  /*AUTOINOUT*/

  bar b(/*AUTOINST*/);
endmodule
)",
                R"(
module qux(input ii, output oo);
endmodule

module bar(input i1, output o1);
  input i2;
  inout io;
  output o2;

  /*AUTOWIRE*/
  // Beginning of automatic wires (for undeclared instantiated-module outputs)
  wire oo;  // From q of qux
  // End of automatics

  /*AUTOREG*/
  // Beginning of automatic regs (for this module's undeclared outputs)
  reg o1;
  reg o2;
  // End of automatics

  qux q(/*AUTOINST*/
    // Inputs
    .ii(ii),
    // Outputs
    .oo(oo));
endmodule

module foo(/*AUTOARG*/
  // Inputs
  i1, i2,
  // Inouts
  io,
  // Outputs
  o1, o2
  );
  /*AUTOINPUT*/
  // Beginning of automatic inputs (from autoinst inputs)
  input i1;  // To b of bar
  input i2;  // To b of bar
  // End of automatics
  /*AUTOOUTPUT*/
  // Beginning of automatic outputs (from autoinst outputs)
  output o1;  // From b of bar
  output o2;  // From b of bar
  // End of automatics
  /*AUTOINOUT*/
  // Beginning of automatic inouts (from autoinst inouts)
  inout io;  // To/From b of bar
  // End of automatics

  bar b(/*AUTOINST*/
    // Inputs
    .i1(i1),
    .i2(i2),
    // Inouts
    .io(io),
    // Outputs
    .o1(o1),
    .o2(o2));
endmodule
)");
}

TEST(Autoexpand, CodeActionExpandAll) {
  TestTextEdits(GenerateAutoExpandTextEdits,
                R"(
module foo(/*AUTOARG*/);
  /*AUTOINPUT*/
  /*AUTOOUTPUT*/

  bar b(/*AUTOINST*/);
endmodule

module bar(/*AUTOARG*/);
  input clk;
  input rst;
  output o1;
  output o2;

  /*AUTOREG*/
endmodule
)",
                R"(
module foo(/*AUTOARG*/
  // Inputs
  clk, rst,
  // Outputs
  o1, o2
  );
  /*AUTOINPUT*/
  // Beginning of automatic inputs (from autoinst inputs)
  input clk;  // To b of bar
  input rst;  // To b of bar
  // End of automatics
  /*AUTOOUTPUT*/
  // Beginning of automatic outputs (from autoinst outputs)
  output o1;  // From b of bar
  output o2;  // From b of bar
  // End of automatics

  bar b(/*AUTOINST*/
    // Inputs
    .clk(clk),
    .rst(rst),
    // Outputs
    .o1(o1),
    .o2(o2));
endmodule

module bar(/*AUTOARG*/
  // Inputs
  clk, rst,
  // Outputs
  o1, o2
  );
  input clk;
  input rst;
  output o1;
  output o2;

  /*AUTOREG*/
  // Beginning of automatic regs (for this module's undeclared outputs)
  reg o1;
  reg o2;
  // End of automatics
endmodule
)");
}

TEST(Autoexpand, CodeActionExpandRange) {
  TestTextEdits(
      [](SymbolTableHandler* symbol_table_handler, BufferTracker* tracker) {
        return AutoExpandCodeActionToTextEdits(
            symbol_table_handler, tracker,
            {.start = {.line = 0}, .end = {.line = 10}},
            "Expand all AUTOs in selected range");
      },
      R"(
module foo(/*AUTOARG*/);
  /*AUTOINPUT*/
  /*AUTOOUTPUT*/

  /* qux AUTO_TEMPLATE
     bar AUTO_TEMPLATE ".*" (
       .o1(out_a),
       .o2(out_b)
     ); */
  bar b(/*AUTOINST*/);
endmodule

module bar(/*AUTOARG*/);
  input clk;
  input rst;
  output o1;
  output o2;

  /*AUTOREG*/
endmodule
)",
      R"(
module foo(/*AUTOARG*/
  // Inputs
  clk, rst,
  // Outputs
  out_a, out_b
  );
  /*AUTOINPUT*/
  // Beginning of automatic inputs (from autoinst inputs)
  input clk;  // To b of bar
  input rst;  // To b of bar
  // End of automatics
  /*AUTOOUTPUT*/
  // Beginning of automatic outputs (from autoinst outputs)
  output out_a;  // From b of bar
  output out_b;  // From b of bar
  // End of automatics

  /* qux AUTO_TEMPLATE
     bar AUTO_TEMPLATE ".*" (
       .o1(out_a),
       .o2(out_b)
     ); */
  bar b(/*AUTOINST*/
    // Inputs
    .clk(clk),
    .rst(rst),
    // Outputs
    .o1(out_a),
    .o2(out_b));
endmodule

module bar(/*AUTOARG*/);
  input clk;
  input rst;
  output o1;
  output o2;

  /*AUTOREG*/
endmodule
)",
      false  // Do not repeat: the range is incorrect after the first expansion
  );
}

}  // namespace
}  // namespace verilog
