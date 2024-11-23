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

// SyntaxTreeLeaf class is a wrapper for TokenInfo. It inherits from Symbol
// to allow placing TokenInfo's into a tree structure

#ifndef VERIBLE_COMMON_TEXT_CONCRETE_SYNTAX_LEAF_H_
#define VERIBLE_COMMON_TEXT_CONCRETE_SYNTAX_LEAF_H_

#include <iosfwd>
#include <utility>

#include "verible/common/text/symbol.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-compare.h"
#include "verible/common/text/visitors.h"

namespace verible {

class SyntaxTreeLeaf final : public Symbol {
 public:
  SyntaxTreeLeaf() = delete;

  explicit SyntaxTreeLeaf(const TokenInfo &token) : token_(token) {}

  // All passed arguments will be forwarded to T's constructor
  template <typename... Args>
  explicit SyntaxTreeLeaf(Args &&...args)
      : token_(std::forward<Args>(args)...) {}

  const TokenInfo &get() const { return token_; }

  TokenInfo *get_mutable() { return &token_; }

  // Compares this to an arbitrary symbol using compare_tokens
  bool equals(const Symbol *symbol,
              const TokenComparator &compare_tokens) const final;

  // Compares this to another leaf using compare_tokens
  bool equals(const SyntaxTreeLeaf *leaf,
              const TokenComparator &compare_tokens) const;

  // The Accept() methods have the visitor visit this leaf and perform no
  // other actions.
  void Accept(TreeVisitorRecursive *visitor) const final;
  void Accept(SymbolVisitor *visitor) const final;
  void Accept(MutableTreeVisitorRecursive *visitor,
              SymbolPtr *this_owned) final;

  // Method override that returns the Kind of SyntaxTreeLeaf
  SymbolKind Kind() const final { return SymbolKind::kLeaf; }
  SymbolTag Tag() const final { return LeafTag(get().token_enum()); }

 private:
  TokenInfo token_;
};

std::ostream &operator<<(std::ostream &os, const SyntaxTreeLeaf &l);

}  // namespace verible

#endif  // VERIBLE_COMMON_TEXT_CONCRETE_SYNTAX_LEAF_H_
