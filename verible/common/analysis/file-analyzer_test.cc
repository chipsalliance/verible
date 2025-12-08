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

// Unit tests for FileAnalyzer

#include "verible/common/analysis/file-analyzer.h"

#include <sstream>
#include <string>
#include <string_view>

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "gtest/gtest.h"
#include "verible/common/strings/line-column-map.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"

namespace verible {
namespace {

TEST(RejectedTokenStreamTest, StringRepresentation) {
  const RejectedToken reject{
      TokenInfo(77, "foobar"),
      AnalysisPhase::kParsePhase,
      "bad syntax",
  };
  std::ostringstream stream;
  stream << reject;
  EXPECT_EQ(stream.str(), "(#77: \"foobar\") (syntax error): bad syntax");
}

// Subclass for the purpose of testing FileAnalyzer.
class FakeFileAnalyzer : public FileAnalyzer {
 public:
  FakeFileAnalyzer(const std::string &text, const std::string &filename)
      : FileAnalyzer(text, filename) {}

  absl::Status Tokenize() final {
    // TODO(fangism): Tokenize(lexer) interface is not directly tested, but
    // covered elsewhere.
    return absl::OkStatus();
  }
};

// Verify that an error token on one line is reported correctly.
TEST(FileAnalyzerTest, TokenErrorMessageSameLine) {
  const std::string text("hello, world\nbye w0rld\n");
  FakeFileAnalyzer analyzer(text, "hello.txt");
  const TokenInfo error_token(1, analyzer.Data().Contents().substr(17, 5));
  {
    const auto message = analyzer.TokenErrorMessage(error_token);
    EXPECT_EQ(message, "token: \"w0rld\" at 2:5-9:");
  }
  {
    constexpr bool with_diagnostic_context = false;
    const auto message = analyzer.LinterTokenErrorMessage(
        {error_token, AnalysisPhase::kParsePhase}, with_diagnostic_context);
    EXPECT_TRUE(absl::StrContains(
        message, "hello.txt:2:5-9: syntax error at token \"w0rld\""))
        << message;
  }
  {
    analyzer.ExtractLinterTokenErrorDetail(
        {error_token, AnalysisPhase::kParsePhase},
        [](const std::string &filename, LineColumnRange range,
           ErrorSeverity severity, AnalysisPhase phase,
           std::string_view token_text, std::string_view context_line,
           const std::string &message) {
          EXPECT_EQ(filename, "hello.txt");
          EXPECT_EQ(range.start.line, 1);
          EXPECT_EQ(range.start.column, 4);
          EXPECT_EQ(range.end.line, 1);
          EXPECT_EQ(range.end.column, 9);
          EXPECT_EQ(phase, AnalysisPhase::kParsePhase);
          EXPECT_EQ(token_text, "w0rld");
        });
  }
}

// Verify that an error token on one line is reported correctly.
TEST(FileAnalyzerTest, TokenErrorMessageSameLineWithContext) {
  const std::string text("hello, world\nbye w0rld\n");
  FakeFileAnalyzer analyzer(text, "hello.txt");
  const TokenInfo error_token(1, analyzer.Data().Contents().substr(17, 5));
  {
    const auto message = analyzer.TokenErrorMessage(error_token);
    EXPECT_EQ(message, "token: \"w0rld\" at 2:5-9:");
  }
  {
    constexpr bool with_diagnostic_context = true;
    const auto message = analyzer.LinterTokenErrorMessage(
        {error_token, AnalysisPhase::kParsePhase}, with_diagnostic_context);
    EXPECT_TRUE(
        absl::StrContains(message,
                          "hello.txt:2:5-9: syntax error at token \"w0rld\"\n"
                          "bye w0rld\n"
                          "    ^"))
        << message;
  }
  {
    analyzer.ExtractLinterTokenErrorDetail(
        {error_token, AnalysisPhase::kParsePhase},
        [](const std::string &filename, LineColumnRange range,
           ErrorSeverity severity, AnalysisPhase phase,
           std::string_view token_text, std::string_view context_line,
           const std::string &message) {
          EXPECT_EQ(filename, "hello.txt");
          EXPECT_EQ(range.start.line, 1);
          EXPECT_EQ(range.start.column, 4);
          EXPECT_EQ(range.end.line, 1);
          EXPECT_EQ(range.end.column, 9);
          EXPECT_EQ(phase, AnalysisPhase::kParsePhase);
          EXPECT_EQ(severity, ErrorSeverity::kError);  // The default.
          EXPECT_EQ(token_text, "w0rld");
          EXPECT_EQ(context_line, "bye w0rld");
        });
  }
}

// Verify that an error token on one character is reported correctly.
TEST(FileAnalyzerTest, TokenErrorMessageOneChar) {
  const std::string text("hello, world\nbye w0rld\n");
  FakeFileAnalyzer analyzer(text, "hello.txt");
  const TokenInfo error_token(1, analyzer.Data().Contents().substr(5, 1));
  {
    const auto message = analyzer.TokenErrorMessage(error_token);
    EXPECT_EQ(message, "token: \",\" at 1:6:");
  }
  {
    constexpr bool with_diagnostic_context = false;
    const auto message = analyzer.LinterTokenErrorMessage(
        {error_token, AnalysisPhase::kParsePhase}, with_diagnostic_context);
    EXPECT_TRUE(absl::StrContains(
        message, "hello.txt:1:6: syntax error at token \",\""));
  }
  {
    analyzer.ExtractLinterTokenErrorDetail(
        {error_token, AnalysisPhase::kParsePhase},
        [](const std::string &filename, LineColumnRange range,
           ErrorSeverity severity, AnalysisPhase phase,
           std::string_view token_text, std::string_view context_line,
           const std::string &message) {
          EXPECT_EQ(filename, "hello.txt");
          EXPECT_EQ(range.start.line, 0);
          EXPECT_EQ(range.start.column, 5);
          EXPECT_EQ(range.end.line, 0);
          EXPECT_EQ(range.end.column, 6);
          EXPECT_EQ(phase, AnalysisPhase::kParsePhase);
          EXPECT_EQ(token_text, ",");
          EXPECT_EQ(context_line, "hello, world");
        });
  }
}

// Verify that an error token on one character is reported correctly.
TEST(FileAnalyzerTest, TokenErrorMessageOneCharWithContext) {
  const std::string text("\thello, world\nbye w0rld\n");
  FakeFileAnalyzer analyzer(text, "hello.txt");
  const TokenInfo error_token(1, analyzer.Data().Contents().substr(6, 1));
  {
    const auto message = analyzer.TokenErrorMessage(error_token);
    EXPECT_EQ(message, "token: \",\" at 1:7:");
  }
  {
    constexpr bool with_diagnostic_context = true;
    const auto message = analyzer.LinterTokenErrorMessage(
        {error_token, AnalysisPhase::kParsePhase}, with_diagnostic_context);
    // The tab character is replaced with a space for the arrow to align
    EXPECT_TRUE(absl::StrContains(message,
                                  "hello.txt:1:7: syntax error at token \",\"\n"
                                  " hello, world\n"
                                  "      ^"))
        << message;
  }
}

// Verify that an error token spanning multiple lines is reported correctly.
TEST(FileAnalyzerTest, TokenErrorMessageDifferentLine) {
  const std::string text("hello, world\nbye w0rld\n");
  FakeFileAnalyzer analyzer(text, "hello.txt");
  const TokenInfo error_token(1, analyzer.Data().Contents().substr(7, 9));
  {
    const auto message = analyzer.TokenErrorMessage(error_token);
    EXPECT_EQ(message, "token: \"world\nbye\" at 1:8:2:3:");
  }
  {
    constexpr bool with_diagnostic_context = false;
    const auto message = analyzer.LinterTokenErrorMessage(
        {error_token, AnalysisPhase::kParsePhase}, with_diagnostic_context);
    EXPECT_TRUE(absl::StrContains(
        message, "hello.txt:1:8:2:3: syntax error at token \"world\nbye\""));
  }
  {
    analyzer.ExtractLinterTokenErrorDetail(
        {error_token, AnalysisPhase::kParsePhase},
        [](const std::string &filename, LineColumnRange range,
           ErrorSeverity severity, AnalysisPhase phase,
           std::string_view token_text, std::string_view context_line,
           const std::string &message) {
          EXPECT_EQ(filename, "hello.txt");
          EXPECT_EQ(range.start.line, 0);
          EXPECT_EQ(range.start.column, 7);
          EXPECT_EQ(range.end.line, 1);
          EXPECT_EQ(range.end.column, 3);
          EXPECT_EQ(phase, AnalysisPhase::kParsePhase);
          EXPECT_EQ(token_text, "world\nbye");
          EXPECT_EQ(context_line, "hello, world");
        });
  }
}

// Verify that an error token spanning multiple lines is reported correctly.
TEST(FileAnalyzerTest, TokenErrorMessageDifferentLineWithContext) {
  const std::string text("hello, world\nbye w0rld\n");
  FakeFileAnalyzer analyzer(text, "hello.txt");
  const TokenInfo error_token(1, analyzer.Data().Contents().substr(7, 9));
  {
    const auto message = analyzer.TokenErrorMessage(error_token);
    EXPECT_EQ(message, "token: \"world\nbye\" at 1:8:2:3:");
  }
  {
    constexpr bool with_diagnostic_context = true;
    const auto message = analyzer.LinterTokenErrorMessage(
        {error_token, AnalysisPhase::kParsePhase}, with_diagnostic_context);
    EXPECT_TRUE(absl::StrContains(
        message,
        "hello.txt:1:8:2:3: syntax error at token \"world\nbye\"\n"
        "hello, world\n"
        "       ^"))
        << message;
  }
}

//
// Verify that an error at EOF reported correctly.
TEST(FileAnalyzerTest, TokenErrorMessageEOF) {
  const std::string text("hello, world\nbye w0rld (\n");
  const TokenInfo error_token(TokenInfo::EOFToken());
  FakeFileAnalyzer analyzer(text, "unbalanced.txt");
  {
    const auto message = analyzer.TokenErrorMessage(error_token);
    EXPECT_EQ(message, "token: <<EOF>> at 3:1");
  }
  {
    constexpr bool with_diagnostic_context = false;
    const auto message = analyzer.LinterTokenErrorMessage(
        {error_token, AnalysisPhase::kParsePhase}, with_diagnostic_context);
    EXPECT_TRUE(absl::StrContains(
        message, "unbalanced.txt:3:1: syntax error (unexpected EOF)"));
  }
}

//
// Verify that an error at EOF reported correctly.
TEST(FileAnalyzerTest, TokenErrorMessageEOFWithContext) {
  const std::string text("hello, world\nbye w0rld (\nmissing");
  const TokenInfo error_token(TokenInfo::EOFToken());
  FakeFileAnalyzer analyzer(text, "unbalanced.txt");
  {
    const auto message = analyzer.TokenErrorMessage(error_token);
    EXPECT_EQ(message, "token: <<EOF>> at 3:8");
  }
  {
    constexpr bool with_diagnostic_context = true;
    const auto message = analyzer.LinterTokenErrorMessage(
        {error_token, AnalysisPhase::kParsePhase}, with_diagnostic_context);
    EXPECT_TRUE(absl::StrContains(
        message, "unbalanced.txt:3:8: syntax error (unexpected EOF)"))
        << message;
  }
  {
    analyzer.ExtractLinterTokenErrorDetail(
        {error_token, AnalysisPhase::kParsePhase},
        [](const std::string &filename, LineColumnRange range,
           ErrorSeverity severity, AnalysisPhase phase,
           std::string_view token_text, std::string_view context_line,
           const std::string &message) {
          EXPECT_EQ(filename, "unbalanced.txt");
          EXPECT_EQ(range.start.line, 2);
          EXPECT_EQ(range.start.column, 7);
          EXPECT_EQ(range.end.line, 2);
          EXPECT_EQ(range.end.column, 7);
          EXPECT_EQ(phase, AnalysisPhase::kParsePhase);
          EXPECT_EQ(token_text, "<EOF>");
          EXPECT_EQ(context_line, "missing");
        });
  }
}

}  // namespace
}  // namespace verible
