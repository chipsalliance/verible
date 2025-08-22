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

#include "verible/verilog/analysis/checkers/port-name-suffix-rule.h"

#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "absl/strings/str_split.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/token-info.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/port.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {

using verible::LintRuleStatus;
using verible::LintViolation;
using verible::Symbol;
using verible::SyntaxTreeContext;
using verible::TokenInfo;
using verible::matcher::Matcher;

// Register PortNameSuffixRule.
VERILOG_REGISTER_LINT_RULE(PortNameSuffixRule);

static constexpr std::string_view kMessageIn =
    "input port names must end with _i, _ni or _pi";
static constexpr std::string_view kMessageOut =
    "output port names must end with _o, _no, or _po";
static constexpr std::string_view kMessageInOut =
    "inout port names must end with _io, _nio or _pio";

const LintRuleDescriptor &PortNameSuffixRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "port-name-suffix",
      .topic = "suffixes-for-signals-and-types",
      .desc =
          "Check that port names end with _i for inputs, _o for outputs and "
          "_io for inouts. "
          "Alternatively, for active-low signals use _n[io], for differential "
          "pairs use _n[io] and _p[io].",
  };
  return d;
}

static const Matcher &PortMatcher() {
  static const Matcher matcher(NodekPortDeclaration());
  return matcher;
}

void PortNameSuffixRule::Violation(std::string_view direction,
                                   const TokenInfo &token,
                                   const SyntaxTreeContext &context) {
  if (direction == "input") {
    violations_.insert(LintViolation(token, kMessageIn, context));
  } else if (direction == "output") {
    violations_.insert(LintViolation(token, kMessageOut, context));
  } else if (direction == "inout") {
    violations_.insert(LintViolation(token, kMessageInOut, context));
  }
}

bool PortNameSuffixRule::IsSuffixCorrect(std::string_view suffix,
                                         std::string_view direction) {
  static const std::map<std::string_view, std::set<std::string_view>> suffixes =
      {{"input", {"i", "ni", "pi"}},
       {"output", {"o", "no", "po"}},
       {"inout", {"io", "nio", "pio"}}};

  // At this point it is guaranteed that the direction will be set to
  // one of the expected values (used as keys in the map above).
  // Therefore checking the suffix like this is safe
  return suffixes.at(direction).count(suffix) == 1;
}

void PortNameSuffixRule::HandleSymbol(const Symbol &symbol,
                                      const SyntaxTreeContext &context) {
  constexpr std::string_view implicit_direction = "input";
  verible::matcher::BoundSymbolManager manager;
  if (PortMatcher().Matches(symbol, &manager)) {
    const auto *identifier_leaf = GetIdentifierFromPortDeclaration(symbol);
    const auto *direction_leaf = GetDirectionFromPortDeclaration(symbol);
    const auto token = identifier_leaf->get();
    std::string direction;
    if (direction_leaf) {
      direction = std::string(direction_leaf->get().text());
    } else {
      // Non-ANSI style: try to find direction inside module body
      // Search parent for matching port declaration with direction
      const SyntaxTreeContext *parent_ctx = &context;
      while (parent_ctx && !direction_leaf) {
        // Iterate symbols in parent context to find matching identifier
        for (const auto &sym : *parent_ctx) {
          if (sym->Kind() == SymbolKind::kNode) {
            const auto *decl_dir = GetDirectionFromPortDeclaration(*sym);
            const auto *decl_id = GetIdentifierFromPortDeclaration(*sym);
            if (decl_id && decl_id->get().text() == identifier_leaf->get().text()) {
              if (decl_dir) {
                direction = std::string(decl_dir->get().text());
                break;
              }
            }
          }
        }
        parent_ctx = parent_ctx->parent();
      }
      if (direction.empty()) direction = std::string(implicit_direction);
    }

    const auto name = ABSL_DIE_IF_NULL(identifier_leaf)->get().text();

    // Check if there is any suffix
    std::vector<std::string> name_parts =
        absl::StrSplit(name, '_', absl::SkipEmpty());

    if (name_parts.size() < 2) {
      // No suffix at all
      Violation(direction, token, context);
    }

    if (!IsSuffixCorrect(name_parts.back(), direction)) {
      Violation(direction, token, context);
    }
  }
}

LintRuleStatus PortNameSuffixRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
