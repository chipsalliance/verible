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

#include <filesystem>

#include "absl/flags/flag.h"
#include "absl/strings/match.h"
#include "common/lsp/lsp-protocol-enums.h"
#include "common/lsp/lsp-protocol.h"
#include "common/util/file_util.h"
#include "gtest/gtest.h"
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

class VerilogLanguageServerSymbolTableTest : public VerilogLanguageServerTest {
 public:
  absl::Status InitializeCommunication() override {
    json initialize_request = {{"jsonrpc", "2.0"},
                               {"id", 1},
                               {"method", "initialize"},
                               {"params", {{"rootUri", "file://" + root_dir}}}};
    return SendRequest(initialize_request.dump());
  }

 protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_rules_config_search, true);
    root_dir = verible::file::JoinPath(
        ::testing::TempDir(),
        ::testing::UnitTest::GetInstance()->current_test_info()->name());
    absl::Status status = verible::file::CreateDir(root_dir);
    ASSERT_OK(status) << status;
    VerilogLanguageServerTest::SetUp();
  }

  void TearDown() override { std::filesystem::remove(root_dir); }

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

// Creates a textDocument/definition request
std::string DefinitionRequest(absl::string_view file, int id, int line,
                              int character) {
  json formattingrequest = {
      {"jsonrpc", "2.0"},
      {"id", id},
      {"method", "textDocument/definition"},
      {"params",
       {{"textDocument", {{"uri", file}}},
        {"position", {{"line", line}, {"character", character}}}}}};
  return formattingrequest.dump();
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

  const std::string module_a_open_request =
      DidOpenRequest("file://" + module_a.filename(), kSampleModuleA);
  ASSERT_OK(SendRequest(module_a_open_request));

  // obtain diagnostics
  GetResponse();

  // find definition for "var1" variable in a.sv file
  std::string definition_request =
      DefinitionRequest("file://" + module_a.filename(), 2, 2, 16);

  ASSERT_OK(SendRequest(definition_request));
  json response = json::parse(GetResponse());

  ASSERT_EQ(response["id"], 2);
  ASSERT_EQ(response["result"].size(), 1);
  ASSERT_EQ(response["result"][0]["range"]["start"]["line"], 1);
  ASSERT_EQ(response["result"][0]["range"]["start"]["character"], 9);
  ASSERT_EQ(response["result"][0]["range"]["end"]["line"], 1);
  ASSERT_EQ(response["result"][0]["range"]["end"]["character"], 13);
  ASSERT_EQ(response["result"][0]["uri"], "file://" + module_a.filename());
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

  const std::string module_a_open_request =
      DidOpenRequest("file://" + module_a.filename(), kSampleModuleA);
  ASSERT_OK(SendRequest(module_a_open_request));
  const std::string module_b_open_request =
      DidOpenRequest("file://" + module_b.filename(), kSampleModuleB);
  ASSERT_OK(SendRequest(module_b_open_request));

  // obtain diagnostics for both files
  GetResponse();

  // find definition for "var1" variable in b.sv file
  std::string definition_request =
      DefinitionRequest("file://" + module_b.filename(), 2, 2, 16);

  ASSERT_OK(SendRequest(definition_request));
  json response_b = json::parse(GetResponse());

  ASSERT_EQ(response_b["id"], 2);
  ASSERT_EQ(response_b["result"].size(), 1);
  ASSERT_EQ(response_b["result"][0]["range"]["start"]["line"], 1);
  ASSERT_EQ(response_b["result"][0]["range"]["start"]["character"], 9);
  ASSERT_EQ(response_b["result"][0]["range"]["end"]["line"], 1);
  ASSERT_EQ(response_b["result"][0]["range"]["end"]["character"], 13);
  ASSERT_EQ(response_b["result"][0]["uri"], "file://" + module_b.filename());

  // find definition for "var1" variable in a.sv file
  definition_request =
      DefinitionRequest("file://" + module_a.filename(), 3, 2, 16);

  ASSERT_OK(SendRequest(definition_request));
  json response_a = json::parse(GetResponse());

  ASSERT_EQ(response_a["id"], 3);
  ASSERT_EQ(response_a["result"].size(), 1);
  ASSERT_EQ(response_a["result"][0]["range"]["start"]["line"], 1);
  ASSERT_EQ(response_a["result"][0]["range"]["start"]["character"], 9);
  ASSERT_EQ(response_a["result"][0]["range"]["end"]["line"], 1);
  ASSERT_EQ(response_a["result"][0]["range"]["end"]["character"], 13);
  ASSERT_EQ(response_a["result"][0]["uri"], "file://" + module_a.filename());
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

  const std::string module_a_open_request =
      DidOpenRequest("file://" + module_a.filename(), kSampleModuleA);
  ASSERT_OK(SendRequest(module_a_open_request));
  const std::string module_b_open_request =
      DidOpenRequest("file://" + module_b.filename(), kSampleModuleB);
  ASSERT_OK(SendRequest(module_b_open_request));

  // obtain diagnostics for both files
  GetResponse();

  // find definition for "var1" variable in b.sv file
  std::string definition_request =
      DefinitionRequest("file://" + module_b.filename(), 2, 4, 14);

  ASSERT_OK(SendRequest(definition_request));
  json response_b = json::parse(GetResponse());

  ASSERT_EQ(response_b["id"], 2);
  ASSERT_EQ(response_b["result"].size(), 1);
  ASSERT_EQ(response_b["result"][0]["range"]["start"]["line"], 1);
  ASSERT_EQ(response_b["result"][0]["range"]["start"]["character"], 9);
  ASSERT_EQ(response_b["result"][0]["range"]["end"]["line"], 1);
  ASSERT_EQ(response_b["result"][0]["range"]["end"]["character"], 13);
  ASSERT_EQ(response_b["result"][0]["uri"], "file://" + module_a.filename());
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

  const std::string module_b_open_request =
      DidOpenRequest("file://" + module_b.filename(), kSampleModuleB);
  ASSERT_OK(SendRequest(module_b_open_request));

  // obtain diagnostics for both files
  GetResponse();

  // find definition for "var1" variable in b.sv file
  std::string definition_request =
      DefinitionRequest("file://" + module_b.filename(), 2, 4, 14);

  ASSERT_OK(SendRequest(definition_request));
  json response_b = json::parse(GetResponse());

  ASSERT_EQ(response_b["id"], 2);
  ASSERT_EQ(response_b["result"].size(), 1);
  ASSERT_EQ(response_b["result"][0]["range"]["start"]["line"], 1);
  ASSERT_EQ(response_b["result"][0]["range"]["start"]["character"], 9);
  ASSERT_EQ(response_b["result"][0]["range"]["end"]["line"], 1);
  ASSERT_EQ(response_b["result"][0]["range"]["end"]["character"], 13);
  ASSERT_EQ(response_b["result"][0]["uri"], "file://" + module_a.filename());
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

  const std::string module_a_open_request =
      DidOpenRequest("file://" + module_a.filename(), kSampleModuleA);
  ASSERT_OK(SendRequest(module_a_open_request));
  const std::string module_b_open_request =
      DidOpenRequest("file://" + module_b.filename(), kSampleModuleB);
  ASSERT_OK(SendRequest(module_b_open_request));

  // Close a.sv from the Language Server perspective
  const std::string closing_request = json{
      //
      {"jsonrpc", "2.0"},
      {"method", "textDocument/didClose"},
      {"params",
       {{"textDocument",
         {
             {"uri", "file://" + module_a.filename()},
         }}}}}.dump();
  ASSERT_OK(SendRequest(closing_request));

  // obtain diagnostics for both files
  GetResponse();

  // find definition for "var1" variable of a module in b.sv file
  std::string definition_request =
      DefinitionRequest("file://" + module_b.filename(), 2, 4, 14);

  ASSERT_OK(SendRequest(definition_request));
  json response_b = json::parse(GetResponse());

  ASSERT_EQ(response_b["id"], 2);
  ASSERT_EQ(response_b["result"].size(), 1);
  ASSERT_EQ(response_b["result"][0]["range"]["start"]["line"], 1);
  ASSERT_EQ(response_b["result"][0]["range"]["start"]["character"], 9);
  ASSERT_EQ(response_b["result"][0]["range"]["end"]["line"], 1);
  ASSERT_EQ(response_b["result"][0]["range"]["end"]["character"], 13);
  ASSERT_EQ(response_b["result"][0]["uri"], "file://" + module_a.filename());

  // perform double check
  ASSERT_OK(SendRequest(definition_request));
  response_b = json::parse(GetResponse());

  ASSERT_EQ(response_b["id"], 2);
  ASSERT_EQ(response_b["result"].size(), 1);
  ASSERT_EQ(response_b["result"][0]["range"]["start"]["line"], 1);
  ASSERT_EQ(response_b["result"][0]["range"]["start"]["character"], 9);
  ASSERT_EQ(response_b["result"][0]["range"]["end"]["line"], 1);
  ASSERT_EQ(response_b["result"][0]["range"]["end"]["character"], 13);
  ASSERT_EQ(response_b["result"][0]["uri"], "file://" + module_a.filename());
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

  const std::string module_a_open_request =
      DidOpenRequest("file://" + module_a.filename(), kSampleModuleA);
  ASSERT_OK(SendRequest(module_a_open_request));

  // obtain diagnostics for both files
  GetResponse();

  // find definition for "var1" variable in a.sv file
  std::string definition_request =
      DefinitionRequest("file://" + module_a.filename(), 2, 2, 16);

  ASSERT_OK(SendRequest(definition_request));
  json response = json::parse(GetResponse());

  ASSERT_EQ(response["id"], 2);
  ASSERT_EQ(response["result"].size(), 1);
  ASSERT_EQ(response["result"][0]["range"]["start"]["line"], 1);
  ASSERT_EQ(response["result"][0]["range"]["start"]["character"], 9);
  ASSERT_EQ(response["result"][0]["range"]["end"]["line"], 1);
  ASSERT_EQ(response["result"][0]["range"]["end"]["character"], 13);
  ASSERT_EQ(response["result"][0]["uri"], "file://" + module_a.filename());
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

  const std::string module_a_open_request =
      DidOpenRequest("file://" + module_a.filename(), kSampleModuleA);
  ASSERT_OK(SendRequest(module_a_open_request));

  // obtain diagnostics for both files
  GetResponse();

  // find definition for "var1" variable of a module in b.sv file
  std::string definition_request =
      DefinitionRequest("file://" + module_b.filename(), 2, 4, 15);

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

  const std::string module_a_open_request =
      DidOpenRequest("file://" + module_a.filename(), kSampleModuleA);
  ASSERT_OK(SendRequest(module_a_open_request));

  // obtain diagnostics for both files
  GetResponse();

  // find definition for "var1" variable in a.sv file
  std::string definition_request =
      DefinitionRequest("https://" + module_a.filename(), 2, 2, 16);

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

  const std::string module_a_open_request =
      DidOpenRequest("file://" + module_a.filename(), kSampleModuleA);
  ASSERT_OK(SendRequest(module_a_open_request));

  // obtain diagnostics for both files
  GetResponse();

  // find definition for "var1" variable in a.sv file
  std::string definition_request =
      DefinitionRequest("file://" + module_a.filename(), 2, 1, 10);

  ASSERT_OK(SendRequest(definition_request));
  json response = json::parse(GetResponse());

  ASSERT_EQ(response["id"], 2);
  ASSERT_EQ(response["result"].size(), 1);
  ASSERT_EQ(response["result"][0]["range"]["start"]["line"], 1);
  ASSERT_EQ(response["result"][0]["range"]["start"]["character"], 9);
  ASSERT_EQ(response["result"][0]["range"]["end"]["line"], 1);
  ASSERT_EQ(response["result"][0]["range"]["end"]["character"], 13);
  ASSERT_EQ(response["result"][0]["uri"], "file://" + module_a.filename());
}

// Check textDocument/definition when the cursor points at nothing
TEST_F(VerilogLanguageServerSymbolTableTest,
       DefinitionRequestCursorAtNoSymbol) {
  absl::string_view filelist_content = "a.sv";

  const verible::file::testing::ScopedTestFile filelist(
      root_dir, filelist_content, "verible.filelist");
  const verible::file::testing::ScopedTestFile module_a(root_dir,
                                                        kSampleModuleA, "a.sv");

  const std::string module_a_open_request =
      DidOpenRequest("file://" + module_a.filename(), kSampleModuleA);
  ASSERT_OK(SendRequest(module_a_open_request));

  // obtain diagnostics for both files
  GetResponse();

  // find definition for "var1" variable in a.sv file
  std::string definition_request =
      DefinitionRequest("file://" + module_a.filename(), 2, 1, 0);

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

  const std::string module_b_open_request =
      DidOpenRequest("file://" + module_b.filename(), kSampleModuleB);
  ASSERT_OK(SendRequest(module_b_open_request));

  // obtain diagnostics for both files
  GetResponse();

  // find definition for "var1" variable in a.sv file
  std::string definition_request =
      DefinitionRequest("file://" + module_b.filename(), 2, 3, 2);

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

  const std::string module_a_open_request =
      DidOpenRequest("file://" + module_a.filename(), kSampleModuleA);
  ASSERT_OK(SendRequest(module_a_open_request));

  // obtain diagnostics
  GetResponse();

  // find definition for "var1" variable in a.sv file
  std::string definition_request =
      DefinitionRequest("file://" + module_a.filename(), 2, 2, 16);

  ASSERT_OK(SendRequest(definition_request));
  json response = json::parse(GetResponse());

  ASSERT_EQ(response["result"].size(), 1);
  ASSERT_EQ(response["result"][0]["uri"], "file://" + module_a.filename());
}

// Check textDocument/definition request where we want definition of a symbol
// inside other module edited in buffer without a filelist
TEST_F(VerilogLanguageServerSymbolTableTest,
       DefinitionRequestSymbolFromDifferentOpenedModuleNoFileList) {
  const verible::file::testing::ScopedTestFile module_a(root_dir,
                                                        kSampleModuleA, "a.sv");
  const verible::file::testing::ScopedTestFile module_b(root_dir,
                                                        kSampleModuleB, "b.sv");

  const std::string module_a_open_request =
      DidOpenRequest("file://" + module_a.filename(), kSampleModuleA);
  ASSERT_OK(SendRequest(module_a_open_request));
  const std::string module_b_open_request =
      DidOpenRequest("file://" + module_b.filename(), kSampleModuleB);
  ASSERT_OK(SendRequest(module_b_open_request));

  // obtain diagnostics for both files
  GetResponse();

  // find definition for "var1" variable in b.sv file
  std::string definition_request =
      DefinitionRequest("file://" + module_b.filename(), 2, 4, 14);

  ASSERT_OK(SendRequest(definition_request));
  json response_b = json::parse(GetResponse());

  ASSERT_EQ(response_b["id"], 2);
  ASSERT_EQ(response_b["result"].size(), 1);
  ASSERT_EQ(response_b["result"][0]["range"]["start"]["line"], 1);
  ASSERT_EQ(response_b["result"][0]["range"]["start"]["character"], 9);
  ASSERT_EQ(response_b["result"][0]["range"]["end"]["line"], 1);
  ASSERT_EQ(response_b["result"][0]["range"]["end"]["character"], 13);
  ASSERT_EQ(response_b["result"][0]["uri"], "file://" + module_a.filename());
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

  const std::string foo_open_request =
      DidOpenRequest("file://" + module_foo.filename(), foo);
  ASSERT_OK(SendRequest(foo_open_request));

  GetResponse();

  // find definition for "bar" type
  std::string definition_request =
      DefinitionRequest("file://" + module_foo.filename(), 2, 1, 3);

  ASSERT_OK(SendRequest(definition_request));
  json response = json::parse(GetResponse());

  ASSERT_EQ(response["id"], 2);
  ASSERT_EQ(response["result"].size(), 1);
  ASSERT_EQ(response["result"][0]["range"]["start"]["line"], 0);
  ASSERT_EQ(response["result"][0]["range"]["start"]["character"], 7);
  ASSERT_EQ(response["result"][0]["range"]["end"]["line"], 0);
  ASSERT_EQ(response["result"][0]["range"]["end"]["character"], 10);
  ASSERT_EQ(response["result"][0]["uri"], "file://" + module_bar_1.filename());
}

// Sample of badly styled modle
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
      DidOpenRequest("file://" + module_mod.filename(), badly_styled_module);

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
      DidOpenRequest("file://" + module_mod.filename(), badly_styled_module);

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
      DidOpenRequest("file://" + module_mod.filename(), badly_styled_module);

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
