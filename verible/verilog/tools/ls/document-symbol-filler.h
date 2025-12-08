// Copyright 2021 The Verible Authors.
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

#ifndef VERILOG_TOOLS_LS_DOCUMENT_SYMBOL_FILLER_H
#define VERILOG_TOOLS_LS_DOCUMENT_SYMBOL_FILLER_H

#include "verible/common/lsp/lsp-protocol.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/visitors.h"

namespace verilog {
class DocumentSymbolFiller : public verible::SymbolVisitor {
 public:
  // Create a visitor that fills the passed language server protocol
  // DocumentSymbol structure.
  // The "kate_workaround" changes some emitted node-types as Kate can't
  // deal with all of them (todo: fix in kate).
  DocumentSymbolFiller(bool kate_workaround, bool include_variables,
                       const verible::TextStructureView &text,
                       verible::lsp::DocumentSymbol *toplevel);

 protected:
  void Visit(const verible::SyntaxTreeLeaf &leaf) final;
  void Visit(const verible::SyntaxTreeNode &node) final;

 private:
  verible::lsp::Range RangeFromLeaf(const verible::SyntaxTreeLeaf &leaf) const;
  verible::lsp::Range RangeFromToken(const verible::TokenInfo &token) const;

  const int kModuleSymbolKind;  // Might be different if kate-workaround.
  const int kBlockSymbolKind;
  bool include_variables;
  const verible::TextStructureView &text_view_;
  verible::lsp::DocumentSymbol *current_symbol_;
};
}  // namespace verilog
#endif  // VERILOG_TOOLS_LS_DOCUMENT_SYMBOL_FILLER_H
