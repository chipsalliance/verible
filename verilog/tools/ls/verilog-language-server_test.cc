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

#include "absl/strings/match.h"
#include "common/lsp/lsp-protocol-enums.h"
#include "common/lsp/lsp-protocol.h"
#include "gtest/gtest.h"

#undef ASSERT_OK
#define ASSERT_OK(value)                      \
  if (auto status__ = (value); status__.ok()) \
    ;                                         \
  else                                        \
    EXPECT_TRUE(status__.ok()) << status__

namespace verilog {
namespace {

// TODO (glatosinski) for JSON messages use types defined in lsp-protocol.h

using namespace nlohmann;

class VerilogLanguageServerTest : public ::testing::Test {
 public:
  // Sends initialize request from client mock to the Language Server.
  // It does not parse the response nor fetch it in any way (for other
  // tests to check e.g. server/client capabilities).
  absl::Status InitializeCommunication() {
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
    return response;
  }

  // Returns response to textDocument/initialize request
  const std::string &GetInitializeResponse() const {
    return initialize_response_;
  }

 private:
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
  void SetUp() override {
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

  EXPECT_EQ(toplevel[0].kind, verible::lsp::SymbolKind::Package);
  EXPECT_EQ(toplevel[0].name, "mini");

  EXPECT_EQ(toplevel[1].kind, verible::lsp::SymbolKind::Method);  // module.
  EXPECT_EQ(toplevel[1].name, "mini");

  // Descend tree into package and look at expected nested symbols there.
  std::vector<verible::lsp::DocumentSymbol> package = toplevel[0].children;
  EXPECT_EQ(package.size(), 2);
  EXPECT_EQ(package[0].kind, verible::lsp::SymbolKind::Function);
  EXPECT_EQ(package[0].name, "fun_foo");

  EXPECT_EQ(package[1].kind, verible::lsp::SymbolKind::Class);
  EXPECT_EQ(package[1].name, "some_class");

  // Descend tree into class and find nested function.
  std::vector<verible::lsp::DocumentSymbol> class_block = package[1].children;
  EXPECT_EQ(class_block.size(), 1);
  EXPECT_EQ(class_block[0].kind, verible::lsp::SymbolKind::Function);
  EXPECT_EQ(class_block[0].name, "member");

  // Descent tree into module and find labelled block.
  std::vector<verible::lsp::DocumentSymbol> module = toplevel[1].children;
  EXPECT_EQ(module.size(), 1);
  EXPECT_EQ(module[0].kind, verible::lsp::SymbolKind::Namespace);
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
    std::string request = FormattingRequest("file://fmt.sv", params);
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
