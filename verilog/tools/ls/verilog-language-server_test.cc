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

#include "gtest/gtest.h"

namespace verilog {
namespace {

using namespace nlohmann;

class VerilogLanguageServerTest : public ::testing::Test {
 public:
  // Wraps request for the Language Server in RPC header
  void Request(absl::string_view request) {
    requeststream.clear();
    requeststream.str(
        absl::StrCat("Content-Length: ", request.size(), "\r\n\r\n", request));
  }

  // Sends initialize request from client mock to the Language Server.
  // It does not parse the response nor fetch it in any way (for other
  // tests to check e.g. server/client capabilities).
  absl::Status InitializeCommunication(const VerilogLanguageServer *server) {
    const absl::string_view initialize =
        R"({ "jsonrpc": "2.0", "id": 1, "method": "initialize", "params": null })";

    Request(initialize);
    return ServerStep();
  }

  // Performs single VerilogLanguageServer step, fetching latest request
  absl::Status ServerStep() {
    return server->Step([this](char *message, int size) -> int {
      requeststream.read(message, size);
      return requeststream.gcount();
    });
  }

  // Returns pointer to the VerilogLanguageServer
  const VerilogLanguageServer *Server() { return server.get(); }

  // Returns the latest responses from the Language Server
  void Response(std::string *response) {
    *response = responsestream.str();
    responsestream.str("");
    responsestream.clear();
  }

  // Sets up the testing environment - creates Language Server object and
  // sends textDocument/initialize request.
  // It stores the response in initialize_response field for further processing
  void SetUp() override {
    server = std::make_unique<VerilogLanguageServer>(
        [this](absl::string_view response) { responsestream << response; });

    absl::Status status = InitializeCommunication(Server());
    EXPECT_TRUE(status.ok()) << "Failed to read request:  " << status;
    Response(&initialize_response);
  }

  // Currently tested instance of VerilogLanguageServer
  std::unique_ptr<VerilogLanguageServer> server;

  // Response from textDocument/initialize request - left for checking e.g
  // server capabilities
  std::string initialize_response;

  // Stream for passing requests to the Language Server
  std::stringstream requeststream;

  // Stream for receiving responses from the Language Server
  std::stringstream responsestream;
};

// Verifies textDocument/initialize request handling
TEST_F(VerilogLanguageServerTest, InitializeRequest) {
  json response = json::parse(initialize_response);

  ASSERT_EQ(response["id"], 1) << "Response message ID invalid";
  ASSERT_EQ(response["result"]["serverInfo"]["name"],
            "Verible Verilog language server.")
      << "Invalid Language Server name";
}

std::string DidOpenRequest(absl::string_view name, absl::string_view content) {
  return absl::StrCat(
      R"({ "jsonrpc": "2.0", "method": "textDocument/didOpen", "params": { "textDocument": {"uri": ")",
      name, R"(", "text": ")", content, R"("}}})");
}

// Checks automatic diagnostics for opened file and textDocument/diagnostic
// request for file with invalid syntax
TEST_F(VerilogLanguageServerTest, SyntaxError) {
  const std::string wrong_file =
      DidOpenRequest("file://syntaxerror.sv", R"(brokenfile\n)");
  Request(wrong_file);
  absl::Status status = ServerStep();
  ASSERT_TRUE(status.ok()) << "Failed to process file with syntax error:  "
                           << status;
  std::string response;
  Response(&response);
  json response_parsed = json::parse(response);
  ASSERT_EQ(response_parsed["method"], "textDocument/publishDiagnostics")
      << "textDocument/publishDiagnostics not received";
  ASSERT_EQ(response_parsed["params"]["uri"], "file://syntaxerror.sv")
      << "Diagnostics for invalid file";
  ASSERT_TRUE(absl::StrContains(
      response_parsed["params"]["diagnostics"][0]["message"], "syntax error"))
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
  Request(diagnostic_request);
  status = ServerStep();
  ASSERT_TRUE(status.ok()) << "Failed to process file with syntax error:  "
                           << status;
  std::string request_response;
  Response(&request_response);
  response_parsed = json::parse(request_response);
  ASSERT_EQ(response_parsed["id"], 2) << "Invalid id";
  ASSERT_EQ(response_parsed["result"]["kind"], "full")
      << "Diagnostics kind invalid";
  ASSERT_TRUE(absl::StrContains(
      response_parsed["result"]["items"][0]["message"], "syntax error"))
      << "No syntax error found";
}

// Tests diagnostics for file with linting error before and after fix
TEST_F(VerilogLanguageServerTest, LintErrorDetection) {
  const std::string lint_error =
      DidOpenRequest("file://mini.sv", R"(module mini();\nendmodule)");
  Request(lint_error);
  absl::Status status = ServerStep();
  ASSERT_TRUE(status.ok()) << "Failed to process file with linting error:  "
                           << status;
  std::string diagnostics;
  Response(&diagnostics);
  json diagnostics_parsed = json::parse(diagnostics);

  // Firstly, check correctness of diagnostics
  ASSERT_EQ(diagnostics_parsed["method"], "textDocument/publishDiagnostics")
      << "textDocument/publishDiagnostics not received";
  ASSERT_EQ(diagnostics_parsed["params"]["uri"], "file://mini.sv")
      << "Diagnostics for invalid file";
  ASSERT_TRUE(absl::StrContains(
      diagnostics_parsed["params"]["diagnostics"][0]["message"],
      "File must end with a newline."))
      << "No syntax error found";
  ASSERT_EQ(
      diagnostics_parsed["params"]["diagnostics"][0]["range"]["start"]["line"],
      1);
  ASSERT_EQ(diagnostics_parsed["params"]["diagnostics"][0]["range"]["start"]
                              ["character"],
            9);

  // Secondly, request a code action at the EOF error message position
  const absl::string_view action_request =
      R"({"jsonrpc":"2.0", "id":10, "method":"textDocument/codeAction","params":{"textDocument":{"uri":"file://mini.sv"},"range":{"start":{"line":1,"character":9},"end":{"line":1,"character":9}}}})";
  Request(action_request);
  status = ServerStep();
  ASSERT_TRUE(status.ok()) << status;
  std::string action_response;
  Response(&action_response);
  json action_parsed = json::parse(action_response);
  ASSERT_EQ(action_parsed["id"], 10);
  ASSERT_EQ(action_parsed["result"][0]["edit"]["changes"]["file://mini.sv"][0]
                         ["newText"],
            "\n");
  // Thirdly, apply change suggested by a code action and check diagnostics
  const absl::string_view fix_request =
      R"({"jsonrpc":"2.0","method":"textDocument/didChange","params":{"textDocument":{"uri":"file://mini.sv"},"contentChanges":[{"range":{"start":{"character":9,"line":1},"end":{"character":9,"line":1}},"text":"\n"}]}})";
  Request(fix_request);
  status = ServerStep();
  ASSERT_TRUE(status.ok()) << status;
  std::string fix_diagnostics;
  Response(&fix_diagnostics);
  json fix_parsed = json::parse(fix_diagnostics);
  ASSERT_EQ(fix_parsed["method"], "textDocument/publishDiagnostics");
  ASSERT_EQ(fix_parsed["params"]["uri"], "file://mini.sv");
  ASSERT_EQ(fix_parsed["params"]["diagnostics"].size(), 0);
}

// Tests textDocument/documentSymbol request support
TEST_F(VerilogLanguageServerTest, DocumentSymbolRequestTest) {
  // Create file and make sure no diagnostic errors were reported
  const std::string mini_module =
      DidOpenRequest("file://mini.sv", R"(module mini();\nendmodule\n)");
  Request(mini_module);
  absl::Status status = ServerStep();
  ASSERT_TRUE(status.ok()) << status;
  std::string diagnostics;
  Response(&diagnostics);

  json diagnostics_parsed = json::parse(diagnostics);

  ASSERT_EQ(diagnostics_parsed["method"], "textDocument/publishDiagnostics")
      << "textDocument/publishDiagnostics not received";
  ASSERT_EQ(diagnostics_parsed["params"]["uri"], "file://mini.sv")
      << "Diagnostics for invalid file";
  ASSERT_EQ(diagnostics_parsed["params"]["diagnostics"].size(), 0)
      << "The test file has errors";
  // Request a document symbol
  const absl::string_view document_symbol_request =
      R"({"jsonrpc":"2.0", "id":11, "method":"textDocument/documentSymbol","params":{"textDocument":{"uri":"file://mini.sv"}}})";
  Request(document_symbol_request);
  status = ServerStep();
  ASSERT_TRUE(status.ok()) << status;
  std::string document_symbol_response;
  Response(&document_symbol_response);

  json document_symbol_parsed = json::parse(document_symbol_response);

  ASSERT_EQ(document_symbol_parsed["id"], 11);
  ASSERT_EQ(document_symbol_parsed["result"].size(), 1);
  ASSERT_EQ(document_symbol_parsed["result"][0]["kind"], 6);
  ASSERT_EQ(document_symbol_parsed["result"][0]["name"], "mini");
}

// Tests closing of the file in the LS context and checks if the LS
// responds gracefully to textDocument/documentSymbol request for
// closed file.
TEST_F(VerilogLanguageServerTest,
       DocumentClosingFollowedByDocumentSymbolRequest) {
  const std::string mini_module =
      DidOpenRequest("file://mini.sv", R"(module mini();\nendmodule\n)");
  Request(mini_module);
  absl::Status status = ServerStep();
  ASSERT_TRUE(status.ok()) << status;
  std::string diagnostics;
  Response(&diagnostics);

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
  Request(closing_request);
  status = ServerStep();
  ASSERT_TRUE(status.ok()) << status;

  // Try to request document symbol for closed file (server should return empty
  // response gracefully)
  const absl::string_view document_symbol_request =
      R"({"jsonrpc":"2.0", "id":13, "method":"textDocument/documentSymbol","params":{"textDocument":{"uri":"file://mini.sv"}}})";
  Request(document_symbol_request);
  status = ServerStep();
  ASSERT_TRUE(status.ok()) << status;
  std::string document_symbol_response;
  Response(&document_symbol_response);

  json document_symbol_parsed = json::parse(document_symbol_response);

  ASSERT_EQ(document_symbol_parsed["id"], 13);
  ASSERT_EQ(document_symbol_parsed["result"].size(), 0);
}

// Tests textDocument/documentHighlight request
TEST_F(VerilogLanguageServerTest, SymbolHighlightingTest) {
  // Create sample file and make sure diagnostics do not have errors
  const std::string mini_module = DidOpenRequest(
      "file://sym.sv", R"(module sym();\nassign a=1;assign b=a+1;endmodule\n)");
  Request(mini_module);
  absl::Status status = ServerStep();
  ASSERT_TRUE(status.ok()) << status;
  std::string diagnostics;
  Response(&diagnostics);

  json diagnostics_parsed = json::parse(diagnostics);

  ASSERT_EQ(diagnostics_parsed["method"], "textDocument/publishDiagnostics")
      << "textDocument/publishDiagnostics not received";
  ASSERT_EQ(diagnostics_parsed["params"]["uri"], "file://sym.sv")
      << "Diagnostics for invalid file";
  ASSERT_EQ(diagnostics_parsed["params"]["diagnostics"].size(), 0)
      << "The test file has errors";
  const absl::string_view highlight_request1 =
      R"({"jsonrpc":"2.0", "id":20, "method":"textDocument/documentHighlight","params":{"textDocument":{"uri":"file://sym.sv"},"position":{"line":1,"character":7}}})";
  Request(highlight_request1);
  status = ServerStep();
  ASSERT_TRUE(status.ok()) << status;

  std::string highlight_response1;
  Response(&highlight_response1);

  json highlight_response1_parsed = json::parse(highlight_response1);
  ASSERT_EQ(highlight_response1_parsed["id"], 20);
  ASSERT_EQ(highlight_response1_parsed["result"].size(), 2);
  ASSERT_EQ(
      highlight_response1_parsed["result"][0],
      json::parse(
          R"({"range":{"start":{"line":1, "character": 7}, "end":{"line":1, "character": 8}}})"));
  ASSERT_EQ(
      highlight_response1_parsed["result"][1],
      json::parse(
          R"({"range":{"start":{"line":1, "character": 20}, "end":{"line":1, "character": 21}}})"));

  const absl::string_view highlight_request2 =
      R"({"jsonrpc":"2.0", "id":21, "method":"textDocument/documentHighlight","params":{"textDocument":{"uri":"file://sym.sv"},"position":{"line":1,"character":2}}})";
  Request(highlight_request2);
  status = ServerStep();
  ASSERT_TRUE(status.ok()) << status;

  std::string highlight_response2;
  Response(&highlight_response2);

  json highlight_response2_parsed = json::parse(highlight_response2);
  ASSERT_EQ(highlight_response2_parsed["id"], 21);
  ASSERT_EQ(highlight_response2_parsed["result"].size(), 0);
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
      "file://fmt.sv", R"(module fmt();\nassign a=1;\nassign b=2;endmodule\n)");
  Request(mini_module);
  absl::Status status = ServerStep();
  ASSERT_TRUE(status.ok()) << status;
  std::string diagnostics;
  Response(&diagnostics);

  json diagnostics_parsed = json::parse(diagnostics);

  ASSERT_EQ(diagnostics_parsed["method"], "textDocument/publishDiagnostics")
      << "textDocument/publishDiagnostics not received";
  ASSERT_EQ(diagnostics_parsed["params"]["uri"], "file://fmt.sv")
      << "Diagnostics for invalid file";
  ASSERT_EQ(diagnostics_parsed["params"]["diagnostics"].size(), 0)
      << "The test file has errors";
  const std::vector<FormattingRequestParams> formatting_params{
      {30, 1, 0, 2, 0, "  assign a=1;\n", 1, 0, 2, 0},
      {31, 1, 0, 1, 1, "  assign a=1;\n", 1, 0, 2, 0},
      {32, 2, 0, 2, 1, "  assign b=2;\nendmodule\n", 2, 0, 3, 0},
      {33, 1, 0, 3, 0, "  assign a = 1;\n  assign b = 2;\nendmodule\n", 1, 0, 3,
       0}};

  for (const auto &params : formatting_params) {
    std::string request = FormattingRequest("file://fmt.sv", params);
    Request(request);
    status = ServerStep();
    ASSERT_TRUE(status.ok()) << status;
    std::string response;
    Response(&response);

    json response_parsed = json::parse(response);
    ASSERT_EQ(response_parsed["id"], params.id) << "Invalid id";
    ASSERT_EQ(response_parsed["result"].size(), 1)
        << "Invalid result size for id:  " << params.id;
    ASSERT_EQ(std::string(response_parsed["result"][0]["newText"]),
              params.new_text)
        << "Invalid patch for id:  " << params.id;
    ASSERT_EQ(response_parsed["result"][0]["range"]["start"]["line"],
              params.new_text_start_line)
        << "Invalid range for id:  " << params.id;
    ASSERT_EQ(response_parsed["result"][0]["range"]["start"]["character"],
              params.new_text_start_character)
        << "Invalid range for id:  " << params.id;
    ASSERT_EQ(response_parsed["result"][0]["range"]["end"]["line"],
              params.new_text_end_line)
        << "Invalid range for id:  " << params.id;
    ASSERT_EQ(response_parsed["result"][0]["range"]["end"]["character"],
              params.new_text_end_character)
        << "Invalid range for id:  " << params.id;
  }
}

// Runs test of entire document formatting with textDocument/formatting request
TEST_F(VerilogLanguageServerTest, FormattingTest) {
  // Create sample file and make sure diagnostics do not have errors
  const std::string mini_module = DidOpenRequest(
      "file://fmt.sv", R"(module fmt();\nassign a=1;\nassign b=2;endmodule\n)");
  Request(mini_module);
  absl::Status status = ServerStep();
  ASSERT_TRUE(status.ok()) << status;
  std::string diagnostics;
  Response(&diagnostics);

  json diagnostics_parsed = json::parse(diagnostics);

  ASSERT_EQ(diagnostics_parsed["method"], "textDocument/publishDiagnostics")
      << "textDocument/publishDiagnostics not received";
  ASSERT_EQ(diagnostics_parsed["params"]["uri"], "file://fmt.sv")
      << "Diagnostics for invalid file";
  ASSERT_EQ(diagnostics_parsed["params"]["diagnostics"].size(), 0)
      << "The test file has errors";

  const absl::string_view formatting_request =
      R"({"jsonrpc":"2.0", "id":34, "method":"textDocument/formatting","params":{"textDocument":{"uri":"file://fmt.sv"}}})";

  Request(formatting_request);
  status = ServerStep();
  ASSERT_TRUE(status.ok()) << status;
  std::string response;
  Response(&response);

  json response_parsed = json::parse(response);
  ASSERT_EQ(response_parsed["id"], 34);
  ASSERT_EQ(response_parsed["result"].size(), 1);
  ASSERT_EQ(std::string(response_parsed["result"][0]["newText"]),
            "module fmt ();\n  assign a = 1;\n  assign b = 2;\nendmodule\n");
  ASSERT_EQ(
      response_parsed["result"][0]["range"],
      json::parse(
          R"({"start":{"line":0, "character": 0}, "end":{"line":3, "character": 0}})"));
}

// Tests correctness of Language Server shutdown request
TEST_F(VerilogLanguageServerTest, ShutdownTest) {
  const std::string mini_module = DidOpenRequest(
      "file://fmt.sv", R"(module fmt();\nassign a=1;\nassign b=2;endmodule\n)");
  Request(mini_module);
  absl::Status status = ServerStep();
  ASSERT_TRUE(status.ok()) << status;
  std::string diagnostics;
  Response(&diagnostics);

  const absl::string_view formatting_request =
      R"({"jsonrpc":"2.0", "id":100, "method":"shutdown","params":{}})";

  Request(formatting_request);
  status = ServerStep();
  ASSERT_TRUE(status.ok()) << status;
  std::string response;
  Response(&response);

  json response_parsed = json::parse(response);

  ASSERT_EQ(response_parsed["id"], 100);
}
}  // namespace
}  // namespace verilog
