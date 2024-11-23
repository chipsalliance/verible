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

#ifndef VERIBLE_COMMON_ANALYSIS_MATCHER_MATCHER_H_
#define VERIBLE_COMMON_ANALYSIS_MATCHER_MATCHER_H_

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/text/symbol.h"

namespace verible {
namespace matcher {

// Forward declaration of Matcher class
class Matcher;

using SymbolPredicate = std::function<bool(const Symbol &)>;
using SymbolTransformer =
    std::function<std::vector<const Symbol *>(const Symbol &)>;

// Manages recursion on symbol for inner_matchers
using InnerMatchHandler = std::function<bool(
    const Symbol &symbol, const std::vector<Matcher> &inner_matchers,
    BoundSymbolManager *manager)>;

// Matcher provides an interface for creating nested tree pattern matchers.
//
// Usage:
//  Matcher matcher(some_symbol_predicate, some_match_handler);
//  matcher.AddMatchers(... some children matchers ...);
//
//  BoundSymbolManager manager;
//
//  matcher.Matches(...some node..., &manager);
//
// Modeled after Clang ASTMatcher's Matcher class
// See ASTMatchersInternal.h, class Matcher
//
class Matcher {
 public:
  Matcher(const SymbolPredicate &p, const InnerMatchHandler &handler)
      : predicate_(p), inner_match_handler_(handler) {}

  Matcher(const SymbolPredicate &p, const InnerMatchHandler &handler,
          const SymbolTransformer &t)
      : predicate_(p), inner_match_handler_(handler), transformer_(t) {}

  // Returns true if this and all submatchers match on symbol.
  // Returns false otherwise.
  // If this and all submatchers match, adds their bound symbols to manager
  // If bind_id_ is not nullopt, then bind symbol to bind_id_
  // TODO(jeremycs): implement match branching behavior here.
  bool Matches(const Symbol &symbol, BoundSymbolManager *manager) const;

  // No-op case for variadic AddMatcher.
  void AddMatchers() const {}

  // Adds an arbitrary number of matchers to this as submatchers
  template <typename... Args>
  void AddMatchers(const Matcher &matcher, Args &&...args) {
    inner_matchers_.push_back(matcher);
    AddMatchers(std::forward<Args>(args)...);
  }

 private:
  // Contains all inner matchers.
  std::vector<Matcher> inner_matchers_;

 protected:
  // Determines whether or not this matches against a given symbol.
  SymbolPredicate predicate_;

  // Define the recursion strategy that is used for traversing inner matchers.
  InnerMatchHandler inner_match_handler_;

  // This transformation is applied to a matched symbol before it is passed
  // to inner matchers.
  // Default transformation does not modify symbol.
  SymbolTransformer transformer_ =
      [](const Symbol &symbol) -> std::vector<const Symbol *> {
    return {&symbol};
  };

  // If present when Matches is called, symbol will be bound to its value
  // If null_opt, then symbol will not be
  absl::optional<std::string> bind_id_ = absl::nullopt;
};

// BindableMatcher is a subclass of matcher that enables setting
// bind_id_.
//
// This class allows the accompanying DSL to restrict which matchers are
// allowed to bind symbols.
//
// bind_id_ should not be allowed to be set in the general case. This is
// because there are many types matchers for which binding to an id does not
// make sense. For instance, AnyOf/AllOf.
//
// Usage:
//  auto matcher = Matcher(some_symbol_predicate,
//                         some_match_handler).Bind("my-id")
//  BoundSymbolManager manager;
//  bool matched = matcher.Matches(...some node..., &manager);
//
//  If matched is true, then manager will contain a node bound to "my-id"
//
class BindableMatcher : public Matcher {
 public:
  // Inherit constructors from Matcher
  using Matcher::Matcher;

  BindableMatcher &Bind(const std::string &id) {
    bind_id_ = id;
    return *this;
  }
};

}  // namespace matcher
}  // namespace verible

#endif  // VERIBLE_COMMON_ANALYSIS_MATCHER_MATCHER_H_
