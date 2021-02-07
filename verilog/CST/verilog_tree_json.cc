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
#include "json/json.h"
#include "verilog/CST/verilog_nonterminals.h"  // for NodeEnumToString
#include "verilog/parser/verilog_token.h"

namespace verilog {

class VerilogTreeToJsonConverter : public verible::SymbolVisitor {
 public:
  explicit VerilogTreeToJsonConverter(absl::string_view base);

  void Visit(const verible::SyntaxTreeLeaf&) override;
  void Visit(const verible::SyntaxTreeNode&) override;

  Json::Value TakeJsonValue() { return std::move(json_); }

 protected:
  // Range of text spanned by syntax tree, used for offset calculation.
  const verible::TokenInfo::Context context_;

  // JSON tree root
  Json::Value json_;

  // Pointer to JSON value of currently visited symbol in its parent's
  // children list.
  Json::Value* value_;
};

VerilogTreeToJsonConverter::VerilogTreeToJsonConverter(absl::string_view base)
    : context_(base,
               [](std::ostream& stream, int e) {
                 stream << TokenTypeToString(static_cast<verilog_tokentype>(e));
               }),
      json_(Json::objectValue),
      value_(&json_) {}

void VerilogTreeToJsonConverter::Visit(const verible::SyntaxTreeLeaf& leaf) {
  *value_ = verible::ToJson(leaf.get(), context_);
}

void VerilogTreeToJsonConverter::Visit(const verible::SyntaxTreeNode& node) {
  *value_ = Json::objectValue;
  (*value_)["tag"] = NodeEnumToString(static_cast<NodeEnum>(node.Tag().tag));
  Json::Value& children = (*value_)["children"] = Json::arrayValue;
  children.resize(node.children().size());

  {
    const verible::ValueSaver<Json::Value*> value_saver(&value_, nullptr);
    unsigned child_rank = 0;
    for (const auto& child : node.children()) {
      value_ = &children[child_rank];
      // nullptrs from children list are intentionally preserved in JSON as
      // `null` values.
      if (child) child->Accept(this);
      ++child_rank;
    }
  }
}

Json::Value ConvertVerilogTreeToJson(const verible::Symbol& root,
                                     absl::string_view base) {
  VerilogTreeToJsonConverter converter(base);
  root.Accept(&converter);
  return converter.TakeJsonValue();
}

}  // namespace verilog
