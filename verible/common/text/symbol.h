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

// Symbol is a base class that covers both terminal and nonterminal symbols.
// See bison_parser_common.h for sample usage.

#ifndef VERIBLE_COMMON_TEXT_SYMBOL_H_
#define VERIBLE_COMMON_TEXT_SYMBOL_H_

#include <functional>
#include <iosfwd>

#include "verible/common/text/symbol-ptr.h"  // IWYU pragma: export
#include "verible/common/text/token-info.h"
#include "verible/common/text/visitors.h"

namespace verible {
using TokenComparator =
    std::function<bool(const TokenInfo &, const TokenInfo &)>;

// Kind is a datatype representing the subclass of a Symbol*
enum class SymbolKind { kLeaf, kNode };

std::ostream &operator<<(std::ostream &, SymbolKind);

// Pair that identifies a tree symbol (leaf or node).
struct SymbolTag {
  SymbolKind kind;
  int tag;

  bool operator==(const SymbolTag &symbol_tag) const {
    return kind == symbol_tag.kind && tag == symbol_tag.tag;
  }

  bool operator!=(const SymbolTag &symbol_tag) const {
    return !(*this == symbol_tag);
  }
};

// Pair of inline helper functions for building SymbolTag
template <typename EnumType>
constexpr SymbolTag NodeTag(EnumType tag) {
  return {SymbolKind::kNode, static_cast<int>(tag)};
}
constexpr SymbolTag LeafTag(int tag) { return {SymbolKind::kLeaf, tag}; }

// forward declare Visitor classes to allow references in Symbol

class Symbol {
 public:
  virtual ~Symbol() = default;

  virtual bool equals(const Symbol *symbol,
                      const TokenComparator &compare_tokens) const = 0;

  // Visitor pattern methods
  virtual void Accept(TreeVisitorRecursive *visitor) const = 0;
  virtual void Accept(SymbolVisitor *visitor) const = 0;

  // The MutableTreeVisitorRecursive overload of Accept takes an extra
  // SymbolPtr that is the owning pointer to this, so that it can be potentially
  // deleted or replaced or transferred in a mutating pass.
  virtual void Accept(MutableTreeVisitorRecursive *visitor,
                      SymbolPtr *this_owned) = 0;

  // Implemented in subclasses to denote their type
  virtual SymbolKind Kind() const = 0;
  virtual SymbolTag Tag() const = 0;

 protected:
  Symbol() = default;
};

}  // namespace verible

#endif  // VERIBLE_COMMON_TEXT_SYMBOL_H_
