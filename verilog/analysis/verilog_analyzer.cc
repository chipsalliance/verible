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

// VerilogAnalyzer implementation (an example)
// Other related analyzers can follow the same structure.

#include "verilog/analysis/verilog_analyzer.h"

#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "common/analysis/file_analyzer.h"
#include "common/lexer/token_stream_adapter.h"
#include "common/strings/comment_utils.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/text_structure.h"
#include "common/text/token_info.h"
#include "common/text/token_stream_view.h"
#include "common/text/visitors.h"
#include "common/util/container_util.h"
#include "common/util/logging.h"
#include "common/util/status.h"
#include "common/util/status_macros.h"
#include "verilog/analysis/verilog_excerpt_parse.h"
#include "verilog/parser/verilog_lexer.h"
#include "verilog/parser/verilog_lexical_context.h"
#include "verilog/parser/verilog_parser.h"
#include "verilog/parser/verilog_token_enum.h"
#include "verilog/preprocessor/verilog_preprocess.h"

namespace verilog {

using verible::FileAnalyzer;
using verible::TokenInfo;
using verible::TokenSequence;
using verible::container::InsertKeyOrDie;

const char VerilogAnalyzer::kParseDirectiveName[] = "verilog_syntax:";

verible::util::Status VerilogAnalyzer::Tokenize() {
  if (!tokenized_) {
    VerilogLexer lexer{Data().Contents()};
    tokenized_ = true;
    lex_status_ = FileAnalyzer::Tokenize(&lexer);
  }
  return lex_status_;
}

absl::string_view VerilogAnalyzer::ScanParsingModeDirective(
    const TokenSequence& raw_tokens) {
  for (const auto& token : raw_tokens) {
    if (token.token_enum == TK_COMMENT_BLOCK ||
        token.token_enum == TK_EOL_COMMENT) {
      const absl::string_view comment_text =
          verible::StripCommentAndSpacePadding(token.text);
      const std::vector<absl::string_view> comment_tokens =
          absl::StrSplit(comment_text, ' ', absl::SkipEmpty());
      if (comment_tokens.size() >= 2 &&
          comment_tokens[0] == kParseDirectiveName) {
        // First directive wins.
        return comment_tokens[1];
      }
      continue;
    }
    // if encountered a non-preprocessing token, stop scanning.
    if (!VerilogLexer::KeepSyntaxTreeTokens(token)) continue;
    if (IsPreprocessorControlToken(token)) continue;
    break;
  }
  return "";
}

// Return a secondary parsing mode to attempt, depending on the token type of
// the first rejected token from parsing as top-level.
static const absl::string_view FailingTokenKeywordToParsingMode(
    verilog_tokentype token_type) {
  switch (token_type) {
    // For starting keywords that uniquely identify a parsing context,
    // retry parsing using that context.

    // TODO(fangism): Automatically generate these mappings based on
    // left-hand-side tokens of nonterminals in the generated parser's
    // internal state machine.  With this, a failing keyword could even map to
    // multiple parsing modes to try, but for now, we limit to one re-parse.

    // Keywords that are unique to module items:
    case verilog_tokentype::TK_always:
    case verilog_tokentype::TK_always_ff:
    case verilog_tokentype::TK_always_comb:
    case verilog_tokentype::TK_always_latch:
    case verilog_tokentype::TK_assign:
      // TK_assign could be procedural_continuous_assignment
      // statement as well (function/task).
    case verilog_tokentype::TK_final:
    case verilog_tokentype::TK_initial:  // also used in UDP blocks
      return "parse-as-module-body";

      // TODO(b/134023515): handle class-unique keywords

    default:
      break;
  }
  return "";
}

std::unique_ptr<VerilogAnalyzer> VerilogAnalyzer::AnalyzeAutomaticMode(
    absl::string_view text, absl::string_view name) {
  VLOG(2) << __FUNCTION__;
  auto analyzer = absl::make_unique<VerilogAnalyzer>(text, name);
  if (analyzer == nullptr) return analyzer;
  const absl::string_view text_base = analyzer->Data().Contents();
  // If there is any lexical error, stop right away.
  const auto lex_status = analyzer->Tokenize();
  if (!lex_status.ok()) return analyzer;
  const absl::string_view parse_mode =
      ScanParsingModeDirective(analyzer->Data().TokenStream());
  if (!parse_mode.empty()) {
    // Invoke alternate parser, and use its results.
    // Slightly inefficient to lex text all over again, but this is
    // acceptable for an exceptional code path.
    VLOG(1) << "Analyzing using parse mode directive: " << parse_mode;
    auto mode_analyzer = AnalyzeVerilogWithMode(text, name, parse_mode);
    if (mode_analyzer != nullptr) return mode_analyzer;
    // Silently ignore any unknown parsing modes.
  }

  // In all other cases, continue to parse in normal mode.  (common path)
  const auto parse_status = analyzer->Analyze();

  if (!parse_status.ok()) {
    VLOG(1) << "Error analyzing verilog.";
    // If there was a syntax error, look at the first rejected token
    // and try to infer whether or not to attempt to re-parse using
    // a different mode.
    const auto& rejected_tokens = analyzer->GetRejectedTokens();
    if (!rejected_tokens.empty()) {
      const auto& first_reject = rejected_tokens.front();
      const absl::string_view retry_parse_mode =
          FailingTokenKeywordToParsingMode(
              verilog_tokentype(first_reject.token_info.token_enum));
      VLOG(1) << "Retrying parsing in mode: " << retry_parse_mode;
      if (!retry_parse_mode.empty()) {
        auto retry_analyzer =
            AnalyzeVerilogWithMode(text, name, retry_parse_mode);
        const absl::string_view retry_text_base =
            retry_analyzer->Data().Contents();
        VLOG(1) << "Retrying to parse:\n" << retry_text_base;
        if (retry_analyzer->ParseStatus().ok()) {
          VLOG(1) << "Retrying parsing succeeded.";
          // Retry mode succeeded, proceed with this analyzer's results.
          return retry_analyzer;
        }
        // Compare the location of first errors, and return the analyzer that
        // got farther before encountering the first error.
        const auto& retry_rejected_tokens = retry_analyzer->GetRejectedTokens();
        if (!retry_rejected_tokens.empty()) {
          VLOG(1) << "Retrying parsing found at least one error.";
          const auto& first_retry_reject = retry_rejected_tokens.front();
          const int retry_error_offset = first_retry_reject.token_info.left(
              retry_analyzer->Data().Contents());
          // When comparing failure location, compensate position for prolog.
          const int original_error_offset =
              first_reject.token_info.left(text_base);
          if (retry_error_offset > original_error_offset) {
            VLOG(1) << "Retry's first error made it further.  Using that.";
            return retry_analyzer;
          }
          // Otherwise, fallback to the first analyzer.
        }
      }
    }
  }
  // TODO(fangism): also return the inferred or detected parsing mode
  VLOG(2) << "end of " << __FUNCTION__;
  return analyzer;
}

void VerilogAnalyzer::FilterTokensForSyntaxTree() {
  data_.FilterTokens(&VerilogLexer::KeepSyntaxTreeTokens);
}

void VerilogAnalyzer::ContextualizeTokens() {
  LexicalContext context;
  context.TransformVerilogSymbols(data_.MakeTokenStreamReferenceView());
}

// Analyzes Verilog code: lexer, filter, parser.
// Result of parsing is stored in syntax_tree_ (if passed)
// or rejected_token_ (if failed).
verible::util::Status VerilogAnalyzer::Analyze() {
  // Lex into tokens.
  RETURN_IF_ERROR(Tokenize());

  // Here would be one place to analyze the raw token stream.
  FilterTokensForSyntaxTree();

  // Disambiguate tokens using lexical context.
  ContextualizeTokens();

  // pseudo-preprocess token stream.
  // TODO(fangism): preprocessor_.Configure();
  //   Not all analyses will want to preprocess.
  {
    VerilogPreprocess preprocessor;
    preprocessor_data_ = preprocessor.ScanStream(Data().GetTokenStreamView());
    if (!preprocessor_data_.errors.empty()) {
      for (const auto& error : preprocessor_data_.errors) {
        rejected_tokens_.push_back(verible::RejectedToken{
            error.token_info, verible::AnalysisPhase::kPreprocessPhase,
            error.error_message});
      }
      parse_status_ =
          verible::util::InvalidArgumentError("Preprocessor error.");
      return parse_status_;
    }
    MutableData().MutableTokenStreamView() =
        preprocessor_data_.preprocessed_token_stream;  // copy
    // TODO(fangism): could we just move, swap, or directly reference?
  }

  auto generator = MakeTokenViewer(Data().GetTokenStreamView());
  VerilogParser parser(&generator);
  parse_status_ = FileAnalyzer::Parse(&parser);
  // Here would be appropriate for analyzing the syntax tree.
  max_used_stack_size_ = parser.MaxUsedStackSize();

  // Expand macro arguments that are parseable as expressions.
  if (parse_status_.ok() && SyntaxTree() != nullptr) {
    ExpandMacroCallArgExpressions();
  }

  return parse_status_;
}

namespace {
using verible::MutableTreeVisitorRecursive;
using verible::SymbolPtr;
using verible::SyntaxTreeLeaf;
using verible::SyntaxTreeNode;
using verible::TextStructureView;
using verible::TokenInfo;

// Helper class to replace macro call argument nodes with expression trees.
class MacroCallArgExpander : public MutableTreeVisitorRecursive {
 public:
  explicit MacroCallArgExpander(absl::string_view text) : full_text_(text) {}

  void Visit(const SyntaxTreeNode&, SymbolPtr*) override {}

  void Visit(const SyntaxTreeLeaf& leaf, SymbolPtr* leaf_owner) override {
    const TokenInfo& token(leaf.get());
    if (token.token_enum == MacroArg) {
      VLOG(3) << "MacroCallArgExpander: examining token: " << token;
      // Attempt to parse text as an expression.
      std::unique_ptr<VerilogAnalyzer> expr_analyzer =
          AnalyzeVerilogExpression(token.text, "<macro-arg-expander>");
      if (!expr_analyzer->ParseStatus().ok()) {
        // If that failed, try to parse text as a property.
        expr_analyzer = AnalyzeVerilogPropertySpec(
            token.text, "<macro-arg-expander>");
        if (!expr_analyzer->ParseStatus().ok()) {
          // If that failed: try to infer parsing mode from comments
          expr_analyzer = VerilogAnalyzer::AnalyzeAutomaticMode(
              token.text, "<macro-arg-expander>");
        }
      }
      if (ABSL_DIE_IF_NULL(expr_analyzer)->LexStatus().ok() &&
          expr_analyzer->ParseStatus().ok()) {
        VLOG(3) << "  ... content is parse-able, saving for expansion.";
        const auto& token_sequence = expr_analyzer->Data().TokenStream();
        const verible::TokenInfo::Context token_context{
            expr_analyzer->Data().Contents(), [](std::ostream& stream, int e) {
              stream << verilog_symbol_name(e);
            }};
        if (VLOG_IS_ON(4)) {
          LOG(INFO) << "macro call-arg's lexed tokens: ";
          for (const auto& t : token_sequence) {
            LOG(INFO) << verible::TokenWithContext{t, token_context};
          }
        }
        CHECK_EQ(token_sequence.back().right(expr_analyzer->Data().Contents()),
                 token.text.length());
        // Defer in-place expansion until all expansions have been collected
        // (for efficiency, avoiding inserting into middle of a vector,
        // and causing excessive reallocation).
        TextStructureView::DeferredExpansion& analysis_slot =
            InsertKeyOrDie(&subtrees_to_splice_, token.left(full_text_));
        CHECK_EQ(analysis_slot.subanalysis.get(), nullptr)
            << "Cannot expand the same location twice.  Token: " << token;
        analysis_slot.expansion_point = leaf_owner;
        analysis_slot.subanalysis = std::move(expr_analyzer);
      } else {
        // Ignore parse failures.
        VLOG(3) << "Ignoring parsing failure: " << token;
      }
    }
  }

  MacroCallArgExpander(const MacroCallArgExpander&) = delete;
  MacroCallArgExpander(MacroCallArgExpander&&) = delete;
  MacroCallArgExpander& operator=(const MacroCallArgExpander&) = delete;

  // Process accumulated DeferredExpansions.
  void ExpandSubtrees(VerilogAnalyzer* analyzer) {
    if (!subtrees_to_splice_.empty()) {
      analyzer->MutableData().ExpandSubtrees(&subtrees_to_splice_);
    }
  }

 private:
  // Deferred set of syntax tree nodes to expand.
  // Key: location.
  // Value: substring analysis results.
  TextStructureView::NodeExpansionMap subtrees_to_splice_;

  // Full text from which tokens were lexed, for calculating byte offsets.
  absl::string_view full_text_;
};

}  // namespace

void VerilogAnalyzer::ExpandMacroCallArgExpressions() {
  VLOG(2) << __FUNCTION__;
  MacroCallArgExpander expander(Data().Contents());
  ABSL_DIE_IF_NULL(SyntaxTree())
      ->Accept(&expander, &MutableData().MutableSyntaxTree());
  expander.ExpandSubtrees(this);
  VLOG(2) << "end of " << __FUNCTION__;
}

bool LexicallyEquivalent(const TokenSequence& left, const TokenSequence& right,
                         std::ostream* errstream) {
  // Filter out whitespaces from both token sequences.
  verible::TokenStreamView left_filtered, right_filtered;
  verible::InitTokenStreamView(left, &left_filtered);
  verible::InitTokenStreamView(right, &right_filtered);
  verible::TokenFilterPredicate not_whitespace = [](const TokenInfo& t) {
    return t.token_enum != TK_SPACE && t.token_enum != TK_NEWLINE;
  };
  verible::FilterTokenStreamViewInPlace(not_whitespace, &left_filtered);
  verible::FilterTokenStreamViewInPlace(not_whitespace, &right_filtered);

  // Compare filtered views, starting with sizes.
  const size_t l_size = left_filtered.size();
  const size_t r_size = right_filtered.size();
  const bool size_match = l_size == r_size;
  if (!size_match) {
    if (errstream != nullptr) {
      *errstream << "Mismatch in token sequence lengths: " << l_size << " vs. "
                 << r_size << std::endl;
    }
  }

  // Compare element-by-element up to the common length.
  const size_t min_size = std::min(l_size, r_size);
  auto left_filtered_stop_iter = left_filtered.begin() + min_size;
  auto mismatch_pair = std::mismatch(
      left_filtered.begin(), left_filtered_stop_iter, right_filtered.begin(),
      [](const TokenSequence::const_iterator l,
         const TokenSequence::const_iterator r) {
        // Ignore location/offset differences.
        return l->EquivalentWithoutLocation(*r);
      });
  if (mismatch_pair.first == left_filtered_stop_iter) {
    if (size_match) {
      // Lengths match, and end of both sequences reached without mismatch.
      return true;
    } else if (l_size < r_size) {
      *errstream << "First excess token in right sequence: "
                 << *right_filtered[min_size] << std::endl;
    } else {  // r_size < l_size
      *errstream << "First excess token in left sequence: "
                 << *left_filtered[min_size] << std::endl;
    }
    return false;
  }
  // else there was a mismatch
  if (errstream != nullptr) {
    const size_t mismatch_index =
        std::distance(left_filtered.begin(), mismatch_pair.first);
    const auto& left_token = **mismatch_pair.first;
    const auto& right_token = **mismatch_pair.second;
    *errstream << "First mismatched token [" << mismatch_index << "]: ("
               << verilog_symbol_name(left_token.token_enum) << ") "
               << left_token << " vs. ("
               << verilog_symbol_name(right_token.token_enum) << ") "
               << right_token << std::endl;
  }
  return false;
}

}  // namespace verilog
