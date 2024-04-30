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

#include "verilog/tools/ls/verilog-language-server.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "common/lsp/lsp-file-utils.h"
#include "common/lsp/lsp-protocol-enums.h"
#include "common/lsp/lsp-protocol.h"
#include "common/strings/line_column_map.h"
#include "common/util/file_util.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "verilog/analysis/verilog_linter.h"

#undef ASSERT_OK
#define ASSERT_OK(value)                             \
  if (const auto &status__ = (value); status__.ok()) \
    ;                                                \
  else                                               \
    EXPECT_TRUE(status__.ok()) << status__

namespace verilog {
namespace {

// TODO (glatosinski) for JSON messages use types defined in lsp-protocol.h

using nlohmann::json;
using verible::lsp::PathToLSPUri;

// TODO (glatosinski) use better sample modules
static constexpr absl::string_view  //
    kSampleModuleA(
        R"(module a;
  assign var1 = 1'b0;
  assign var2 = var1 | 1'b1;
endmodule
)");
static constexpr absl::string_view  //
    kSampleModuleB(
        R"(module b;
  assign var1 = 1'b0;
  assign var2 = var1 | 1'b1;
  a vara;
  assign vara.var1 = 1'b1;
endmodule
)");

class VerilogLanguageServerTest : public ::testing::Test {
 public:
  // Sends initialize request from client mock to the Language Server.
  // It does not parse the response nor fetch it in any way (for other
  // tests to check e.g. server/client capabilities).
  virtual absl::Status InitializeCommunication() {
    const absl::string_view initialize =
        R"({ "jsonrpc": "2.0", "id": 1, "method": "initialize", "params": null })";

    SetRequest(initialize);
    return ServerStep();
  }

  // Runs SetRequest and ServerStep, returning the status from the
  // Language Server
  // TODO: accept nlohmann::json object directly ?
  absl::Status SendRequest(absl::string_view request) {
    SetRequest(request);
    return ServerStep();
  }

  // Returns the latest responses from the Language Server
  std::string GetResponse() {
    std::string response = response_stream_.str();
    response_stream_.str("");
    response_stream_.clear();
    absl::SetFlag(&FLAGS_rules_config_search, true);
    return response;
  }

  // Returns response to textDocument/initialize request
  const std::string &GetInitializeResponse() const {
    return initialize_response_;
  }

 protected:
  // Wraps request for the Language Server in RPC header
  void SetRequest(absl::string_view request) {
    request_stream_.clear();
    request_stream_.str(
        absl::StrCat("Content-Length: ", request.size(), "\r\n\r\n", request));
  }

  // Performs single VerilogLanguageServer step, fetching latest request
  absl::Status ServerStep() {
    return server_->Step([this](char *message, int size) -> int {
      request_stream_.read(message, size);
      return request_stream_.gcount();
    });
  }

  // Sets up the testing environment - creates Language Server object and
  // sends textDocument/initialize request.
  // It stores the response in initialize_response field for further processing
  void SetUp() override {  // not yet final
    server_ = std::make_unique<VerilogLanguageServer>(
        [this](absl::string_view response) { response_stream_ << response; });

    absl::Status status = InitializeCommunication();
    EXPECT_TRUE(status.ok()) << "Failed to read request:  " << status;
    initialize_response_ = GetResponse();
  }

  // Currently tested instance of VerilogLanguageServer
  std::unique_ptr<VerilogLanguageServer> server_;

  // Response from textDocument/initialize request - left for checking e.g
  // server capabilities
  std::string initialize_response_;

  // Stream for passing requests to the Language Server
  std::stringstream request_stream_;

  // Stream for receiving responses from the Language Server
  std::stringstream response_stream_;
};

class VerilogLanguageServerSymbolTableTest : public VerilogLanguageServerTest {
 public:
  absl::Status InitializeCommunication() final {
    json initialize_request = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "initialize"},
        {"params", {{"rootUri", PathToLSPUri(root_dir)}}}};
    return SendRequest(initialize_request.dump());
  }

 protected:
  void SetUp() final {
    absl::SetFlag(&FLAGS_rules_config_search, true);
    root_dir = verible::file::JoinPath(
        ::testing::TempDir(),
        ::testing::UnitTest::GetInstance()->current_test_info()->name());
    absl::Status status = verible::file::CreateDir(root_dir);
    ASSERT_OK(status) << status;
    VerilogLanguageServerTest::SetUp();
  }

  void TearDown() final { std::filesystem::remove(root_dir); }

  // path to the project
  std::string root_dir;
};

// Verifies textDocument/initialize request handling
TEST_F(VerilogLanguageServerTest, InitializeRequest) {
  std::string response_str = GetInitializeResponse();
  json response = json::parse(response_str);

  EXPECT_EQ(response["id"], 1) << "Response message ID invalid";
  EXPECT_EQ(response["result"]["serverInfo"]["name"],
            "Verible Verilog language server.")
      << "Invalid Language Server name";
}

static std::string DidOpenRequest(absl::string_view name,
                                  absl::string_view content) {
  return nlohmann::json{//
                        {"jsonrpc", "2.0"},
                        {"method", "textDocument/didOpen"},
                        {"params",
                         {{"textDocument",
                           {
                               {"uri", name},
                               {"text", content},
                           }}}}}
      .dump();
}

// Checks automatic diagnostics for opened file and textDocument/diagnostic
// request for file with invalid syntax
TEST_F(VerilogLanguageServerTest, SyntaxError) {
  const std::string wrong_file =
      DidOpenRequest("file://syntaxerror.sv", "brokenfile");
  ASSERT_OK(SendRequest(wrong_file)) << "process file with syntax error";
  json response = json::parse(GetResponse());
  EXPECT_EQ(response["method"], "textDocument/publishDiagnostics")
      << "textDocument/publishDiagnostics not received";
  EXPECT_EQ(response["params"]["uri"], "file://syntaxerror.sv")
      << "Diagnostics for invalid file";
  EXPECT_TRUE(absl::StrContains(
      response["params"]["diagnostics"][0]["message"].get<std::string>(),
      "syntax error"))
      << "No syntax error found";

  // query diagnostics explicitly
  const absl::string_view diagnostic_request = R"(
    {
      "jsonrpc": "2.0", "id": 2, "method": "textDocument/diagnostic",
      "params":
      {
        "textDocument": {"uri": "file://syntaxerror.sv"}
      }
    }
  )";
  ASSERT_OK(SendRequest(diagnostic_request))
      << "Failed to process file with syntax error";
  response = json::parse(GetResponse());
  EXPECT_EQ(response["id"], 2) << "Invalid id";
  EXPECT_EQ(response["result"]["kind"], "full") << "Diagnostics kind invalid";
  EXPECT_TRUE(absl::StrContains(
      response["result"]["items"][0]["message"].get<std::string>(),
      "syntax error"))
      << "No syntax error found";
}

// Tests diagnostics for file with linting error before and after fix
TEST_F(VerilogLanguageServerTest, LintErrorDetection) {
  const std::string lint_error =
      DidOpenRequest("file://mini.sv", "module mini();\nendmodule");
  ASSERT_OK(SendRequest(lint_error)) << "process file with linting error";

  const json diagnostics = json::parse(GetResponse());

  // Firstly, check correctness of diagnostics
  EXPECT_EQ(diagnostics["method"], "textDocument/publishDiagnostics")
      << "textDocument/publishDiagnostics not received";
  EXPECT_EQ(diagnostics["params"]["uri"], "file://mini.sv")
      << "Diagnostics for invalid file";
  EXPECT_TRUE(absl::StrContains(
      diagnostics["params"]["diagnostics"][0]["message"].get<std::string>(),
      "File must end with a newline."))
      << "No syntax error found";
  EXPECT_EQ(diagnostics["params"]["diagnostics"][0]["range"]["start"]["line"],
            1);
  EXPECT_EQ(
      diagnostics["params"]["diagnostics"][0]["range"]["start"]["character"],
      9);

  // Secondly, request a code action at the EOF error message position
  const absl::string_view action_request =
      R"({"jsonrpc":"2.0", "id":10, "method":"textDocument/codeAction","params":{"textDocument":{"uri":"file://mini.sv"},"range":{"start":{"line":1,"character":9},"end":{"line":1,"character":9}}}})";
  ASSERT_OK(SendRequest(action_request));

  const json action = json::parse(GetResponse());
  EXPECT_EQ(action["id"], 10);
  EXPECT_EQ(
      action["result"][0]["edit"]["changes"]["file://mini.sv"][0]["newText"],
      "\n");
  // Thirdly, apply change suggested by a code action and check diagnostics
  const absl::string_view apply_fix =
      R"({"jsonrpc":"2.0","method":"textDocument/didChange","params":{"textDocument":{"uri":"file://mini.sv"},"contentChanges":[{"range":{"start":{"character":9,"line":1},"end":{"character":9,"line":1}},"text":"\n"}]}})";
  ASSERT_OK(SendRequest(apply_fix));

  const json diagnostic_of_fixed = json::parse(GetResponse());
  EXPECT_EQ(diagnostic_of_fixed["method"], "textDocument/publishDiagnostics");
  EXPECT_EQ(diagnostic_of_fixed["params"]["uri"], "file://mini.sv");
  EXPECT_EQ(diagnostic_of_fixed["params"]["diagnostics"].size(), 0);
}

// Tests textDocument/documentSymbol request support; expect document outline.
TEST_F(VerilogLanguageServerTest, DocumentSymbolRequestTest) {
  // Create file, absorb diagnostics
  const std::string mini_module = DidOpenRequest("file://mini_pkg.sv", R"(
package mini;

function static void fun_foo();
endfunction

class some_class;
   function void member();
   endfunction
endclass
endpackage

module mini(input clk);
  always@(posedge clk) begin : labelled_block
  end

  reg foo;
  net bar;
  some_class baz();

endmodule
)");

  ASSERT_OK(SendRequest(mini_module));

  // Expect to receive diagnostics right away. Ignore.
  const json diagnostics = json::parse(GetResponse());
  EXPECT_EQ(diagnostics["method"], "textDocument/publishDiagnostics")
      << "textDocument/publishDiagnostics not received";

  // Request a document symbol
  const absl::string_view document_symbol_request =
      R"({"jsonrpc":"2.0", "id":11, "method":"textDocument/documentSymbol","params":{"textDocument":{"uri":"file://mini_pkg.sv"}}})";
  ASSERT_OK(SendRequest(document_symbol_request));

  // TODO: by default, the Kate workarounds are active, so
  // Module -> Method and Namespace -> Class. Remove by default.
  const json document_symbol = json::parse(GetResponse());
  EXPECT_EQ(document_symbol["id"], 11);

  std::vector<verible::lsp::DocumentSymbol> toplevel =
      document_symbol["result"];
  EXPECT_EQ(toplevel.size(), 2);

  EXPECT_EQ(toplevel[0].kind, verible::lsp::SymbolKind::kPackage);
  EXPECT_EQ(toplevel[0].name, "mini");

  EXPECT_EQ(toplevel[1].kind, verible::lsp::SymbolKind::kMethod);  // module.
  EXPECT_EQ(toplevel[1].name, "mini");

  // Descend tree into package and look at expected nested symbols there.
  std::vector<verible::lsp::DocumentSymbol> package = toplevel[0].children;
  EXPECT_EQ(package.size(), 2);
  EXPECT_EQ(package[0].kind, verible::lsp::SymbolKind::kFunction);
  EXPECT_EQ(package[0].name, "fun_foo");

  EXPECT_EQ(package[1].kind, verible::lsp::SymbolKind::kClass);
  EXPECT_EQ(package[1].name, "some_class");

  // Descend tree into class and find nested function.
  std::vector<verible::lsp::DocumentSymbol> class_block = package[1].children;
  EXPECT_EQ(class_block.size(), 1);
  EXPECT_EQ(class_block[0].kind, verible::lsp::SymbolKind::kFunction);
  EXPECT_EQ(class_block[0].name, "member");

  // Descent tree into module and find labelled block.
  std::vector<verible::lsp::DocumentSymbol> module = toplevel[1].children;
  EXPECT_EQ(module.size(), 4);
  EXPECT_EQ(module[0].kind, verible::lsp::SymbolKind::kNamespace);
  EXPECT_EQ(module[0].name, "labelled_block");

  EXPECT_EQ(module[1].kind, verible::lsp::SymbolKind::kVariable);
  EXPECT_EQ(module[1].name, "foo");

  EXPECT_EQ(module[2].kind, verible::lsp::SymbolKind::kVariable);
  EXPECT_EQ(module[2].name, "bar");

  EXPECT_EQ(module[3].kind, verible::lsp::SymbolKind::kVariable);
  EXPECT_EQ(module[3].name, "baz");
}

TEST_F(VerilogLanguageServerTest, DocumentSymbolRequestWithoutVariablesTest) {
  server_->include_variables = false;
  // Create file, absorb diagnostics
  const std::string mini_module = DidOpenRequest("file://mini_pkg.sv", R"(
package mini;

function static void fun_foo();
endfunction

class some_class;
   function void member();
   endfunction
endclass
endpackage

module mini(input clk);
  always@(posedge clk) begin : labelled_block
  end

  reg foo;
  net bar;
  some_class baz();

endmodule
)");

  ASSERT_OK(SendRequest(mini_module));

  // Expect to receive diagnostics right away. Ignore.
  const json diagnostics = json::parse(GetResponse());
  EXPECT_EQ(diagnostics["method"], "textDocument/publishDiagnostics")
      << "textDocument/publishDiagnostics not received";

  // Request a document symbol
  const absl::string_view document_symbol_request =
      R"({"jsonrpc":"2.0", "id":11, "method":"textDocument/documentSymbol","params":{"textDocument":{"uri":"file://mini_pkg.sv"}}})";
  ASSERT_OK(SendRequest(document_symbol_request));

  // TODO: by default, the Kate workarounds are active, so
  // Module -> Method and Namespace -> Class. Remove by default.
  const json document_symbol = json::parse(GetResponse());
  EXPECT_EQ(document_symbol["id"], 11);

  std::vector<verible::lsp::DocumentSymbol> toplevel =
      document_symbol["result"];
  EXPECT_EQ(toplevel.size(), 2);

  EXPECT_EQ(toplevel[0].kind, verible::lsp::SymbolKind::kPackage);
  EXPECT_EQ(toplevel[0].name, "mini");

  EXPECT_EQ(toplevel[1].kind, verible::lsp::SymbolKind::kMethod);  // module.
  EXPECT_EQ(toplevel[1].name, "mini");

  // Descend tree into package and look at expected nested symbols there.
  std::vector<verible::lsp::DocumentSymbol> package = toplevel[0].children;
  EXPECT_EQ(package.size(), 2);
  EXPECT_EQ(package[0].kind, verible::lsp::SymbolKind::kFunction);
  EXPECT_EQ(package[0].name, "fun_foo");

  EXPECT_EQ(package[1].kind, verible::lsp::SymbolKind::kClass);
  EXPECT_EQ(package[1].name, "some_class");

  // Descend tree into class and find nested function.
  std::vector<verible::lsp::DocumentSymbol> class_block = package[1].children;
  EXPECT_EQ(class_block.size(), 1);
  EXPECT_EQ(class_block[0].kind, verible::lsp::SymbolKind::kFunction);
  EXPECT_EQ(class_block[0].name, "member");

  // Descent tree into module and find labelled block.
  std::vector<verible::lsp::DocumentSymbol> module = toplevel[1].children;
  EXPECT_EQ(module.size(), 1);
  EXPECT_EQ(module[0].kind, verible::lsp::SymbolKind::kNamespace);
  EXPECT_EQ(module[0].name, "labelled_block");
}
// Tests closing of the file in the LS context and checks if the LS
// responds gracefully to textDocument/documentSymbol request for
// closed file.
TEST_F(VerilogLanguageServerTest,
       DocumentClosingFollowedByDocumentSymbolRequest) {
  const std::string mini_module =
      DidOpenRequest("file://mini.sv", "module mini();\nendmodule\n");
  ASSERT_OK(SendRequest(mini_module));
  std::string discard_diagnostics = GetResponse();

  // Close the file from the Language Server perspective
  const absl::string_view closing_request = R"(
    {
      "jsonrpc":"2.0",
      "method":"textDocument/didClose",
      "params":{
        "textDocument":{
          "uri":"file://mini.sv"
        }
      }
    })";
  ASSERT_OK(SendRequest(closing_request));

  // Try to request document symbol for closed file (server should return empty
  // response gracefully)
  const absl::string_view document_symbol_request =
      R"({"jsonrpc":"2.0", "id":13, "method":"textDocument/documentSymbol","params":{"textDocument":{"uri":"file://mini.sv"}}})";
  ASSERT_OK(SendRequest(document_symbol_request));

  json document_symbol = json::parse(GetResponse());
  EXPECT_EQ(document_symbol["id"], 13);
  EXPECT_EQ(document_symbol["result"].size(), 0);
}

// Tests textDocument/documentHighlight request
TEST_F(VerilogLanguageServerTest, SymbolHighlightingTest) {
  // Create sample file and make sure diagnostics do not have errors
  const std::string mini_module = DidOpenRequest(
      "file://sym.sv", "module sym();\nassign a=1;assign b=a+1;endmodule\n");
  ASSERT_OK(SendRequest(mini_module));

  const json diagnostics = json::parse(GetResponse());
  EXPECT_EQ(diagnostics["method"], "textDocument/publishDiagnostics")
      << "textDocument/publishDiagnostics not received";
  EXPECT_EQ(diagnostics["params"]["uri"], "file://sym.sv")
      << "Diagnostics for invalid file";
  EXPECT_EQ(diagnostics["params"]["diagnostics"].size(), 0)
      << "The test file has errors";
  const absl::string_view highlight_request1 =
      R"({"jsonrpc":"2.0", "id":20, "method":"textDocument/documentHighlight","params":{"textDocument":{"uri":"file://sym.sv"},"position":{"line":1,"character":7}}})";
  ASSERT_OK(SendRequest(highlight_request1));

  const json highlight_response1 = json::parse(GetResponse());
  EXPECT_EQ(highlight_response1["id"], 20);
  EXPECT_EQ(highlight_response1["result"].size(), 2);
  EXPECT_EQ(
      highlight_response1["result"][0],
      json::parse(
          R"({"range":{"start":{"line":1, "character": 7}, "end":{"line":1, "character": 8}}})"));
  EXPECT_EQ(
      highlight_response1["result"][1],
      json::parse(
          R"({"range":{"start":{"line":1, "character": 20}, "end":{"line":1, "character": 21}}})"));

  const absl::string_view highlight_request2 =
      R"({"jsonrpc":"2.0", "id":21, "method":"textDocument/documentHighlight","params":{"textDocument":{"uri":"file://sym.sv"},"position":{"line":1,"character":2}}})";
  ASSERT_OK(SendRequest(highlight_request2));

  const json highlight_response2 = json::parse(GetResponse());
  EXPECT_EQ(highlight_response2["id"], 21);
  EXPECT_EQ(highlight_response2["result"].size(), 0);
}

// Tests structure holding data for test textDocument/rangeFormatting requests
struct FormattingRequestParams {
  FormattingRequestParams(int id, int start_line, int start_character,
                          int end_line, int end_character,
                          absl::string_view new_text, int new_text_start_line,
                          int new_text_start_character, int new_text_end_line,
                          int new_text_end_character)
      : id(id),
        start_line(start_line),
        start_character(start_character),
        end_line(end_line),
        end_character(end_character),
        new_text(new_text),
        new_text_start_line(new_text_start_line),
        new_text_start_character(new_text_start_character),
        new_text_end_line(new_text_end_line),
        new_text_end_character(new_text_end_character) {}

  int id;
  int start_line;
  int start_character;
  int end_line;
  int end_character;

  absl::string_view new_text;
  int new_text_start_line;
  int new_text_start_character;
  int new_text_end_line;
  int new_text_end_character;
};

// Creates a textDocument/rangeFormatting request from FormattingRequestParams
// structure
std::string FormattingRequest(absl::string_view file,
                              const FormattingRequestParams &params) {
  json formattingrequest = {
      {"jsonrpc", "2.0"},
      {"id", params.id},
      {"method", "textDocument/rangeFormatting"},
      {"params",
       {{"textDocument", {{"uri", file}}},
        {"range",
         {
             {"start",
              {{"line", params.start_line},
               {"character", params.start_character}}},
             {"end",
              {{"line", params.end_line}, {"character", params.end_character}}},
         }}}}};
  return formattingrequest.dump();
}

// Runs tests for textDocument/rangeFormatting requests
TEST_F(VerilogLanguageServerTest, RangeFormattingTest) {
  // Create sample file and make sure diagnostics do not have errors
  const std::string mini_module = DidOpenRequest(
      "file://fmt.sv", "module fmt();\nassign a=1;\nassign b=2;endmodule\n");
  ASSERT_OK(SendRequest(mini_module));

  const json diagnostics = json::parse(GetResponse());

  EXPECT_EQ(diagnostics["method"], "textDocument/publishDiagnostics")
      << "textDocument/publishDiagnostics not received";
  EXPECT_EQ(diagnostics["params"]["uri"], "file://fmt.sv")
      << "Diagnostics for invalid file";
  EXPECT_EQ(diagnostics["params"]["diagnostics"].size(), 0)
      << "The test file has errors";
  const std::vector<FormattingRequestParams> formatting_params{
      {30, 1, 0, 2, 0, "  assign a=1;\n", 1, 0, 2, 0},
      {31, 1, 0, 1, 1, "  assign a=1;\n", 1, 0, 2, 0},
      {32, 2, 0, 2, 1, "  assign b=2;\nendmodule\n", 2, 0, 3, 0},
      {33, 1, 0, 3, 0, "  assign a = 1;\n  assign b = 2;\nendmodule\n", 1, 0, 3,
       0}};

  for (const auto &params : formatting_params) {
    const std::string request = FormattingRequest("file://fmt.sv", params);
    ASSERT_OK(SendRequest(request));

    const json response = json::parse(GetResponse());
    EXPECT_EQ(response["id"], params.id) << "Invalid id";
    EXPECT_EQ(response["result"].size(), 1)
        << "Invalid result size for id:  " << params.id;
    EXPECT_EQ(std::string(response["result"][0]["newText"]), params.new_text)
        << "Invalid patch for id:  " << params.id;
    EXPECT_EQ(response["result"][0]["range"]["start"]["line"],
              params.new_text_start_line)
        << "Invalid range for id:  " << params.id;
    EXPECT_EQ(response["result"][0]["range"]["start"]["character"],
              params.new_text_start_character)
        << "Invalid range for id:  " << params.id;
    EXPECT_EQ(response["result"][0]["range"]["end"]["line"],
              params.new_text_end_line)
        << "Invalid range for id:  " << params.id;
    EXPECT_EQ(response["result"][0]["range"]["end"]["character"],
              params.new_text_end_character)
        << "Invalid range for id:  " << params.id;
  }
}

// Runs test of entire document formatting with textDocument/formatting request
TEST_F(VerilogLanguageServerTest, FormattingTest) {
  // Create sample file and make sure diagnostics do not have errors
  const std::string mini_module = DidOpenRequest(
      "file://fmt.sv", "module fmt();\nassign a=1;\nassign b=2;endmodule\n");
  ASSERT_OK(SendRequest(mini_module));

  const json diagnostics = json::parse(GetResponse());

  EXPECT_EQ(diagnostics["method"], "textDocument/publishDiagnostics")
      << "textDocument/publishDiagnostics not received";
  EXPECT_EQ(diagnostics["params"]["uri"], "file://fmt.sv")
      << "Diagnostics for invalid file";
  EXPECT_EQ(diagnostics["params"]["diagnostics"].size(), 0)
      << "The test file has errors";

  const absl::string_view formatting_request =
      R"({"jsonrpc":"2.0", "id":34, "method":"textDocument/formatting","params":{"textDocument":{"uri":"file://fmt.sv"}}})";

  ASSERT_OK(SendRequest(formatting_request));

  const json response = json::parse(GetResponse());
  EXPECT_EQ(response["id"], 34);
  EXPECT_EQ(response["result"].size(), 1);
  EXPECT_EQ(std::string(response["result"][0]["newText"]),
            "module fmt ();\n  assign a = 1;\n  assign b = 2;\nendmodule\n");
  EXPECT_EQ(
      response["result"][0]["range"],
      json::parse(
          R"({"start":{"line":0, "character": 0}, "end":{"line":3, "character": 0}})"));
}

TEST_F(VerilogLanguageServerTest, FormattingFileWithEmptyNewline_issue1667) {
  const std::string fmt_module = DidOpenRequest(
      "file://fmt.sv", "module fmt();\nassign a=1;\nassign b=2;endmodule");
  // ---------------------------------------------------- no newline ---^
  ASSERT_OK(SendRequest(fmt_module));

  GetResponse();  // Ignore diagnostics.

  const absl::string_view formatting_request = R"(
{"jsonrpc":"2.0", "id":1,
 "method": "textDocument/formatting",
 "params": {"textDocument":{"uri":"file://fmt.sv"}}})";

  ASSERT_OK(SendRequest(formatting_request));

  const json response = json::parse(GetResponse());

  // Formatted output now has a newline at end.
  EXPECT_EQ(std::string(response["result"][0]["newText"]),
            "module fmt ();\n  assign a = 1;\n  assign b = 2;\nendmodule\n");

  // Full range of original file, including the characters of the last line.
  EXPECT_EQ(response["result"][0]["range"], json::parse(R"(
{"start":{"line":0, "character": 0},
 "end":  {"line":2, "character": 20}})"));
}

TEST_F(VerilogLanguageServerTest, FormattingFileWithSyntaxErrors_issue1843) {
  // Contains syntax errors. Shouldn't crash
  const std::string file_contents =
      "module fmt(input logic a,);\nassign a=1;\nendmodule";
  const std::string fmt_module = DidOpenRequest("file://fmt.sv", file_contents);
  ASSERT_OK(SendRequest(fmt_module));

  GetResponse();  // Ignore diagnostics.

  const absl::string_view formatting_request = R"(
{"jsonrpc":"2.0", "id":1,
 "method": "textDocument/formatting",
 "params": {"textDocument":{"uri":"file://fmt.sv"}}})";

  // Doesn't crash.
  ASSERT_OK(SendRequest(formatting_request));
  const json response = json::parse(GetResponse());
}

// Creates a request based on TextDocumentPosition parameters
std::string TextDocumentPositionBasedRequest(absl::string_view method,
                                             absl::string_view file, int id,
                                             int line, int character) {
  verible::lsp::TextDocumentPositionParams params{
      .textDocument = {.uri = {file.begin(), file.end()}},
      .position = {.line = line, .character = character}};
  json request = {
      {"jsonrpc", "2.0"}, {"id", id}, {"method", method}, {"params", params}};
  return request.dump();
}

// Creates a textDocument/definition request
std::string DefinitionRequest(absl::string_view file, int id, int line,
                              int character) {
  return TextDocumentPositionBasedRequest("textDocument/definition", file, id,
                                          line, character);
}

// Creates a textDocument/references request
std::string ReferencesRequest(absl::string_view file, int id, int line,
                              int character) {
  return TextDocumentPositionBasedRequest("textDocument/references", file, id,
                                          line, character);
}

void CheckDefinitionEntry(const json &entry, verible::LineColumn start,
                          verible::LineColumn end,
                          const std::string &file_uri) {
  ASSERT_EQ(entry["range"]["start"]["line"], start.line);
  ASSERT_EQ(entry["range"]["start"]["character"], start.column);
  ASSERT_EQ(entry["range"]["end"]["line"], end.line);
  ASSERT_EQ(entry["range"]["end"]["character"], end.column);
  ASSERT_EQ(entry["uri"], file_uri);
}

// Performs assertions on textDocument/definition responses where single
// definition is expected
void CheckDefinitionResponseSingleDefinition(const json &response, int id,
                                             verible::LineColumn start,
                                             verible::LineColumn end,
                                             const std::string &file_uri) {
  ASSERT_EQ(response["id"], id);
  ASSERT_EQ(response["result"].size(), 1);
  CheckDefinitionEntry(response["result"][0], start, end, file_uri);
}

// Creates a textDocument/hover request
std::string HoverRequest(absl::string_view file, int id, int line,
                         int character) {
  return TextDocumentPositionBasedRequest("textDocument/hover", file, id, line,
                                          character);
}

// Checks if the hover appears on port symbols
// In this test the hover for "sum" symbol in assign
// is checked
TEST_F(VerilogLanguageServerSymbolTableTest, HoverOverSymbol) {
  absl::string_view filelist_content = "mod.v\n";
  static constexpr absl::string_view  //
      module_content(
          R"(module mod(
    input clk,
    input reg [31:0] a,
    input reg [31:0] b,
    output reg [31:0] sum);
  always @(posedge clk) begin : addition
    assign sum = a + b; // hover over sum
  end
endmodule
)");

  const verible::file::testing::ScopedTestFile filelist(
      root_dir, filelist_content, "verible.filelist");
  const verible::file::testing::ScopedTestFile module(root_dir, module_content,
                                                      "mod.v");

  const std::string module_open_request =
      DidOpenRequest("file://" + module.filename(), module_content);
  ASSERT_OK(SendRequest(module_open_request));

  GetResponse();

  std::string hover_request = HoverRequest("file://" + module.filename(), 2,
                                           /* line */ 6, /* column */ 12);

  ASSERT_OK(SendRequest(hover_request));
  json response = json::parse(GetResponse());
  verible::lsp::Hover hover = response["result"];
  ASSERT_EQ(hover.contents.kind, "markdown");
  ASSERT_TRUE(
      absl::StrContains(hover.contents.value, "data/net/var/instance sum"));
  ASSERT_TRUE(absl::StrContains(hover.contents.value, "reg [31:0]"));
}

// Checks if the hover appears on "end" token when block name is available
TEST_F(VerilogLanguageServerSymbolTableTest, HoverOverEnd) {
  absl::string_view filelist_content = "mod.v\n";
  static constexpr absl::string_view  //
      module_content(
          R"(module mod(
    input clk,
    input reg [31:0] a,
    input reg [31:0] b,
    output reg [31:0] sum);
  always @(posedge clk) begin : addition
    assign sum = a + b;
  end // hover over end
endmodule
)");

  const verible::file::testing::ScopedTestFile filelist(
      root_dir, filelist_content, "verible.filelist");
  const verible::file::testing::ScopedTestFile module(root_dir, module_content,
                                                      "mod.v");

  const std::string module_open_request =
      DidOpenRequest("file://" + module.filename(), module_content);
  ASSERT_OK(SendRequest(module_open_request));

  GetResponse();

  std::string hover_request = HoverRequest("file://" + module.filename(), 2,
                                           /* line */ 7, /* column */ 3);

  ASSERT_OK(SendRequest(hover_request));
  json response = json::parse(GetResponse());
  verible::lsp::Hover hover = response["result"];

  ASSERT_EQ(hover.contents.kind, "markdown");
  ASSERT_TRUE(absl::StrContains(hover.contents.value, "End of block"));
  ASSERT_TRUE(absl::StrContains(hover.contents.value, "Name: addition"));
}

// Performs simple textDocument/definition request with no VerilogProject set
TEST_F(VerilogLanguageServerSymbolTableTest, DefinitionRequestNoProjectTest) {
  std::string definition_request = DefinitionRequest("file://b.sv", 2, 3, 18);
  ASSERT_OK(SendRequest(definition_request));
  json response = json::parse(GetResponse());

  ASSERT_EQ(response["id"], 2);
  ASSERT_EQ(response["result"].size(), 0);
}

// Performs simple textDocument/definition request
TEST_F(VerilogLanguageServerSymbolTableTest, DefinitionRequestTest) {
  absl::string_view filelist_content = "a.sv\n";

  const verible::file::testing::ScopedTestFile filelist(
      root_dir, filelist_content, "verible.filelist");
  const verible::file::testing::ScopedTestFile module_a(root_dir,
                                                        kSampleModuleA, "a.sv");

  const std::string module_a_uri = PathToLSPUri(module_a.filename());
  const std::string module_a_open_request =
      DidOpenRequest(module_a_uri, kSampleModuleA);
  ASSERT_OK(SendRequest(module_a_open_request));

  // obtain diagnostics
  GetResponse();

  // find definition for "var1" variable in a.sv file
  const std::string definition_request =
      DefinitionRequest(module_a_uri, 2, 2, 16);

  ASSERT_OK(SendRequest(definition_request));
  json response = json::parse(GetResponse());

  CheckDefinitionResponseSingleDefinition(response, 2, {.line = 1, .column = 9},
                                          {.line = 1, .column = 13},
                                          module_a_uri);
}

// Check textDocument/definition request when there are two symbols of the same
// name (variable name), but in different modules
TEST_F(VerilogLanguageServerSymbolTableTest,
       DefinitionRequestSameVariablesDifferentModules) {
  absl::string_view filelist_content = "a.sv\nb.sv\n";

  const verible::file::testing::ScopedTestFile filelist(
      root_dir, filelist_content, "verible.filelist");
  const verible::file::testing::ScopedTestFile module_a(root_dir,
                                                        kSampleModuleA, "a.sv");
  const verible::file::testing::ScopedTestFile module_b(root_dir,
                                                        kSampleModuleB, "b.sv");

  const std::string module_a_uri = PathToLSPUri(module_a.filename());
  const std::string module_b_uri = PathToLSPUri(module_b.filename());
  const std::string module_a_open_request =
      DidOpenRequest(module_a_uri, kSampleModuleA);
  ASSERT_OK(SendRequest(module_a_open_request));
  const std::string module_b_open_request =
      DidOpenRequest(module_b_uri, kSampleModuleB);
  ASSERT_OK(SendRequest(module_b_open_request));

  // obtain diagnostics for both files
  GetResponse();

  // find definition for "var1" variable in b.sv file
  std::string definition_request = DefinitionRequest(module_b_uri, 2, 2, 16);

  ASSERT_OK(SendRequest(definition_request));
  json response_b = json::parse(GetResponse());

  CheckDefinitionResponseSingleDefinition(
      response_b, 2, {.line = 1, .column = 9}, {.line = 1, .column = 13},
      module_b_uri);

  // find definition for "var1" variable in a.sv file
  const std::string definition_request2 =
      DefinitionRequest(module_a_uri, 3, 2, 16);

  ASSERT_OK(SendRequest(definition_request2));
  json response_a = json::parse(GetResponse());

  CheckDefinitionResponseSingleDefinition(
      response_a, 3, {.line = 1, .column = 9}, {.line = 1, .column = 13},
      module_a_uri);
}

// Check textDocument/definition request where we want definition of a symbol
// inside other module edited in buffer
TEST_F(VerilogLanguageServerSymbolTableTest,
       DefinitionRequestSymbolFromDifferentOpenedModule) {
  absl::string_view filelist_content = "a.sv\nb.sv\n";

  const verible::file::testing::ScopedTestFile filelist(
      root_dir, filelist_content, "verible.filelist");
  const verible::file::testing::ScopedTestFile module_a(root_dir,
                                                        kSampleModuleA, "a.sv");
  const verible::file::testing::ScopedTestFile module_b(root_dir,
                                                        kSampleModuleB, "b.sv");

  const std::string module_a_uri = PathToLSPUri(module_a.filename());
  const std::string module_b_uri = PathToLSPUri(module_b.filename());
  const std::string module_a_open_request =
      DidOpenRequest(module_a_uri, kSampleModuleA);
  ASSERT_OK(SendRequest(module_a_open_request));
  const std::string module_b_open_request =
      DidOpenRequest(module_b_uri, kSampleModuleB);
  ASSERT_OK(SendRequest(module_b_open_request));

  // obtain diagnostics for both files
  GetResponse();

  // find definition for "var1" variable in b.sv file
  const std::string definition_request =
      DefinitionRequest(module_b_uri, 2, 4, 14);

  ASSERT_OK(SendRequest(definition_request));
  json response_b = json::parse(GetResponse());

  CheckDefinitionResponseSingleDefinition(
      response_b, 2, {.line = 1, .column = 9}, {.line = 1, .column = 13},
      module_a_uri);
}

// Check textDocument/definition request where we want definition of a symbol
// inside other module that is not edited in buffer
TEST_F(VerilogLanguageServerSymbolTableTest,
       DefinitionRequestSymbolFromDifferentNotOpenedModule) {
  absl::string_view filelist_content = "a.sv\nb.sv\n";

  const verible::file::testing::ScopedTestFile filelist(
      root_dir, filelist_content, "verible.filelist");
  const verible::file::testing::ScopedTestFile module_a(root_dir,
                                                        kSampleModuleA, "a.sv");
  const verible::file::testing::ScopedTestFile module_b(root_dir,
                                                        kSampleModuleB, "b.sv");

  const std::string module_a_uri = PathToLSPUri(module_a.filename());
  const std::string module_b_uri = PathToLSPUri(module_b.filename());
  const std::string module_b_open_request =
      DidOpenRequest(module_b_uri, kSampleModuleB);
  ASSERT_OK(SendRequest(module_b_open_request));

  // obtain diagnostics for both files
  GetResponse();

  // find definition for "var1" variable in b.sv file
  const std::string definition_request =
      DefinitionRequest(module_b_uri, 2, 4, 14);

  ASSERT_OK(SendRequest(definition_request));
  json response_b = json::parse(GetResponse());

  CheckDefinitionResponseSingleDefinition(
      response_b, 2, {.line = 1, .column = 9}, {.line = 1, .column = 13},
      module_a_uri);
}

// Check textDocument/definition request where we want definition of a symbol
// inside other module which was opened and closed
TEST_F(VerilogLanguageServerSymbolTableTest,
       DefinitionRequestSymbolFromDifferentOpenedAndClosedModule) {
  absl::string_view filelist_content = "a.sv\nb.sv\n";

  const verible::file::testing::ScopedTestFile filelist(
      root_dir, filelist_content, "verible.filelist");
  const verible::file::testing::ScopedTestFile module_a(root_dir,
                                                        kSampleModuleA, "a.sv");
  const verible::file::testing::ScopedTestFile module_b(root_dir,
                                                        kSampleModuleB, "b.sv");

  const std::string module_a_uri = PathToLSPUri(module_a.filename());
  const std::string module_b_uri = PathToLSPUri(module_b.filename());
  const std::string module_a_open_request =
      DidOpenRequest(module_a_uri, kSampleModuleA);
  ASSERT_OK(SendRequest(module_a_open_request));
  const std::string module_b_open_request =
      DidOpenRequest(module_b_uri, kSampleModuleB);
  ASSERT_OK(SendRequest(module_b_open_request));

  // Close a.sv from the Language Server perspective
  const std::string closing_request = json{
      //
      {"jsonrpc", "2.0"},
      {"method", "textDocument/didClose"},
      {"params",
       {{"textDocument",
         {
             {"uri", module_a_uri},
         }}}}}.dump();
  ASSERT_OK(SendRequest(closing_request));

  // obtain diagnostics for both files
  GetResponse();

  // find definition for "var1" variable of a module in b.sv file
  const std::string definition_request =
      DefinitionRequest(module_b_uri, 2, 4, 14);

  ASSERT_OK(SendRequest(definition_request));
  json response_b = json::parse(GetResponse());

  CheckDefinitionResponseSingleDefinition(
      response_b, 2, {.line = 1, .column = 9}, {.line = 1, .column = 13},
      module_a_uri);

  // perform double check
  ASSERT_OK(SendRequest(definition_request));
  response_b = json::parse(GetResponse());

  CheckDefinitionResponseSingleDefinition(
      response_b, 2, {.line = 1, .column = 9}, {.line = 1, .column = 13},
      module_a_uri);
}

// Check textDocument/definition request where we want definition of a symbol
// when there are incorrect files in the project
TEST_F(VerilogLanguageServerSymbolTableTest,
       DefinitionRequestInvalidFileInWorkspace) {
  absl::string_view filelist_content = "a.sv\nb.sv\n";

  static constexpr absl::string_view  //
      sample_module_b_with_error(
          R"(module b;
  assign var1 = 1'b0;
  assigne var2 = var1 | 1'b1;
  a vara;
  assign vara.var1 = 1'b1;
endmodule
)");

  const verible::file::testing::ScopedTestFile filelist(
      root_dir, filelist_content, "verible.filelist");
  const verible::file::testing::ScopedTestFile module_a(root_dir,
                                                        kSampleModuleA, "a.sv");
  const verible::file::testing::ScopedTestFile module_b(
      root_dir, sample_module_b_with_error, "b.sv");

  const std::string module_a_uri = PathToLSPUri(module_a.filename());
  const std::string module_b_uri = PathToLSPUri(module_b.filename());
  const std::string module_a_open_request =
      DidOpenRequest(module_a_uri, kSampleModuleA);
  ASSERT_OK(SendRequest(module_a_open_request));

  // obtain diagnostics for both files
  GetResponse();

  // find definition for "var1" variable in a.sv file
  const std::string definition_request =
      DefinitionRequest(module_a_uri, 2, 2, 16);

  ASSERT_OK(SendRequest(definition_request));
  json response = json::parse(GetResponse());

  CheckDefinitionResponseSingleDefinition(response, 2, {.line = 1, .column = 9},
                                          {.line = 1, .column = 13},
                                          module_a_uri);
}

// Check textDocument/definition request where we want definition of a symbol
// inside incorrect file
TEST_F(VerilogLanguageServerSymbolTableTest, DefinitionRequestInInvalidFile) {
  absl::string_view filelist_content = "a.sv\nb.sv\n";

  static constexpr absl::string_view  //
      sample_module_b_with_error(
          R"(module b;
  assign var1 = 1'b0;
  assigne var2 = var1 | 1'b1;
  a vara;
  assign vara.var1 = 1'b1;
endmodule
)");

  const verible::file::testing::ScopedTestFile filelist(
      root_dir, filelist_content, "verible.filelist");
  const verible::file::testing::ScopedTestFile module_a(root_dir,
                                                        kSampleModuleA, "a.sv");
  const verible::file::testing::ScopedTestFile module_b(
      root_dir, sample_module_b_with_error, "b.sv");

  const std::string module_a_uri = PathToLSPUri(module_a.filename());
  const std::string module_b_uri = PathToLSPUri(module_b.filename());
  const std::string module_a_open_request =
      DidOpenRequest(module_a_uri, kSampleModuleA);
  ASSERT_OK(SendRequest(module_a_open_request));

  // obtain diagnostics for both files
  GetResponse();

  // find definition for "var1" variable of a module in b.sv file
  const std::string definition_request =
      DefinitionRequest(module_b_uri, 2, 4, 15);

  ASSERT_OK(SendRequest(definition_request));
  json response_b = json::parse(GetResponse());

  // For now when the file is invalid we will not be able to obtain symbols
  // from it if it was incorrect from the start
  ASSERT_EQ(response_b["id"], 2);
  ASSERT_EQ(response_b["result"].size(), 0);
}

// Check textDocument/definition request when URI is not supported
TEST_F(VerilogLanguageServerSymbolTableTest, DefinitionRequestUnsupportedURI) {
  absl::string_view filelist_content = "a.sv";

  const verible::file::testing::ScopedTestFile filelist(
      root_dir, filelist_content, "verible.filelist");
  const verible::file::testing::ScopedTestFile module_a(root_dir,
                                                        kSampleModuleA, "a.sv");

  const std::string module_a_uri = PathToLSPUri(module_a.filename());
  const std::string module_a_open_request =
      DidOpenRequest(module_a_uri, kSampleModuleA);
  ASSERT_OK(SendRequest(module_a_open_request));

  // obtain diagnostics for both files
  GetResponse();

  // find definition for "var1" variable in a.sv file
  const std::string definition_request = DefinitionRequest(
      absl::StrReplaceAll(module_a_uri, {{"file://", "https://"}}), 2, 2, 16);

  ASSERT_OK(SendRequest(definition_request));
  json response = json::parse(GetResponse());

  ASSERT_EQ(response["id"], 2);
  ASSERT_EQ(response["result"].size(), 0);
}

// Check textDocument/definition when the cursor points at definition
TEST_F(VerilogLanguageServerSymbolTableTest,
       DefinitionRequestCursorAtDefinition) {
  absl::string_view filelist_content = "a.sv";

  const verible::file::testing::ScopedTestFile filelist(
      root_dir, filelist_content, "verible.filelist");
  const verible::file::testing::ScopedTestFile module_a(root_dir,
                                                        kSampleModuleA, "a.sv");

  const std::string module_a_uri = PathToLSPUri(module_a.filename());
  const std::string module_a_open_request =
      DidOpenRequest(module_a_uri, kSampleModuleA);
  ASSERT_OK(SendRequest(module_a_open_request));

  // obtain diagnostics for both files
  GetResponse();

  // find definition for "var1" variable in a.sv file
  const std::string definition_request =
      DefinitionRequest(module_a_uri, 2, 1, 10);

  ASSERT_OK(SendRequest(definition_request));
  json response = json::parse(GetResponse());

  CheckDefinitionResponseSingleDefinition(response, 2, {.line = 1, .column = 9},
                                          {.line = 1, .column = 13},
                                          module_a_uri);
}

// Check textDocument/definition when the cursor points at nothing
TEST_F(VerilogLanguageServerSymbolTableTest,
       DefinitionRequestCursorAtNoSymbol) {
  absl::string_view filelist_content = "a.sv";

  const verible::file::testing::ScopedTestFile filelist(
      root_dir, filelist_content, "verible.filelist");
  const verible::file::testing::ScopedTestFile module_a(root_dir,
                                                        kSampleModuleA, "a.sv");

  const std::string module_a_uri = PathToLSPUri(module_a.filename());
  const std::string module_a_open_request =
      DidOpenRequest(module_a_uri, kSampleModuleA);
  ASSERT_OK(SendRequest(module_a_open_request));

  // obtain diagnostics for both files
  GetResponse();

  // find definition for "var1" variable in a.sv file
  const std::string definition_request =
      DefinitionRequest(module_a_uri, 2, 1, 0);

  ASSERT_OK(SendRequest(definition_request));
  json response = json::parse(GetResponse());

  ASSERT_EQ(response["id"], 2);
  ASSERT_EQ(response["result"].size(), 0);
}

// Check textDocument/definition when the cursor points at nothing
TEST_F(VerilogLanguageServerSymbolTableTest,
       DefinitionRequestCursorAtUnknownSymbol) {
  absl::string_view filelist_content = "b.sv";

  const verible::file::testing::ScopedTestFile filelist(
      root_dir, filelist_content, "verible.filelist");
  const verible::file::testing::ScopedTestFile module_b(root_dir,
                                                        kSampleModuleB, "b.sv");

  const std::string module_b_uri = PathToLSPUri(module_b.filename());
  const std::string module_b_open_request =
      DidOpenRequest(module_b_uri, kSampleModuleB);
  ASSERT_OK(SendRequest(module_b_open_request));

  // obtain diagnostics for both files
  GetResponse();

  // find definition for "var1" variable in a.sv file
  const std::string definition_request =
      DefinitionRequest(module_b_uri, 2, 3, 2);

  ASSERT_OK(SendRequest(definition_request));
  json response = json::parse(GetResponse());

  ASSERT_EQ(response["id"], 2);
  ASSERT_EQ(response["result"].size(), 0);
}

// Performs simple textDocument/definition request when no verible.filelist
// file is provided in the workspace
TEST_F(VerilogLanguageServerSymbolTableTest, DefinitionRequestNoFileList) {
  const verible::file::testing::ScopedTestFile module_a(root_dir,
                                                        kSampleModuleA, "a.sv");

  const std::string module_a_uri = PathToLSPUri(module_a.filename());
  const std::string module_a_open_request =
      DidOpenRequest(module_a_uri, kSampleModuleA);
  ASSERT_OK(SendRequest(module_a_open_request));

  // obtain diagnostics
  GetResponse();

  // find definition for "var1" variable in a.sv file
  const std::string definition_request =
      DefinitionRequest(module_a_uri, 2, 2, 16);

  ASSERT_OK(SendRequest(definition_request));
  json response = json::parse(GetResponse());

  ASSERT_EQ(response["result"].size(), 1);
  ASSERT_EQ(response["result"][0]["uri"], module_a_uri);
}

// Check textDocument/definition request where we want definition of a symbol
// inside other module edited in buffer without a filelist
TEST_F(VerilogLanguageServerSymbolTableTest,
       DefinitionRequestSymbolFromDifferentOpenedModuleNoFileList) {
  const verible::file::testing::ScopedTestFile module_a(root_dir,
                                                        kSampleModuleA, "a.sv");
  const verible::file::testing::ScopedTestFile module_b(root_dir,
                                                        kSampleModuleB, "b.sv");

  const std::string module_a_uri = PathToLSPUri(module_a.filename());
  const std::string module_b_uri = PathToLSPUri(module_b.filename());

  const std::string module_a_open_request =
      DidOpenRequest(module_a_uri, kSampleModuleA);
  ASSERT_OK(SendRequest(module_a_open_request));
  const std::string module_b_open_request =
      DidOpenRequest(module_b_uri, kSampleModuleB);
  ASSERT_OK(SendRequest(module_b_open_request));

  // obtain diagnostics for both files
  GetResponse();

  // find definition for "var1" variable in b.sv file
  const std::string definition_request =
      DefinitionRequest(module_b_uri, 2, 4, 14);

  ASSERT_OK(SendRequest(definition_request));
  json response_b = json::parse(GetResponse());

  CheckDefinitionResponseSingleDefinition(
      response_b, 2, {.line = 1, .column = 9}, {.line = 1, .column = 13},
      module_a_uri);
}

TEST_F(VerilogLanguageServerSymbolTableTest, MultipleDefinitionsOfSameSymbol) {
  static constexpr absl::string_view filelist_content =
      "bar_1.sv\nbar_2.sv\nfoo.sv";

  static constexpr absl::string_view  //
      bar_1(
          R"(module bar();
endmodule
)");
  static constexpr absl::string_view  //
      bar_2(
          R"(module bar();
endmodule
)");
  static constexpr absl::string_view  //
      foo(
          R"(module foo();
  bar x;
endmodule
)");

  const verible::file::testing::ScopedTestFile filelist(
      root_dir, filelist_content, "verible.filelist");
  const verible::file::testing::ScopedTestFile module_bar_1(root_dir, bar_1,
                                                            "bar_1.sv");
  const verible::file::testing::ScopedTestFile module_bar_2(root_dir, bar_2,
                                                            "bar_2.sv");
  const verible::file::testing::ScopedTestFile module_foo(root_dir, foo,
                                                          "foo.sv");

  const std::string module_foo_uri = PathToLSPUri(module_foo.filename());
  const std::string module_bar_1_uri = PathToLSPUri(module_bar_1.filename());

  const std::string foo_open_request = DidOpenRequest(module_foo_uri, foo);
  ASSERT_OK(SendRequest(foo_open_request));

  GetResponse();

  // find definition for "bar" type
  const std::string definition_request =
      DefinitionRequest(module_foo_uri, 2, 1, 3);

  ASSERT_OK(SendRequest(definition_request));
  json response = json::parse(GetResponse());

  CheckDefinitionResponseSingleDefinition(response, 2, {.line = 0, .column = 7},
                                          {.line = 0, .column = 10},
                                          module_bar_1_uri);
}

// Sample of badly styled module
constexpr static absl::string_view badly_styled_module =
    "module my_module(input logic in, output logic out);\n\tassign out = in; "
    "\nendmodule";

// Checks if a given substring (lint rule type) is present
// in linter diagnostics.
bool CheckDiagnosticsContainLinterIssue(const json &diagnostics,
                                        absl::string_view lint_issue_type) {
  for (const auto &d : diagnostics) {
    if (absl::StrContains(d["message"].get<std::string>(), lint_issue_type)) {
      return true;
    }
  }
  return false;
}

// Performs default run of the linter, without configuration file.
TEST_F(VerilogLanguageServerSymbolTableTest, DefaultConfigurationTest) {
  const verible::file::testing::ScopedTestFile module_mod(
      root_dir, badly_styled_module, "my_mod.sv");

  const std::string mod_open_request =
      DidOpenRequest(PathToLSPUri(module_mod.filename()), badly_styled_module);

  ASSERT_OK(SendRequest(mod_open_request));

  const json diagnostics = json::parse(GetResponse());

  ASSERT_EQ(diagnostics["method"], "textDocument/publishDiagnostics");
  ASSERT_GT(diagnostics["params"]["diagnostics"].size(), 0);

  ASSERT_TRUE(CheckDiagnosticsContainLinterIssue(
      diagnostics["params"]["diagnostics"], "module-filename"));
  ASSERT_TRUE(CheckDiagnosticsContainLinterIssue(
      diagnostics["params"]["diagnostics"], "no-tabs"));
  ASSERT_TRUE(CheckDiagnosticsContainLinterIssue(
      diagnostics["params"]["diagnostics"], "no-trailing-spaces"));
  ASSERT_TRUE(CheckDiagnosticsContainLinterIssue(
      diagnostics["params"]["diagnostics"], "posix-eof"));
}

// Checks the work of linter in language server when
// configuration file with "-no-tabs" file is present
TEST_F(VerilogLanguageServerSymbolTableTest, ParsingLinterNoTabs) {
  constexpr absl::string_view lint_config = "-no-tabs";
  const verible::file::testing::ScopedTestFile module_mod(
      root_dir, badly_styled_module, "my_mod.sv");
  const verible::file::testing::ScopedTestFile lint_file(root_dir, lint_config,
                                                         ".rules.verible_lint");
  const std::string mod_open_request =
      DidOpenRequest(PathToLSPUri(module_mod.filename()), badly_styled_module);

  ASSERT_OK(SendRequest(mod_open_request));

  const json diagnostics = json::parse(GetResponse());

  ASSERT_EQ(diagnostics["method"], "textDocument/publishDiagnostics");
  ASSERT_GT(diagnostics["params"]["diagnostics"].size(), 0);

  ASSERT_TRUE(CheckDiagnosticsContainLinterIssue(
      diagnostics["params"]["diagnostics"], "module-filename"));
  ASSERT_FALSE(CheckDiagnosticsContainLinterIssue(
      diagnostics["params"]["diagnostics"], "no-tabs"));
  ASSERT_TRUE(CheckDiagnosticsContainLinterIssue(
      diagnostics["params"]["diagnostics"], "no-trailing-spaces"));
  ASSERT_TRUE(CheckDiagnosticsContainLinterIssue(
      diagnostics["params"]["diagnostics"], "posix-eof"));
}

// Performs another check on linter configuration with more disabled
// rules
TEST_F(VerilogLanguageServerSymbolTableTest,
       ParsingLinterNoTabsIgnoreModuleName) {
  constexpr absl::string_view lint_config =
      "-module-filename\n-posix-eof\n-no-tabs";
  const verible::file::testing::ScopedTestFile module_mod(
      root_dir, badly_styled_module, "my_mod.sv");
  const verible::file::testing::ScopedTestFile lint_file(root_dir, lint_config,
                                                         ".rules.verible_lint");
  const std::string mod_open_request =
      DidOpenRequest(PathToLSPUri(module_mod.filename()), badly_styled_module);

  ASSERT_OK(SendRequest(mod_open_request));

  const json diagnostics = json::parse(GetResponse());

  ASSERT_EQ(diagnostics["method"], "textDocument/publishDiagnostics");
  ASSERT_GT(diagnostics["params"]["diagnostics"].size(), 0);

  ASSERT_FALSE(CheckDiagnosticsContainLinterIssue(
      diagnostics["params"]["diagnostics"], "module-filename"));
  ASSERT_FALSE(CheckDiagnosticsContainLinterIssue(
      diagnostics["params"]["diagnostics"], "no-tabs"));
  ASSERT_TRUE(CheckDiagnosticsContainLinterIssue(
      diagnostics["params"]["diagnostics"], "no-trailing-spaces"));
  ASSERT_FALSE(CheckDiagnosticsContainLinterIssue(
      diagnostics["params"]["diagnostics"], "posix-eof"));
}

// compares references returned by the VerilogLanguageServer with the list
// of references from exemplar
void CheckReferenceResults(json results, json exemplar) {
  ASSERT_EQ(results.size(), exemplar.size());
  std::sort(results.begin(), results.end(),
            [](const json &a, const json &b) { return a.dump() < b.dump(); });
  std::sort(exemplar.begin(), exemplar.end(),
            [](const json &a, const json &b) { return a.dump() < b.dump(); });
  ASSERT_EQ(results, exemplar);
}

// Creates single reference entry for comparison purposes
json ReferenceEntry(verible::LineColumn start, verible::LineColumn end,
                    const std::string &uri) {
  return {{"range",
           {{"end", {{"character", end.column}, {"line", end.line}}},
            {"start", {{"character", start.column}, {"line", start.line}}}}},
          {"uri", uri}};
}

// Check textDocument/references request when there are two symbols of the same
// name (variable name) in two modules
TEST_F(VerilogLanguageServerSymbolTableTest,
       ReferencesRequestSameVariablesDifferentModules) {
  absl::string_view filelist_content = "a.sv\nb.sv\n";

  const verible::file::testing::ScopedTestFile filelist(
      root_dir, filelist_content, "verible.filelist");
  const verible::file::testing::ScopedTestFile module_a(root_dir,
                                                        kSampleModuleA, "a.sv");
  const verible::file::testing::ScopedTestFile module_b(root_dir,
                                                        kSampleModuleB, "b.sv");

  const std::string module_a_uri = PathToLSPUri(module_a.filename());
  const std::string module_b_uri = PathToLSPUri(module_b.filename());

  const std::string module_a_open_request =
      DidOpenRequest(module_a_uri, kSampleModuleA);
  ASSERT_OK(SendRequest(module_a_open_request));
  const std::string module_b_open_request =
      DidOpenRequest(module_b_uri, kSampleModuleB);
  ASSERT_OK(SendRequest(module_b_open_request));

  // obtain diagnostics for both files
  GetResponse();

  // find references for "var1" variable in a.sv file
  std::string references_request = ReferencesRequest(module_a_uri, 2, 1, 11);

  ASSERT_OK(SendRequest(references_request));
  json response_a = json::parse(GetResponse());

  ASSERT_EQ(response_a["id"], 2);

  json var1_a_refs = {ReferenceEntry(
                          {
                              .line = 2,
                              .column = 16,
                          },
                          {.line = 2, .column = 20}, module_a_uri),
                      ReferenceEntry(
                          {
                              .line = 1,
                              .column = 9,
                          },
                          {.line = 1, .column = 13}, module_a_uri),
                      ReferenceEntry(
                          {
                              .line = 4,
                              .column = 14,
                          },
                          {.line = 4, .column = 18}, module_b_uri)};

  CheckReferenceResults(response_a["result"], var1_a_refs);

  // find references for "var1" variable in b.sv file
  references_request = ReferencesRequest(module_b_uri, 3, 2, 18);

  ASSERT_OK(SendRequest(references_request));
  json response_b = json::parse(GetResponse());

  ASSERT_EQ(response_b["id"], 3);

  json var1_b_refs = {
      ReferenceEntry(
          {
              .line = 1,
              .column = 9,
          },
          {.line = 1, .column = 13}, module_b_uri),
      ReferenceEntry(
          {
              .line = 2,
              .column = 16,
          },
          {.line = 2, .column = 20}, module_b_uri),
  };

  CheckReferenceResults(response_b["result"], var1_b_refs);
}

// Check textDocument/references behavior when pointing to an invalid space
TEST_F(VerilogLanguageServerSymbolTableTest, CheckReferenceInvalidLocation) {
  absl::string_view filelist_content = "a.sv\n";

  const verible::file::testing::ScopedTestFile filelist(
      root_dir, filelist_content, "verible.filelist");
  const verible::file::testing::ScopedTestFile module_a(root_dir,
                                                        kSampleModuleA, "a.sv");

  const std::string module_a_uri = PathToLSPUri(module_a.filename());

  const std::string module_a_open_request =
      DidOpenRequest(module_a_uri, kSampleModuleA);
  ASSERT_OK(SendRequest(module_a_open_request));

  // obtain diagnostics for both files
  GetResponse();

  // find references for "var1" variable in a.sv file
  std::string references_request = ReferencesRequest(module_a_uri, 2, 1, 0);

  ASSERT_OK(SendRequest(references_request));
  json response_a = json::parse(GetResponse());

  ASSERT_EQ(response_a["id"], 2);
  ASSERT_EQ(response_a["result"].size(), 0);
}

// Check textDocument/references behavior when pointing to a keyword
TEST_F(VerilogLanguageServerSymbolTableTest, CheckReferenceKeyword) {
  absl::string_view filelist_content = "a.sv\n";

  const verible::file::testing::ScopedTestFile filelist(
      root_dir, filelist_content, "verible.filelist");
  const verible::file::testing::ScopedTestFile module_a(root_dir,
                                                        kSampleModuleA, "a.sv");

  const std::string module_a_uri = PathToLSPUri(module_a.filename());

  const std::string module_a_open_request =
      DidOpenRequest(module_a_uri, kSampleModuleA);
  ASSERT_OK(SendRequest(module_a_open_request));

  // obtain diagnostics for both files
  GetResponse();

  // find references for "var1" variable in a.sv file
  std::string references_request = ReferencesRequest(module_a_uri, 2, 1, 5);

  ASSERT_OK(SendRequest(references_request));
  json response_a = json::parse(GetResponse());

  ASSERT_EQ(response_a["id"], 2);
  ASSERT_EQ(response_a["result"].size(), 0);
}

// Check textDocument/references behavior when pointing to an unknown symbol
TEST_F(VerilogLanguageServerSymbolTableTest, CheckReferenceUnknownSymbol) {
  absl::string_view filelist_content = "b.sv\n";

  const verible::file::testing::ScopedTestFile filelist(
      root_dir, filelist_content, "verible.filelist");
  const verible::file::testing::ScopedTestFile module_b(root_dir,
                                                        kSampleModuleB, "b.sv");

  const std::string module_b_uri = PathToLSPUri(module_b.filename());

  const std::string module_b_open_request =
      DidOpenRequest(module_b_uri, kSampleModuleB);
  ASSERT_OK(SendRequest(module_b_open_request));

  // obtain diagnostics for both files
  GetResponse();

  // find references for "var1" variable in a.sv file
  std::string references_request = ReferencesRequest(module_b_uri, 2, 4, 16);

  ASSERT_OK(SendRequest(references_request));
  json response_b = json::parse(GetResponse());

  ASSERT_EQ(response_b["id"], 2);
  ASSERT_EQ(response_b["result"].size(), 0);
}

// Checks the definition request for module type in different module
TEST_F(VerilogLanguageServerSymbolTableTest, DefinitionRequestModule) {
  static constexpr absl::string_view  //
      instmodule(
          R"(module InstModule (
    o,
    i
);
  output [31:0] o;
  input i;
  wire [31:0] o = {32{i}};
endmodule

module ExampInst (
    o,
    i
);

  output o;
  input i;

  InstModule instName (  /*AUTOINST*/);

endmodule
)");
  const verible::file::testing::ScopedTestFile module_instmodule(
      root_dir, instmodule, "instmodule.sv");

  const std::string module_instmodule_uri =
      PathToLSPUri(module_instmodule.filename());
  const std::string foo_open_request =
      DidOpenRequest(module_instmodule_uri, instmodule);
  ASSERT_OK(SendRequest(foo_open_request));

  GetResponse();

  // find definition for "InstModule"
  const std::string definition_request =
      DefinitionRequest(module_instmodule_uri, 2, 17, 3);

  ASSERT_OK(SendRequest(definition_request));
  json response = json::parse(GetResponse());

  CheckDefinitionResponseSingleDefinition(response, 2, {.line = 0, .column = 7},
                                          {.line = 0, .column = 17},
                                          module_instmodule_uri);
}

// Checks the go-to definition when pointing to the definition of the symbol
TEST_F(VerilogLanguageServerSymbolTableTest, DefinitionRequestSelf) {
  static constexpr absl::string_view  //
      instmodule(
          R"(module InstModule (
    o,
    i
);
  output [31:0] o;
  input i;
  wire [31:0] o = {32{i}};
endmodule

module ExampInst (
    o,
    i
);

  output o;
  input i;

  InstModule instName (  /*AUTOINST*/);

endmodule
)");
  const verible::file::testing::ScopedTestFile module_instmodule(
      root_dir, instmodule, "instmodule.sv");

  const std::string module_instmodule_uri =
      PathToLSPUri(module_instmodule.filename());
  const std::string foo_open_request =
      DidOpenRequest(module_instmodule_uri, instmodule);
  ASSERT_OK(SendRequest(foo_open_request));

  GetResponse();

  // find definition for "InstModule"
  const std::string definition_request =
      DefinitionRequest(module_instmodule_uri, 2, 0, 8);

  ASSERT_OK(SendRequest(definition_request));
  json response = json::parse(GetResponse());

  CheckDefinitionResponseSingleDefinition(response, 2, {.line = 0, .column = 7},
                                          {.line = 0, .column = 17},
                                          module_instmodule_uri);
}

// Checks the definition request for module port
// This check verifies ports with types defined inside port list
TEST_F(VerilogLanguageServerSymbolTableTest,
       DefinitionRequestPortTypesInsideList) {
  static constexpr absl::string_view  //
      instmodule(
          R"(module InstModule (
    output logic [31:0] o,
    input logic i
);
  wire [31:0] o = {32{i}};
endmodule
)");
  const verible::file::testing::ScopedTestFile module_instmodule(
      root_dir, instmodule, "instmodule.sv");

  const std::string module_instmodule_uri =
      PathToLSPUri(module_instmodule.filename());
  const std::string foo_open_request =
      DidOpenRequest(module_instmodule_uri, instmodule);
  ASSERT_OK(SendRequest(foo_open_request));

  GetResponse();

  // find definition for "i"
  std::string definition_request =
      DefinitionRequest(module_instmodule_uri, 2, 4, 22);
  ASSERT_OK(SendRequest(definition_request));
  json response = json::parse(GetResponse());
  CheckDefinitionResponseSingleDefinition(
      response, 2, {.line = 2, .column = 16}, {.line = 2, .column = 17},
      module_instmodule_uri);
}

// Checks the definition request for module port
// This check verifies ports with types defined outside port list
TEST_F(VerilogLanguageServerSymbolTableTest,
       DefinitionRequestPortTypesOutsideList) {
  static constexpr absl::string_view  //
      instmodule(
          R"(module InstModule (
    o,
    i
);
  output logic [31:0] o;
  input logic i;
  wire [31:0] o = {32{i}};
endmodule
)");
  const verible::file::testing::ScopedTestFile module_instmodule(
      root_dir, instmodule, "instmodule.sv");

  const std::string module_instmodule_uri =
      PathToLSPUri(module_instmodule.filename());
  const std::string foo_open_request =
      DidOpenRequest(module_instmodule_uri, instmodule);
  ASSERT_OK(SendRequest(foo_open_request));

  GetResponse();

  // find definition for "bar" type
  const std::string definition_request =
      DefinitionRequest(module_instmodule_uri, 2, 6, 22);

  ASSERT_OK(SendRequest(definition_request));
  json response = json::parse(GetResponse());
  CheckDefinitionResponseSingleDefinition(
      response, 2, {.line = 5, .column = 14}, {.line = 5, .column = 15},
      module_instmodule_uri);
}

// Checks jumps to different variants of kModulePortDeclaration
// * port with implicit type
// * kPortIdentifier (reg with assignment)
// * port with dimensions
// * simple port
TEST_F(VerilogLanguageServerSymbolTableTest,
       DefinitionRequestPortPortIdentifierVariant) {
  static constexpr absl::string_view  //
      port_identifier(
          R"(module port_identifier(a, rst, clk, out);
    input logic [15:0] a;
    input rst;
    input logic clk;
    output reg [15:0] out = 0;

    always @(posedge clk) begin
        if (! rst) begin
            out = 0;
        end
        else begin
            out = out + a;
        end
    end
endmodule)");
  const verible::file::testing::ScopedTestFile module_port_identifier(
      root_dir, port_identifier, "port_identifier.sv");

  const std::string module_port_identifier_uri =
      PathToLSPUri(module_port_identifier.filename());
  const std::string foo_open_request =
      DidOpenRequest(module_port_identifier_uri, port_identifier);
  ASSERT_OK(SendRequest(foo_open_request));

  GetResponse();

  // find definition for "a"
  std::string definition_request =
      DefinitionRequest(module_port_identifier_uri, 2, 11, 24);
  ASSERT_OK(SendRequest(definition_request));
  json response = json::parse(GetResponse());
  CheckDefinitionResponseSingleDefinition(
      response, 2, {.line = 1, .column = 23}, {.line = 1, .column = 24},
      module_port_identifier_uri);

  // find definition for "clk"
  definition_request = DefinitionRequest(module_port_identifier_uri, 3, 6, 22);
  ASSERT_OK(SendRequest(definition_request));
  response = json::parse(GetResponse());
  CheckDefinitionResponseSingleDefinition(
      response, 3, {.line = 3, .column = 16}, {.line = 3, .column = 19},
      module_port_identifier_uri);

  // find definition for "rst"
  definition_request = DefinitionRequest(module_port_identifier_uri, 4, 6, 22);
  ASSERT_OK(SendRequest(definition_request));
  response = json::parse(GetResponse());
  CheckDefinitionResponseSingleDefinition(
      response, 4, {.line = 3, .column = 16}, {.line = 3, .column = 19},
      module_port_identifier_uri);

  // find first definition for "out"
  definition_request = DefinitionRequest(module_port_identifier_uri, 5, 8, 13);
  ASSERT_OK(SendRequest(definition_request));
  response = json::parse(GetResponse());
  CheckDefinitionResponseSingleDefinition(
      response, 5, {.line = 4, .column = 22}, {.line = 4, .column = 25},
      module_port_identifier_uri);

  // find second definition for "out"
  definition_request = DefinitionRequest(module_port_identifier_uri, 6, 11, 18);
  ASSERT_OK(SendRequest(definition_request));
  response = json::parse(GetResponse());
  CheckDefinitionResponseSingleDefinition(
      response, 6, {.line = 4, .column = 22}, {.line = 4, .column = 25},
      module_port_identifier_uri);
}

// Verifies the work of the go-to definition request when the
// definition of the symbol is split into multiple lines,
// e.g. for port module declarations.
TEST_F(VerilogLanguageServerSymbolTableTest, MultilinePortDefinitions) {
  static constexpr absl::string_view  //
      port_identifier(
          R"(module port_identifier(i, o, trigger);
  input trigger;
  input i;
  output o;

  reg [31:0] i;
  wire [31:0] o;

  always @(posedge clock)
    assign o = i;
endmodule
)");
  const verible::file::testing::ScopedTestFile module_port_identifier(
      root_dir, port_identifier, "port_identifier.sv");

  const std::string module_port_identifier_uri =
      PathToLSPUri(module_port_identifier.filename());
  const std::string foo_open_request =
      DidOpenRequest(module_port_identifier_uri, port_identifier);
  ASSERT_OK(SendRequest(foo_open_request));

  json diagnostics = json::parse(GetResponse());
  ASSERT_EQ(diagnostics["method"], "textDocument/publishDiagnostics");
  ASSERT_EQ(diagnostics["params"]["uri"], module_port_identifier_uri);
  ASSERT_EQ(diagnostics["params"]["diagnostics"].size(), 0);

  // find definition for "i"
  std::string definition_request =
      DefinitionRequest(module_port_identifier_uri, 2, 9, 15);
  ASSERT_OK(SendRequest(definition_request));
  json response = json::parse(GetResponse());

  ASSERT_EQ(response["id"], 2);
  ASSERT_EQ(response["result"].size(), 2);

  std::sort(
      response["result"].begin(), response["result"].end(),
      [](const json &a, const json &b) -> bool { return a.dump() < b.dump(); });

  CheckDefinitionEntry(response["result"][0], {.line = 5, .column = 13},
                       {.line = 5, .column = 14}, module_port_identifier_uri);
  CheckDefinitionEntry(response["result"][1], {.line = 2, .column = 8},
                       {.line = 2, .column = 9}, module_port_identifier_uri);
}

// Verifies the work of the go-to definition request when
// definition of the symbol later in the definition list is requested
TEST_F(VerilogLanguageServerSymbolTableTest, MultilinePortDefinitionsWithList) {
  static constexpr absl::string_view  //
      port_identifier(
          R"(module port_identifier(a, b, o, trigger);
  input trigger;
  input a, b;
  output o;

  reg [31:0] a, b;
  wire [31:0] o;

  always @(posedge clock)
    assign o = a + b;
endmodule
)");
  const verible::file::testing::ScopedTestFile module_port_identifier(
      root_dir, port_identifier, "port_identifier.sv");

  const std::string module_port_identifier_uri =
      PathToLSPUri(module_port_identifier.filename());
  const std::string foo_open_request =
      DidOpenRequest(module_port_identifier_uri, port_identifier);
  ASSERT_OK(SendRequest(foo_open_request));

  json diagnostics = json::parse(GetResponse());
  ASSERT_EQ(diagnostics["method"], "textDocument/publishDiagnostics");
  ASSERT_EQ(diagnostics["params"]["uri"], module_port_identifier_uri);
  ASSERT_EQ(diagnostics["params"]["diagnostics"].size(), 0);

  // find definition for "i"
  std::string definition_request =
      DefinitionRequest(module_port_identifier_uri, 2, 5, 16);
  ASSERT_OK(SendRequest(definition_request));
  json response = json::parse(GetResponse());

  ASSERT_EQ(response["id"], 2);
  ASSERT_EQ(response["result"].size(), 2);

  std::sort(
      response["result"].begin(), response["result"].end(),
      [](const json &a, const json &b) -> bool { return a.dump() < b.dump(); });

  CheckDefinitionEntry(response["result"][0], {.line = 2, .column = 11},
                       {.line = 2, .column = 12}, module_port_identifier_uri);
  CheckDefinitionEntry(response["result"][1], {.line = 5, .column = 16},
                       {.line = 5, .column = 17}, module_port_identifier_uri);
}

std::string RenameRequest(const verible::lsp::RenameParams &params) {
  json request = {{"jsonrpc", "2.0"},
                  {"id", 2},
                  {"method", "textDocument/rename"},
                  {"params", params}};
  return request.dump();
}
std::string PrepareRenameRequest(
    const verible::lsp::PrepareRenameParams &params) {
  json request = {{"jsonrpc", "2.0"},
                  {"id", 2},
                  {"method", "textDocument/prepareRename"},
                  {"params", params}};
  return request.dump();
}
// Runs tests for textDocument/rangeFormatting requests
TEST_F(VerilogLanguageServerSymbolTableTest,
       PrepareRenameReturnsRangeOfEditableSymbol) {
  // Create sample file and make sure diagnostics do not have errors
  std::string file_uri = PathToLSPUri(absl::string_view(root_dir + "/fmt.sv"));
  verible::lsp::PrepareRenameParams params;
  params.position.line = 2;
  params.position.character = 1;
  params.textDocument.uri = file_uri;

  const std::string mini_module =
      DidOpenRequest(file_uri,
                     "module fmt();\nfunction automatic "
                     "bar();\nbar();\nbar();\nendfunction;\nendmodule\n");
  ASSERT_OK(SendRequest(mini_module));

  const json diagnostics = json::parse(GetResponse());
  EXPECT_EQ(diagnostics["method"], "textDocument/publishDiagnostics")
      << "textDocument/publishDiagnostics not received";
  EXPECT_EQ(diagnostics["params"]["uri"], file_uri)
      << "Diagnostics for invalid file";

  EXPECT_EQ(diagnostics["params"]["diagnostics"].size(), 0)
      << "The test file has errors";
  ASSERT_OK(SendRequest(PrepareRenameRequest(params)));

  const json response = json::parse(GetResponse());
  EXPECT_EQ(response["result"]["start"]["line"], 2)
      << "Invalid result for id:  ";
  EXPECT_EQ(response["result"]["start"]["character"], 0)
      << "Invalid result for id:  ";
  EXPECT_EQ(response["result"]["end"]["line"], 2) << "Invalid result for id:  ";
  EXPECT_EQ(response["result"]["end"]["character"], 3)
      << "Invalid result for id:  ";
}

TEST_F(VerilogLanguageServerSymbolTableTest, PrepareRenameReturnsNull) {
  // Create sample file and make sure diagnostics do not have errors
  std::string file_uri = PathToLSPUri(absl::string_view(root_dir + "/fmt.sv"));
  verible::lsp::PrepareRenameParams params;
  params.position.line = 1;
  params.position.character = 1;
  params.textDocument.uri = file_uri;

  const std::string mini_module =
      DidOpenRequest(file_uri,
                     "module fmt();\nfunction automatic "
                     "bar();\nbar();\nbar();\nendfunction;\nendmodule\n");
  ASSERT_OK(SendRequest(mini_module));

  const json diagnostics = json::parse(GetResponse());
  EXPECT_EQ(diagnostics["method"], "textDocument/publishDiagnostics")
      << "textDocument/publishDiagnostics not received";
  EXPECT_EQ(diagnostics["params"]["uri"], file_uri)
      << "Diagnostics for invalid file";

  EXPECT_EQ(diagnostics["params"]["diagnostics"].size(), 0)
      << "The test file has errors";
  ASSERT_OK(SendRequest(PrepareRenameRequest(params)));

  const json response = json::parse(GetResponse());
  EXPECT_EQ(response["result"], nullptr) << "Invalid result for id:  ";
}

TEST_F(VerilogLanguageServerSymbolTableTest, RenameTestSymbolSingleFile) {
  // Create sample file and make sure diagnostics do not have errors
  std::string file_uri =
      PathToLSPUri(absl::string_view(root_dir + "/rename.sv"));
  verible::lsp::RenameParams params;
  params.position.line = 2;
  params.position.character = 1;
  params.textDocument.uri = file_uri;
  params.newName = "foo";

  absl::string_view filelist_content = "rename.sv\n";

  const verible::file::testing::ScopedTestFile filelist(
      root_dir, filelist_content, "verible.filelist");
  const verible::file::testing::ScopedTestFile module_foo(
      root_dir,
      "module rename();\nfunction automatic "
      "bar();\nbar();\nbar();\nendfunction;\nendmodule\n",
      "rename.sv");

  const std::string mini_module =
      DidOpenRequest(file_uri,
                     "module rename();\nfunction automatic "
                     "bar();\nbar();\nbar();\nendfunction;\nendmodule\n");

  ASSERT_OK(SendRequest(mini_module));

  const json diagnostics = json::parse(GetResponse());
  EXPECT_EQ(diagnostics["method"], "textDocument/publishDiagnostics")
      << "textDocument/publishDiagnostics not received";

  EXPECT_EQ(diagnostics["params"]["uri"],
            PathToLSPUri(verible::lsp::LSPUriToPath(file_uri)))
      << "Diagnostics for invalid file";
  EXPECT_EQ(diagnostics["params"]["diagnostics"].size(), 0)
      << "The test file has errors";
  std::string request = RenameRequest(params);
  ASSERT_OK(SendRequest(request));

  const json response = json::parse(GetResponse());
  EXPECT_EQ(response["result"]["changes"].size(), 1)
      << "Invalid result size for id:  ";
  EXPECT_EQ(response["result"]["changes"][file_uri].size(), 3)
      << "Invalid result size for id:  ";
}

TEST_F(VerilogLanguageServerSymbolTableTest, RenameTestSymbolMultipleFiles) {
  // Create sample file and make sure diagnostics do not have errors
  std::string top_uri = PathToLSPUri(absl::string_view(root_dir + "/top.sv"));
  std::string foo_uri = PathToLSPUri(absl::string_view(root_dir + "/foo.sv"));
  verible::lsp::RenameParams params;
  params.position.line = 2;
  params.position.character = 9;
  params.textDocument.uri = top_uri;
  params.newName = "foobaz";
  std::string foosv =
      "package foo;\n"
      "    class foobar;\n"
      "    endclass;\n"
      "endpackage;\n";
  std::string topsv =
      "import foo::*;\n"
      "module top;\n"
      "  foo::foobar bar;\n"
      "endmodule;\n";
  absl::string_view filelist_content = "./foo.sv\n./top.sv\n";

  const verible::file::testing::ScopedTestFile filelist(
      root_dir, filelist_content, "verible.filelist");
  const verible::file::testing::ScopedTestFile module_foo(root_dir, foosv,
                                                          "foo.sv");

  const verible::file::testing::ScopedTestFile module_top(root_dir, topsv,
                                                          "top.sv");
  const std::string top_request = DidOpenRequest(top_uri, topsv);
  ASSERT_OK(SendRequest(top_request));

  const json diagnostics = json::parse(GetResponse());
  EXPECT_EQ(diagnostics["method"], "textDocument/publishDiagnostics")
      << "textDocument/publishDiagnostics not received";
  EXPECT_EQ(diagnostics["params"]["uri"],
            PathToLSPUri(verible::lsp::LSPUriToPath(top_uri)))
      << "Diagnostics for invalid file";

  const std::string foo_request = DidOpenRequest(foo_uri, foosv);
  ASSERT_OK(SendRequest(foo_request));

  const json diagnostics_foo = json::parse(GetResponse());
  EXPECT_EQ(diagnostics_foo["method"], "textDocument/publishDiagnostics")
      << "textDocument/publishDiagnostics not received";
  EXPECT_EQ(diagnostics_foo["params"]["uri"],
            PathToLSPUri(verible::lsp::LSPUriToPath(foo_uri)))
      << "Diagnostics for invalid file";

  // Complaints about package and file names
  EXPECT_EQ(diagnostics["params"]["diagnostics"].size(), 0)
      << "The test file has errors";
  std::string request = RenameRequest(params);
  ASSERT_OK(SendRequest(request));

  const json response = json::parse(GetResponse());
  EXPECT_EQ(response["result"]["changes"].size(), 2)
      << "Invalid result size for id:  ";
  EXPECT_EQ(response["result"]["changes"][top_uri].size(), 1)
      << "Invalid result size for id:  ";
  EXPECT_EQ(response["result"]["changes"][foo_uri].size(), 1)
      << "Invalid result size for id:  ";
}

TEST_F(VerilogLanguageServerSymbolTableTest, RenameTestPackageDistinction) {
  // Create sample file and make sure diagnostics do not have errors
  std::string file_uri =
      PathToLSPUri(absl::string_view(root_dir + "/rename.sv"));
  verible::lsp::RenameParams params;
  params.position.line = 7;
  params.position.character = 15;
  params.textDocument.uri = file_uri;
  params.newName = "foobaz";
  std::string renamesv =
      "package foo;\n"
      "    class foobar;\n"
      "        bar::foobar baz;\n"
      "    endclass;\n"
      "endpackage;\n"
      "package bar;\n"
      "    class foobar;\n"
      "        foo::foobar baz;\n"
      "    endclass;\n"
      "endpackage;\n";
  absl::string_view filelist_content = "rename.sv\n";

  const verible::file::testing::ScopedTestFile filelist(
      root_dir, filelist_content, "verible.filelist");
  const verible::file::testing::ScopedTestFile module_foo(root_dir, renamesv,
                                                          "rename.sv");

  const std::string mini_module = DidOpenRequest(file_uri, renamesv);
  ASSERT_OK(SendRequest(mini_module));

  const json diagnostics = json::parse(GetResponse());
  EXPECT_EQ(diagnostics["method"], "textDocument/publishDiagnostics")
      << "textDocument/publishDiagnostics not received";
  EXPECT_EQ(diagnostics["params"]["uri"],
            PathToLSPUri(verible::lsp::LSPUriToPath(file_uri)))
      << "Diagnostics for invalid file";

  // Complaints about package and file names
  EXPECT_EQ(diagnostics["params"]["diagnostics"].size(), 2)
      << "The test file has errors";
  std::string request = RenameRequest(params);
  ASSERT_OK(SendRequest(request));

  const json response = json::parse(GetResponse());
  EXPECT_EQ(response["result"]["changes"].size(), 1)
      << "Invalid result size for id:  ";
  EXPECT_EQ(response["result"]["changes"][file_uri].size(), 2)
      << "Invalid result size for id:  ";
}

// Tests correctness of Language Server shutdown request
TEST_F(VerilogLanguageServerTest, ShutdownTest) {
  const absl::string_view shutdown_request =
      R"({"jsonrpc":"2.0", "id":100, "method":"shutdown","params":{}})";

  ASSERT_OK(SendRequest(shutdown_request));

  const json response = json::parse(GetResponse());
  EXPECT_EQ(response["id"], 100);
}

}  // namespace
}  // namespace verilog
