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

#include "verilog/CST/verilog_tree_json.h"

#include "absl/strings/string_view.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/token_info.h"
#include "common/text/token_info_json.h"
#include "common/util/value_saver.h"
#include "verilog/CST/verilog_nonterminals.h"  // for NodeEnumToString
#include "verilog/parser/verilog_token.h"
#include "verilog/parser/verilog_token_classifications.h"

using nlohmann::json;

namespace verilog {

class VerilogTreeToJsonConverter : public verible::SymbolVisitor {
 public:
  explicit VerilogTreeToJsonConverter(absl::string_view base);

  void Visit(const verible::SyntaxTreeLeaf&) final;
  void Visit(const verible::SyntaxTreeNode&) final;

  json TakeJsonValue() { return std::move(json_); }

 protected:
  // Range of text spanned by syntax tree, used for offset calculation.
  const verible::TokenInfo::Context context_;

  // JSON tree root
  json json_;

  // Pointer to JSON value of currently visited symbol in its parent's
  // children list.
  json* value_;
};

VerilogTreeToJsonConverter::VerilogTreeToJsonConverter(absl::string_view base)
    : context_(base,
               [](std::ostream& stream, int e) {
                 stream << TokenTypeToString(static_cast<verilog_tokentype>(e));
               }),
      value_(&json_) {}

void VerilogTreeToJsonConverter::Visit(const verible::SyntaxTreeLeaf& leaf) {
  const verilog_tokentype tokentype =
      static_cast<verilog_tokentype>(leaf.Tag().tag);
  absl::string_view type_str = TokenTypeToString(tokentype);
  // Don't include token's text for operators, keywords, or anything that is a
  // part of Verilog syntax. For such types, TokenTypeToString() is equal to
  // token's text. Exception has to be made for identifiers, because things like
  // "PP_Identifier" or "SymbolIdentifier" (which are values returned by
  // TokenTypeToString()) could be used as Verilog identifier.
  const bool include_text =
      verilog::IsIdentifierLike(tokentype) || (leaf.get().text() != type_str);
  *value_ = verible::ToJson(leaf.get(), context_, include_text);
}

void VerilogTreeToJsonConverter::Visit(const verible::SyntaxTreeNode& node) {
  *value_ = json::object();
  (*value_)["tag"] = NodeEnumToString(static_cast<NodeEnum>(node.Tag().tag));
  json& children = (*value_)["children"] = json::array();

  {
    const verible::ValueSaver<json*> value_saver(&value_, nullptr);
    for (const auto& child : node.children()) {
      value_ = &children.emplace_back(nullptr);
      // nullptrs from children list are intentionally preserved in JSON as
      // `null` values.
      if (child) child->Accept(this);
    }
  }
}

json ConvertVerilogTreeToJson(const verible::Symbol& root,
                              absl::string_view base) {
  VerilogTreeToJsonConverter converter(base);
  root.Accept(&converter);
  return converter.TakeJsonValue();
}

}  // namespace verilog
