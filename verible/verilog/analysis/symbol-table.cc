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

#include "verible/verilog/analysis/symbol-table.h"

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <sstream>
#include <stack>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "verible/common/strings/display-utils.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-compare.h"
#include "verible/common/text/tree-context-visitor.h"
#include "verible/common/text/tree-utils.h"
#include "verible/common/text/visitors.h"
#include "verible/common/util/casts.h"
#include "verible/common/util/enum-flags.h"
#include "verible/common/util/logging.h"
#include "verible/common/util/spacer.h"
#include "verible/common/util/tree-operations.h"
#include "verible/common/util/value-saver.h"
#include "verible/verilog/CST/class.h"
#include "verible/verilog/CST/declaration.h"
#include "verible/verilog/CST/functions.h"
#include "verible/verilog/CST/macro.h"
#include "verible/verilog/CST/module.h"
#include "verible/verilog/CST/net.h"
#include "verible/verilog/CST/package.h"
#include "verible/verilog/CST/parameters.h"
#include "verible/verilog/CST/port.h"
#include "verible/verilog/CST/seq-block.h"
#include "verible/verilog/CST/statement.h"
#include "verible/verilog/CST/tasks.h"
#include "verible/verilog/CST/type.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/verilog-project.h"
#include "verible/verilog/parser/verilog-parser.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {

using verible::AutoTruncate;
using verible::StringSpanOfSymbol;
using verible::SyntaxTreeLeaf;
using verible::SyntaxTreeNode;
using verible::TokenInfo;
using verible::TreeContextVisitor;
using verible::ValueSaver;

// Returns string_view of `text` with outermost double-quotes removed.
// If `text` is not wrapped in quotes, return it as-is.
static absl::string_view StripOuterQuotes(absl::string_view text) {
  return absl::StripSuffix(absl::StripPrefix(text, "\""), "\"");
}

static const verible::EnumNameMap<SymbolMetaType> &SymbolMetaTypeNames() {
  static const verible::EnumNameMap<SymbolMetaType> kSymbolMetaTypeNames({
      // short-hand annotation for identifier reference type
      {"<root>", SymbolMetaType::kRoot},
      {"class", SymbolMetaType::kClass},
      {"module", SymbolMetaType::kModule},
      {"package", SymbolMetaType::kPackage},
      {"parameter", SymbolMetaType::kParameter},
      {"typedef", SymbolMetaType::kTypeAlias},
      {"data/net/var/instance", SymbolMetaType::kDataNetVariableInstance},
      {"function", SymbolMetaType::kFunction},
      {"task", SymbolMetaType::kTask},
      {"struct", SymbolMetaType::kStruct},
      {"enum", SymbolMetaType::kEnumType},
      {"<enum constant>", SymbolMetaType::kEnumConstant},
      {"interface", SymbolMetaType::kInterface},
      {"<unspecified>", SymbolMetaType::kUnspecified},
      {"<callable>", SymbolMetaType::kCallable},
  });
  return kSymbolMetaTypeNames;
}

std::ostream &operator<<(std::ostream &stream, SymbolMetaType symbol_type) {
  return SymbolMetaTypeNames().Unparse(symbol_type, stream);
}

absl::string_view SymbolMetaTypeAsString(SymbolMetaType type) {
  return SymbolMetaTypeNames().EnumName(type);
}

// Root SymbolTableNode has no key, but we identify it as "$root"
static constexpr absl::string_view kRoot("$root");

std::ostream &SymbolTableNodeFullPath(std::ostream &stream,
                                      const SymbolTableNode &node) {
  if (node.Parent() != nullptr) {
    SymbolTableNodeFullPath(stream, *node.Parent()) << "::" << *node.Key();
  } else {
    stream << kRoot;
  }
  return stream;
}

static std::string ContextFullPath(const SymbolTableNode &context) {
  std::ostringstream stream;
  SymbolTableNodeFullPath(stream, context);
  return stream.str();
}

std::ostream &ReferenceNodeFullPath(std::ostream &stream,
                                    const ReferenceComponentNode &node) {
  if (node.Parent() != nullptr) {
    ReferenceNodeFullPath(stream, *node.Parent());  // recursive
  }
  return node.Value().PrintPathComponent(stream);
}

static std::string ReferenceNodeFullPathString(
    const ReferenceComponentNode &node) {
  std::ostringstream stream;
  ReferenceNodeFullPath(stream, node);
  return stream.str();
}

static std::ostream &operator<<(std::ostream &stream,
                                const ReferenceComponentNode &ref_node) {
  PrintTree(ref_node, &stream,
            [](std::ostream &s, const ReferenceComponent &ref_comp)
                -> std::ostream & { return s << ref_comp; });
  return stream;
}

// Validates iterator/pointer stability when appending new child.
// Detects unwanted reallocation.
static ReferenceComponentNode *CheckedNewChildReferenceNode(
    ReferenceComponentNode *parent, const ReferenceComponent &component) {
  auto &siblings = parent->Children();
  if (!siblings.empty()) {
    CHECK_LT(siblings.size(), siblings.capacity())
        << "\nReallocation would invalidate pointers to reference nodes at:\n"
        << *parent << "\nWhile attempting to add child:\n"
        << component << "\nFix: pre-allocate child nodes.";
  }
  // Otherwise, this first node had no prior siblings, so no need to check.
  siblings.emplace_back(component);  // copy
  return &siblings.back();
}

static absl::Status DiagnoseMemberSymbolResolutionFailure(
    absl::string_view name, const SymbolTableNode &context) {
  const absl::string_view context_name =
      context.Parent() == nullptr ? kRoot : *context.Key();
  return absl::NotFoundError(
      absl::StrCat("No member symbol \"", name, "\" in parent scope (",
                   SymbolMetaTypeAsString(context.Value().metatype), ") ",
                   context_name, "."));
}

static const SymbolTableNode *LookupSymbolUpwards(
    const SymbolTableNode &context, absl::string_view symbol);

class SymbolTable::Builder : public TreeContextVisitor {
 public:
  Builder(const VerilogSourceFile &source, SymbolTable *symbol_table,
          VerilogProject *project)
      : source_(&source),
        token_context_(MakeTokenContext()),
        symbol_table_(symbol_table),
        current_scope_(&symbol_table_->MutableRoot()) {}

  std::vector<absl::Status> TakeDiagnostics() {
    return std::move(diagnostics_);
  }

 private:  // methods
  void Visit(const SyntaxTreeNode &node) final {
    const auto tag = static_cast<NodeEnum>(node.Tag().tag);
    VLOG(2) << __FUNCTION__ << " [node]: " << tag;
    switch (tag) {
      case NodeEnum::kModuleDeclaration:
        DeclareModule(node);
        break;
      case NodeEnum::kGenerateIfClause:
        DeclareGenerateIf(node);
        break;
      case NodeEnum::kGenerateElseClause:
        DeclareGenerateElse(node);
        break;
      case NodeEnum::kPackageDeclaration:
        DeclarePackage(node);
        break;
      case NodeEnum::kClassDeclaration:
        DeclareClass(node);
        break;
      case NodeEnum::kInterfaceDeclaration:
        DeclareInterface(node);
        break;
      case NodeEnum::kFunctionPrototype:  // fall-through
      case NodeEnum::kFunctionDeclaration:
        DeclareFunction(node);
        break;
      case NodeEnum::kFunctionHeader:
        SetupFunctionHeader(node);
        break;
      case NodeEnum::kClassConstructorPrototype:
        DeclareConstructor(node);
        break;
      case NodeEnum::kTaskPrototype:  // fall-through
      case NodeEnum::kTaskDeclaration:
        DeclareTask(node);
        break;
        // No special handling needed for kTaskHeader
      case NodeEnum::kPortList:
        DeclarePorts(node);
        break;
      case NodeEnum::kModulePortDeclaration:
      case NodeEnum::kPortItem:           // fall-through
                                          // for function/task parameters
      case NodeEnum::kPortDeclaration:    // fall-through
      case NodeEnum::kNetDeclaration:     // fall-through
      case NodeEnum::kStructUnionMember:  // fall-through
      case NodeEnum::kTypeDeclaration:    // fall-through
      case NodeEnum::kDataDeclaration:
        DeclareData(node);
        break;
      case NodeEnum::kParamDeclaration:
        DeclareParameter(node);
        break;
      case NodeEnum::kTypeInfo:  // fall-through
      case NodeEnum::kDataType:
        DescendDataType(node);
        break;
      case NodeEnum::kReference:
      case NodeEnum::kReferenceCallBase:
        DescendReferenceExpression(node);
        break;
      case NodeEnum::kActualParameterList:
        DescendActualParameterList(node);
        break;
      case NodeEnum::kPortActualList:
        DescendPortActualList(node);
        break;
      case NodeEnum::kArgumentList:
        DescendCallArgumentList(node);
        break;
      case NodeEnum::kGateInstanceRegisterVariableList: {
        // TODO: reserve() to guarantee pointer/iterator stability in VectorTree
        Descend(node);
        break;
      }
      case NodeEnum::kNetVariable:
        DeclareNet(node);
        break;
      case NodeEnum::kRegisterVariable:
        DeclareRegister(node);
        break;
      case NodeEnum::kGateInstance:
        DeclareInstance(node);
        break;
      case NodeEnum::kVariableDeclarationAssignment:
        DeclareVariable(node);
        break;
      case NodeEnum::kQualifiedId:
        HandleQualifiedId(node);
        break;
      case NodeEnum::kPreprocessorInclude:
        EnterIncludeFile(node);
        break;
      case NodeEnum::kExtendsList:
        DescendExtends(node);
        break;
      case NodeEnum::kStructType:
        DescendStructType(node);
        break;
      case NodeEnum::kEnumType:
        DescendEnumType(node);
        break;
      case NodeEnum::kLPValue:
        HandlePossibleImplicitDeclaration(node);
        break;
      case NodeEnum::kBindDirective:
        // TODO(#1241) Not handled right now.
        // TODO(#1255) Not handled right now.
        break;
      default:
        Descend(node);
        break;
    }
    VLOG(2) << "end of " << __FUNCTION__ << " [node]: " << tag;
  }

  // This overload enters 'scope' for the duration of the call.
  // New declared symbols will belong to that scope.
  void Descend(const SyntaxTreeNode &node, SymbolTableNode *scope) {
    const ValueSaver<SymbolTableNode *> save_scope(&current_scope_, scope);
    Descend(node);
  }

  void Descend(const SyntaxTreeNode &node) {
    TreeContextVisitor::Visit(node);  // maintains syntax tree Context() stack.
  }

  // RAII-class balance the Builder::references_builders_ stack.
  // The work of moving collecting references into the current scope is done in
  // the destructor.
  class CaptureDependentReference {
   public:
    explicit CaptureDependentReference(Builder *builder)
        : builder_(builder),
          saved_branch_point_(builder_->reference_branch_point_) {
      // Push stack space to capture references.
      builder_->reference_builders_.emplace(/* DependentReferences */);
      // Reset the branch point to start new named parameter/port chains
      // from the same context.
      builder_->reference_branch_point_ = nullptr;
    }

    ~CaptureDependentReference() {
      // This completes the capture of a chain of dependent references.
      // Ref() can be empty if the subtree doesn't reference any identifiers.
      // Empty refs are non-actionable and must be excluded.
      DependentReferences &ref(Ref());
      if (!ref.Empty()) {
        builder_->current_scope_->Value().local_references_to_bind.emplace_back(
            std::move(ref));
      }
      builder_->reference_builders_.pop();
      builder_->reference_branch_point_ = saved_branch_point_;  // restore
    }

    // Returns the chain of dependent references that were built.
    DependentReferences &Ref() const {
      return builder_->reference_builders_.top();
    }

   private:
    Builder *builder_;
    ReferenceComponentNode *saved_branch_point_;
  };

  void DescendReferenceExpression(const SyntaxTreeNode &reference) {
    // capture expressions referenced from the current scope
    if (!Context().DirectParentIs(NodeEnum::kReferenceCallBase)) {
      const CaptureDependentReference capture(this);
      // subexpressions' references will be collected before this one
      Descend(reference);  // no scope change
    } else {
      // subexpressions' references will be collected before this one
      Descend(reference);  // no scope change
    }
  }

  void DescendExtends(const SyntaxTreeNode &extends) {
    VLOG(2) << __FUNCTION__ << " from: " << CurrentScopeFullPath();
    {
      // At this point we are already inside the scope of the class declaration,
      // however, the base classes should be resolved starting from the scope
      // that *contains* this class declaration.
      const ValueSaver<SymbolTableNode *> save(&current_scope_,
                                               current_scope_->Parent());

      // capture the one base class type referenced by 'extends'
      const CaptureDependentReference capture(this);
      Descend(extends);
    }

    // Link this new type reference as the base type of the current class being
    // declared.
    const DependentReferences &recent_ref =
        current_scope_->Parent()->Value().local_references_to_bind.back();
    const ReferenceComponentNode *base_type_ref =
        recent_ref.LastTypeComponent();
    SymbolInfo &current_declared_class_info = current_scope_->Value();
    current_declared_class_info.parent_type.user_defined_type = base_type_ref;
  }

  // Traverse a subtree for a data type and collects type references
  // originating from the current context.
  // If the context is such that this type is used in a declaration,
  // then capture that type information to be used later.
  //
  // The state/stack management here is intended to accommodate type references
  // of arbitrary complexity.
  // A generalized type could look like:
  //   "A#(.B(1))::C#(.D(E#(.F(0))))::G"
  // This should produce the following reference trees:
  //   A -+- ::B
  //      |
  //      \- ::C -+- ::D
  //              |
  //              \- ::G
  //   E -+- ::F
  //
  void DescendDataType(const SyntaxTreeNode &data_type_node) {
    VLOG(2) << __FUNCTION__ << ": " << StringSpanOfSymbol(data_type_node);
    const CaptureDependentReference capture(this);

    {
      // Inform that named parameter identifiers will yield parallel children
      // from this reference branch point.  Start this out as nullptr, and set
      // it once an unqualified identifier is encountered that starts a
      // reference tree.
      const ValueSaver<ReferenceComponentNode *> set_branch(
          &reference_branch_point_, nullptr);

      Descend(data_type_node);
      // declaration_type_info_ will be restored after this closes.
    }

    if (declaration_type_info_ != nullptr) {
      // 'declaration_type_info_' holds the declared type we want to capture.
      if (verible::GetLeftmostLeaf(data_type_node) != nullptr) {
        declaration_type_info_->syntax_origin = &data_type_node;
        // Otherwise, if the type subtree contains no leaves (e.g. implicit or
        // void), then do not assign a syntax origin.
        // StringSpanOfSymbol(*declaration_type_info_->syntax_origin) <<
        // std::endl;
      }

      const DependentReferences &type_ref(capture.Ref());
      if (!type_ref.Empty()) {
        // then some user-defined type was referenced
        declaration_type_info_->user_defined_type =
            type_ref.LastTypeComponent();
      }
      VLOG(3) << "declared type: " << *declaration_type_info_;
    }

    // In all cases, a type is being referenced from the current scope, so add
    // it to the list of references to resolve (done by 'capture').
    VLOG(2) << "end of " << __FUNCTION__;
  }

  void DescendActualParameterList(const SyntaxTreeNode &node) {
    if (reference_branch_point_ != nullptr) {
      // Pre-allocate siblings to guarantee pointer/iterator stability.
      // FindAll* will also catch actual port connections inside preprocessing
      // conditionals.
      const size_t num_params = FindAllNamedParams(node).size();
      // +1 to accommodate the slot needed for a nested type reference
      // e.g. for "B" in "A#(.X(), .Y(), ...)::B"
      reference_branch_point_->Children().reserve(num_params + 1);
    }
    Descend(node);
  }

  void DescendPortActualList(const SyntaxTreeNode &node) {
    if (reference_branch_point_ != nullptr) {
      // Pre-allocate siblings to guarantee pointer/iterator stability.
      // FindAll* will also catch actual port connections inside preprocessing
      // conditionals.
      const size_t num_ports = FindAllActualNamedPort(node).size();
      reference_branch_point_->Children().reserve(num_ports);
    }
    Descend(node);
  }

  void DescendCallArgumentList(const SyntaxTreeNode &node) {
    if (reference_branch_point_ != nullptr) {
      // Pre-allocate siblings to guarantee pointer/iterator stability.
      // FindAll* will also catch call arguments inside preprocessing
      // conditionals.
      const size_t num_args = FindAllNamedParams(node).size();
      // In case of an anonymous instance that would be the same point,
      // so the reserve needs to check the capacity first to add the amount
      // instead of allocating just the number of places
      reference_branch_point_->Children().reserve(
          reference_branch_point_->Children().capacity() + num_args);
    }
    Descend(node);
  }

  void DescendStructType(const SyntaxTreeNode &struct_type) {
    CHECK(struct_type.MatchesTag(NodeEnum::kStructType));
    // Structs do not inherently have names, so they are all anonymous.
    // Type declarations (typedefs) create named alias elsewhere.
    const absl::string_view anon_name =
        current_scope_->Value().CreateAnonymousScope("struct");
    SymbolTableNode *new_struct = DeclareScopedElementAndDescend(
        struct_type, anon_name, SymbolMetaType::kStruct);

    // Create a self-reference to this struct type so that it can be linked
    // for declarations that use this type.
    const ReferenceComponent anon_type_ref{
        .identifier = anon_name,
        .ref_type = ReferenceType::kImmediate,
        .required_metatype = SymbolMetaType::kStruct,
        // pre-resolve this symbol immediately
        .resolved_symbol = new_struct,
    };

    const CaptureDependentReference capture(this);
    capture.Ref().PushReferenceComponent(anon_type_ref);

    if (declaration_type_info_ != nullptr) {
      declaration_type_info_->user_defined_type = capture.Ref().LastLeaf();
    }
  }

  void DescendEnumType(const SyntaxTreeNode &enum_type) {
    CHECK(enum_type.MatchesTag(NodeEnum::kEnumType));
    const absl::string_view anon_name =
        current_scope_->Value().CreateAnonymousScope("enum");
    SymbolTableNode *new_enum = DeclareScopedElementAndDescend(
        enum_type, anon_name, SymbolMetaType::kEnumType);

    const ReferenceComponent anon_type_ref{
        .identifier = anon_name,
        .ref_type = ReferenceType::kImmediate,
        .required_metatype = SymbolMetaType::kEnumType,
        // pre-resolve this symbol immediately
        .resolved_symbol = new_enum,
    };

    const CaptureDependentReference capture(this);
    capture.Ref().PushReferenceComponent(anon_type_ref);

    if (declaration_type_info_ != nullptr) {
      declaration_type_info_->user_defined_type = capture.Ref().LastLeaf();
    }

    // Iterate over enumeration constants
    for (const auto &itr : *new_enum) {
      const auto enum_constant_name = itr.first;
      const auto &symbol = itr.second;
      const auto &syntax_origin =
          *ABSL_DIE_IF_NULL(symbol.Value().syntax_origin);

      const ReferenceComponent itr_ref{
          .identifier = enum_constant_name,
          .ref_type = ReferenceType::kImmediate,
          .required_metatype = SymbolMetaType::kEnumConstant,
          // pre-resolve this symbol immediately
          .resolved_symbol = &symbol,
      };

      // CaptureDependentReference class doesn't support
      // copy constructor
      const CaptureDependentReference cap(this);
      cap.Ref().PushReferenceComponent(anon_type_ref);
      cap.Ref().PushReferenceComponent(itr_ref);

      // Create default DeclarationTypeInfo
      DeclarationTypeInfo decl_type_info;
      const ValueSaver<DeclarationTypeInfo *> save_type(&declaration_type_info_,
                                                        &decl_type_info);
      declaration_type_info_->syntax_origin = &syntax_origin;
      declaration_type_info_->user_defined_type = cap.Ref().LastLeaf();

      // Constants should be visible in current scope so we create
      // variable instances with references to enum constants
      //
      // Consider using something different than kTypeAlias here
      // (which is technically an alias for a type and not for a data/variable)
      // e.g. kConstantAlias or even kGenericAlias.
      EmplaceTypedElementInCurrentScope(syntax_origin, enum_constant_name,
                                        SymbolMetaType::kTypeAlias);
    }
  }

  void HandlePossibleImplicitDeclaration(const SyntaxTreeNode &node) {
    VLOG(2) << __FUNCTION__;

    // Only left-hand side of continuous assignment statements are allowed to
    // implicitly declare nets (LRM 6.10: Implicit declarations).
    if (Context().DirectParentsAre(
            {NodeEnum::kNetVariableAssignment, NodeEnum::kAssignmentList,
             NodeEnum::kContinuousAssignmentStatement})) {
      CHECK(node.MatchesTag(NodeEnum::kLPValue));

      DeclarationTypeInfo decl_type_info;
      const ValueSaver<DeclarationTypeInfo *> save_type(&declaration_type_info_,
                                                        &decl_type_info);
      declaration_type_info_->implicit = true;
      Descend(node);
    } else {
      Descend(node);
    }
  }

  // stores the direction of the port in the current declaration type info
  void HandleDirection(const SyntaxTreeLeaf &leaf) {
    if (!declaration_type_info_) return;
    if (Context().DirectParentIs(NodeEnum::kModulePortDeclaration) ||
        Context().DirectParentIs(NodeEnum::kPortDeclaration)) {
      declaration_type_info_->direction = leaf.get().text();
    }
  }

  void HandleIdentifier(const SyntaxTreeLeaf &leaf) {
    const absl::string_view text = leaf.get().text();
    VLOG(2) << __FUNCTION__ << ": " << text;
    VLOG(2) << "current context: " << CurrentScopeFullPath();
    if (Context().DirectParentIs(NodeEnum::kParamType)) {
      // This identifier declares a value parameter.
      EmplaceTypedElementInCurrentScope(leaf, text, SymbolMetaType::kParameter);
      return;
    }
    if (Context().DirectParentIs(NodeEnum::kTypeAssignment)) {
      // This identifier declares a type parameter.
      EmplaceElementInCurrentScope(leaf, text, SymbolMetaType::kParameter);
      return;
    }
    // If identifier is within ModulePortDeclaration, add port identifier to the
    // scope
    if (Context().DirectParentsAre(
            {NodeEnum::kUnqualifiedId, NodeEnum::kModulePortDeclaration}) ||
        Context().DirectParentsAre(
            {NodeEnum::kUnqualifiedId, NodeEnum::kIdentifierUnpackedDimensions,
             NodeEnum::kIdentifierList, NodeEnum::kModulePortDeclaration}) ||
        Context().DirectParentsAre({NodeEnum::kIdentifierUnpackedDimensions,
                                    NodeEnum::kIdentifierList,
                                    NodeEnum::kModulePortDeclaration}) ||
        Context().DirectParentsAre({NodeEnum::kIdentifierUnpackedDimensions,
                                    NodeEnum::kIdentifierUnpackedDimensionsList,
                                    NodeEnum::kModulePortDeclaration}) ||
        Context().DirectParentsAre({NodeEnum::kPortIdentifier,
                                    NodeEnum::kPortIdentifierList,
                                    NodeEnum::kModulePortDeclaration})) {
      EmplacePortIdentifierInCurrentScope(
          leaf, text, SymbolMetaType::kDataNetVariableInstance);
      // TODO(fangism): Add attributes to distinguish public ports from
      // private internals members.
      return;
    }
    // If identifier is within PortDeclaration/Port, add a typed element
    if (Context().DirectParentsAre(
            {NodeEnum::kUnqualifiedId, NodeEnum::kPortDeclaration}) ||
        Context().DirectParentsAre(
            {NodeEnum::kUnqualifiedId,
             NodeEnum::kDataTypeImplicitBasicIdDimensions,
             NodeEnum::kPortItem})) {
      EmplaceTypedElementInCurrentScope(
          leaf, text, SymbolMetaType::kDataNetVariableInstance);
      // TODO(fangism): Add attributes to distinguish public ports from
      // private internals members.
      return;
    }

    if (Context().DirectParentsAre(
            {NodeEnum::kUnqualifiedId, NodeEnum::kFunctionHeader})) {
      // We deferred adding a declared function to the current scope until this
      // point (from DeclareFunction()).
      // Note that this excludes the out-of-line definition case,
      // which is handled in DescendThroughOutOfLineDefinition().

      const SyntaxTreeNode *decl_syntax =
          Context().NearestParentMatching([](const SyntaxTreeNode &node) {
            return node.MatchesTagAnyOf(
                {NodeEnum::kFunctionDeclaration, NodeEnum::kFunctionPrototype});
          });
      if (decl_syntax == nullptr) return;
      SymbolTableNode *declared_function = &EmplaceTypedElementInCurrentScope(
          *decl_syntax, text, SymbolMetaType::kFunction);
      // After this point, we've registered the new function with its return
      // type, so we can switch context over to the newly declared function
      // for its port interface and definition internals.
      current_scope_ = declared_function;
      return;
    }

    if (Context().DirectParentIs(NodeEnum::kClassConstructorPrototype)) {
      // This is a constructor or its prototype.
      // From verilog.y, the "new" token is directly under this prototype node,
      // and the full constructor definition also contains its prototype.
      const SyntaxTreeNode *decl_syntax = &Context().top();
      SymbolTableNode *declared_function = &EmplaceTypedElementInCurrentScope(
          *decl_syntax, text, SymbolMetaType::kFunction);
      current_scope_ = declared_function;
      return;
    }

    if (Context().DirectParentsAre(
            {NodeEnum::kUnqualifiedId, NodeEnum::kTaskHeader})) {
      // We deferred adding a declared task to the current scope until this
      // point (from DeclareFunction()).
      // Note that this excludes the out-of-line definition case,
      // which is handled in DescendThroughOutOfLineDefinition().

      const SyntaxTreeNode *decl_syntax =
          Context().NearestParentMatching([](const SyntaxTreeNode &node) {
            return node.MatchesTagAnyOf(
                {NodeEnum::kTaskDeclaration, NodeEnum::kTaskPrototype});
          });
      if (decl_syntax == nullptr) return;
      SymbolTableNode *declared_task = EmplaceElementInCurrentScope(
          *decl_syntax, text, SymbolMetaType::kTask);
      // After this point, we've registered the new task,
      // so we can switch context over to the newly declared function
      // for its port interface and definition internals.
      current_scope_ = declared_task;
      return;
    }

    if (Context().DirectParentsAre({NodeEnum::kDataTypeImplicitIdDimensions,
                                    NodeEnum::kStructUnionMember})) {
      // This is a struct/union member.  Add it to the enclosing scope.
      // e.g. "foo" in "struct { int foo; }"
      EmplaceTypedElementInCurrentScope(
          leaf, text, SymbolMetaType::kDataNetVariableInstance);
      return;
    }
    if (Context().DirectParentsAre(
            {NodeEnum::kVariableDeclarationAssignment,
             NodeEnum::kVariableDeclarationAssignmentList,
             NodeEnum::kStructUnionMember})) {
      // This is part of a declaration covered by kVariableDeclarationAssignment
      // already, so do not interpret this as a reference.
      // e.g. "z" in "struct { int y, z; }"
      return;
    }

    if (Context().DirectParentsAre(
            {NodeEnum::kEnumName, NodeEnum::kEnumNameList})) {
      EmplaceTypedElementInCurrentScope(leaf, text,
                                        SymbolMetaType::kEnumConstant);
      return;
    }

    // In DeclareInstance(), we already planted a self-reference that is
    // resolved to the instance being declared.
    if (Context().DirectParentIs(NodeEnum::kGateInstance)) return;

    if (Context().DirectParentIs(NodeEnum::kTypeDeclaration)) {
      // This identifier declares a type alias (typedef).
      EmplaceTypedElementInCurrentScope(leaf, text, SymbolMetaType::kTypeAlias);
      return;
    }

    // Capture only referencing identifiers, omit declarative identifiers.
    // This is set up when traversing references, e.g. types, expressions.
    // All of the code below takes effect inside a CaptureDependentReferences
    // RAII block.

    // FIXME(jbylicki): this is the place where the handling of a and b returns
    if (reference_builders_.empty()) return;

    // Building a reference, possible part of a chain or qualified
    // reference.
    DependentReferences &ref(reference_builders_.top());

    const ReferenceComponent new_ref{
        .identifier = text,
        .ref_type = InferReferenceType(),
        .required_metatype = InferMetaType(),
    };

    // For instances' named ports, and types' named parameters,
    // add references as siblings of the same parent.
    // (Recall that instances form self-references).
    if (Context().DirectParentIsOneOf(
            {NodeEnum::kActualNamedPort, NodeEnum::kParamByName})) {
      CheckedNewChildReferenceNode(ABSL_DIE_IF_NULL(reference_branch_point_),
                                   new_ref);
      return;
    }

    // Handle possible implicit declarations here
    if (declaration_type_info_ != nullptr && declaration_type_info_->implicit) {
      const SymbolTableNode *resolved =
          LookupSymbolUpwards(*ABSL_DIE_IF_NULL(current_scope_), text);
      if (resolved == nullptr) {
        // No explicit declaration found, declare here
        SymbolTableNode &implicit_declaration =
            EmplaceTypedElementInCurrentScope(
                leaf, text, SymbolMetaType::kDataNetVariableInstance);

        const ReferenceComponent implicit_ref{
            .identifier = text,
            .ref_type = InferReferenceType(),
            .required_metatype = InferMetaType(),
            // pre-resolve
            .resolved_symbol = &implicit_declaration,
        };

        ref.PushReferenceComponent(implicit_ref);
        return;
      }
    }

    // For all other cases, grow the reference chain deeper.
    // For type references, which may contained named parameters,
    // when encountering the first unqualified reference, establish its
    // reference node as the point from which named parameter references
    // get added as siblings.
    // e.g. "A#(.B(...), .C(...))" would result in a reference tree:
    //   A -+- ::B
    //      |
    //      \- ::C
    reference_branch_point_ = ref.PushReferenceComponent(new_ref);
  }

  void Visit(const SyntaxTreeLeaf &leaf) final {
    const auto tag = leaf.Tag().tag;
    VLOG(2) << __FUNCTION__ << " [leaf]: " << VerboseToken(leaf.get());
    switch (tag) {
      case verilog_tokentype::TK_new:  // constructor name
      case verilog_tokentype::SymbolIdentifier:
        HandleIdentifier(leaf);
        break;

      case verilog_tokentype::TK_SCOPE_RES:  // "::"
      case '.':
        last_hierarchy_operator_ = &leaf.get();
        break;
      case verilog_tokentype::TK_input:
      case verilog_tokentype::TK_output:
      case verilog_tokentype::TK_inout:
      case verilog_tokentype::TK_ref:
        HandleDirection(leaf);
        break;

      default:
        // TODO(hzeller): use verilog::IsIdentifierLike() ?
        // Using that would result in some mis-classifications.
        break;
    }
    VLOG(2) << "end " << __FUNCTION__ << " [leaf]:" << VerboseToken(leaf.get());
  }

  // Distinguish between '.' and "::" hierarchy in reference components.
  ReferenceType InferReferenceType() const {
    CHECK(!reference_builders_.empty())
        << "Not currently in a reference context.";
    const DependentReferences &ref(reference_builders_.top());
    if (ref.Empty() || last_hierarchy_operator_ == nullptr) {
      // The root component is always treated as unqualified.

      // Out-of-line definitions' base/outer references must be resolved
      // immediately.
      if (Context().DirectParentsAre({NodeEnum::kUnqualifiedId,
                                      NodeEnum::kQualifiedId,  // out-of-line
                                      NodeEnum::kFunctionHeader})) {
        return ReferenceType::kImmediate;
      }
      if (Context().DirectParentsAre({NodeEnum::kUnqualifiedId,
                                      NodeEnum::kQualifiedId,  // out-of-line
                                      NodeEnum::kTaskHeader})) {
        return ReferenceType::kImmediate;
      }

      return ReferenceType::kUnqualified;
    }
    if (Context().DirectParentIs(NodeEnum::kParamByName)) {
      // Even though named parameters are referenced with ".PARAM",
      // they are branched off of a base reference that already points
      // to the type whose scope should be used, so no additional typeof()
      // indirection is needed.
      return ReferenceType::kDirectMember;
    }
    return ABSL_DIE_IF_NULL(last_hierarchy_operator_)->token_enum() == '.'
               ? ReferenceType::kMemberOfTypeOfParent
               : ReferenceType::kDirectMember;
  }

  bool QualifiedIdComponentInLastPosition() const {
    const SyntaxTreeNode *qualified_id =
        Context().NearestParentWithTag(NodeEnum::kQualifiedId);
    const SyntaxTreeNode *unqualified_id =
        Context().NearestParentWithTag(NodeEnum::kUnqualifiedId);
    return ABSL_DIE_IF_NULL(qualified_id)->back().get() == unqualified_id;
  }

  bool ExtendedCallIsLast() const {
    const SyntaxTreeNode *reference_call_base =
        Context().NearestParentWithTag(NodeEnum::kReferenceCallBase);
    if (reference_call_base == nullptr) return false;
    for (auto &child : reference_call_base->children()) {
      if (SymbolCastToNode(*child).MatchesTagAnyOf(
              {NodeEnum::kHierarchyExtension})) {
        return false;
      }
    }
    return true;
  }

  bool UnextendedCall() const {
    const SyntaxTreeNode *rcb =
        Context().NearestParentWithTag(NodeEnum::kReferenceCallBase);
    if (rcb == nullptr) return false;
    for (auto &reference : rcb->children()) {
      if (reference && SymbolCastToNode(*reference)
                           .MatchesTagAnyOf({NodeEnum::kReference})) {
        for (auto &child : SymbolCastToNode(*reference).children()) {
          if (SymbolCastToNode(*child).MatchesTagAnyOf(
                  {NodeEnum::kHierarchyExtension})) {
            return false;
          }
        }
      }
    }
    return true;
  }

  // Does the context necessitate that the symbol being referenced have a
  // particular metatype?
  SymbolMetaType InferMetaType() const {
    const DependentReferences &ref(reference_builders_.top());
    // Out-of-line definitions' base/outer references must be resolved
    // immediately to a class.
    // Member references (inner) is a function or task, depending on header
    // type.
    if (Context().DirectParentsAre({NodeEnum::kUnqualifiedId,
                                    NodeEnum::kQualifiedId,  // out-of-line
                                    NodeEnum::kFunctionHeader})) {
      return ref.Empty() ? SymbolMetaType::kClass : SymbolMetaType::kFunction;
    }
    if (Context().DirectParentsAre({NodeEnum::kUnqualifiedId,
                                    NodeEnum::kQualifiedId,  // out-of-line
                                    NodeEnum::kTaskHeader})) {
      return ref.Empty() ? SymbolMetaType::kClass : SymbolMetaType::kTask;
    }

    // TODO: import references bases must be resolved as
    // SymbolMetaType::kPackage.
    if (Context().DirectParentIs(NodeEnum::kActualNamedPort)) {
      return SymbolMetaType::kDataNetVariableInstance;
    }

    // module type or class parameters by name
    if (Context().DirectParentsAre(
            {NodeEnum::kParamByName, NodeEnum::kActualParameterByNameList})) {
      return SymbolMetaType::kParameter;
    }

    // function call arguments by name
    if (Context().DirectParentsAre(
            {NodeEnum::kParamByName, NodeEnum::kArgumentList})) {
      return SymbolMetaType::kDataNetVariableInstance;
    }

    if (Context().DirectParentsAre({NodeEnum::kUnqualifiedId,
                                    NodeEnum::kLocalRoot, NodeEnum::kReference,
                                    NodeEnum::kReferenceCallBase,
                                    NodeEnum::kFunctionCall})) {
      // bare call like "function_name(...)"
      if (UnextendedCall()) return SymbolMetaType::kCallable;
    }

    if (Context().DirectParentsAre(
            {NodeEnum::kUnqualifiedId, NodeEnum::kHierarchyExtension,
             NodeEnum::kReference, NodeEnum::kReferenceCallBase,
             NodeEnum::kFunctionCall})) {
      if (ExtendedCallIsLast()) return SymbolMetaType::kCallable;
    }

    if (Context().DirectParentsAre(
            {NodeEnum::kUnqualifiedId, NodeEnum::kMethodCallExtension,
             NodeEnum::kReferenceCallBase, NodeEnum::kFunctionCall})) {
      return SymbolMetaType::kCallable;
    }
    if (Context().DirectParentsAre(
            {NodeEnum::kUnqualifiedId, NodeEnum::kQualifiedId,
             NodeEnum::kLocalRoot, NodeEnum::kReference,
             NodeEnum::kReferenceCallBase, NodeEnum::kFunctionCall})) {
      // qualified call like "pkg_or_class::function_name(...)"
      // Only the last component needs to be callable.
      if (QualifiedIdComponentInLastPosition()) {
        return SymbolMetaType::kCallable;
      }
      // TODO(fangism): could require parents to be kPackage or kClass
    }

    if (Context().DirectParentsAre(
            {NodeEnum::kUnqualifiedId, NodeEnum::kHierarchyExtension,
             NodeEnum::kReference, NodeEnum::kReferenceCallBase})) {
      // method call like "obj.method_name(...)"
      return SymbolMetaType::kCallable;
      // TODO(fangism): check that method is non-static
    }

    if (Context().DirectParentsAre(
            {NodeEnum::kUnqualifiedId, NodeEnum::kExtendsList})) {
      // e.g. "base" in "class derived extends base;"
      return SymbolMetaType::kClass;
    }
    if (Context().DirectParentsAre({NodeEnum::kUnqualifiedId,
                                    NodeEnum::kQualifiedId,
                                    NodeEnum::kExtendsList})) {
      // base class is a qualified type like "pkg_or_class::class_name"
      // Only the last component needs to be a type.
      if (QualifiedIdComponentInLastPosition()) {
        return SymbolMetaType::kClass;
      }
      // TODO(fangism): could require parents to be kPackage or kClass
    }

    // Default: no specific metatype.
    return SymbolMetaType::kUnspecified;
  }

  // Creates a named element in the current scope.
  // Suitable for SystemVerilog language elements: functions, tasks, packages,
  // classes, modules, etc...
  SymbolTableNode *EmplaceElementInCurrentScope(const verible::Symbol &element,
                                                absl::string_view name,
                                                SymbolMetaType metatype) {
    const auto [kv, did_emplace] = current_scope_->TryEmplace(
        name, SymbolInfo{metatype, source_, &element});
    if (!did_emplace) {
      if (kv->second.Value().is_port_identifier) {
        kv->second.Value().supplement_definitions.push_back(name);
      } else {
        DiagnoseSymbolAlreadyExists(name, kv->second);
      }
    }
    return &kv->second;  // scope of the new (or pre-existing symbol)
  }

  // checks whether a given tag belongs to one of the listed tags
  bool IsTagMatching(int tag, std::initializer_list<int> tags) {
    return std::find(tags.begin(), tags.end(), tag) != tags.end();
  }

  // checks if the current first leaf has conflicting information with the
  // second symbol
  bool IsTypeLeafConflicting(const SyntaxTreeLeaf *first,
                             const verible::Symbol *second) {
    if (!first || !second) return false;
    if (IsTagMatching(second->Tag().tag,
                      {static_cast<int>(NodeEnum::kPackedDimensions),
                       static_cast<int>(NodeEnum::kUnpackedDimensions)})) {
      return false;
    }
    if (second->Kind() == verible::SymbolKind::kLeaf) {
      const SyntaxTreeLeaf *second_leaf =
          verible::down_cast<const SyntaxTreeLeaf *>(second);
      // conflict if there are multiple direction specifications
      const std::initializer_list<int> directiontags = {
          verilog_tokentype::TK_input, verilog_tokentype::TK_output,
          verilog_tokentype::TK_inout, verilog_tokentype::TK_ref};

      const bool is_first_direction =
          IsTagMatching(first->Tag().tag, directiontags);
      const bool is_second_direction =
          IsTagMatching(second_leaf->Tag().tag, directiontags);

      if (is_first_direction && is_second_direction) return true;

      // conflict if there are multiple sign specifications
      const std::initializer_list<int> signtags = {
          verilog_tokentype::TK_signed, verilog_tokentype::TK_unsigned};
      const bool is_first_sign = IsTagMatching(first->Tag().tag, signtags);
      const bool is_second_sign =
          IsTagMatching(second_leaf->Tag().tag, signtags);

      if (is_first_sign && is_second_sign) return true;

      // since dimensions are not handled here and
      // there are two different leaves that are not direction or sign
      // then we assume it is a different type on both sides (hence the
      // conflict)
      if (!(is_first_direction || is_second_direction || is_first_sign ||
            is_second_sign)) {
        return true;
      }
    }
    if (second->Kind() == verible::SymbolKind::kNode) {
      const SyntaxTreeNode *second_node =
          verible::down_cast<const SyntaxTreeNode *>(second);
      for (const auto &child : second_node->children()) {
        if (IsTypeLeafConflicting(first, child.get())) return true;
      }
    }
    return false;
  }

  // checks if two nodes have conflicting information
  bool DoesConflictingNodeExist(const SyntaxTreeNode *node,
                                const verible::Symbol *context) {
    if (context && context->Kind() == verible::SymbolKind::kNode) {
      const SyntaxTreeNode *second_node =
          verible::down_cast<const SyntaxTreeNode *>(context);
      if ((node->Tag().tag == second_node->Tag().tag) &&
          !verible::EqualTreesByEnumString(node, second_node)) {
        return true;
      }
      for (const auto &child : second_node->children()) {
        if (DoesConflictingNodeExist(node, child.get())) return true;
      }
    }
    return false;
  }

  // checks if two kDataTypes have conflicting information, used
  // in multiline definitions of nodes
  bool IsDataTypeNodeConflicting(const verible::Symbol *first,
                                 const verible::Symbol *second) {
    // if type was not specified for symbol (e.g. implicit) in any case, return
    // true
    if (!first || !second) return false;
    // if the left expression is a leaf, do final checks against the right
    // expression
    if (first->Kind() == verible::SymbolKind::kLeaf) {
      const SyntaxTreeLeaf *leaf =
          verible::down_cast<const SyntaxTreeLeaf *>(first);
      return IsTypeLeafConflicting(leaf, second);
    }
    // if the left expression is a node, iterate over its children and check
    // compatibility
    if (first->Kind() == verible::SymbolKind::kNode) {
      const SyntaxTreeNode *node =
          verible::down_cast<const SyntaxTreeNode *>(first);
      if (IsTagMatching(node->Tag().tag,
                        {static_cast<int>(NodeEnum::kPackedDimensions),
                         static_cast<int>(NodeEnum::kUnpackedDimensions)})) {
        if (DoesConflictingNodeExist(node, second)) return true;
        return false;
      }
      for (const auto &child : node->children()) {
        // run method recursively for each child
        if (IsDataTypeNodeConflicting(child.get(), second)) return true;
      }
    }
    return false;
  }

  // Checks potential multiline declaration of port
  // against correctness
  void CheckMultilinePortDeclarationCorrectness(SymbolTableNode *existing_node,
                                                absl::string_view name) {
    DeclarationTypeInfo &new_decl_info =
        *ABSL_DIE_IF_NULL(declaration_type_info_);
    DeclarationTypeInfo &old_decl_info = existing_node->Value().declared_type;
    // TODO (glatosinski): currently direction is kept separately from
    // kDataTypes, that is why it is handled separately. We may want to
    // include it in kDataType to have a full type information in one place
    // Also, according to some entries (e.g. net_variable) it is possible
    // to have both delay and strength, we may want to have separate fields
    // for them in MakeDataType (currently we have delay_or_strength)
    if (!new_decl_info.direction.empty() && !old_decl_info.direction.empty()) {
      DiagnoseSymbolAlreadyExists(name, *existing_node);
      return;
    }
    if (IsDataTypeNodeConflicting(old_decl_info.syntax_origin,
                                  new_decl_info.syntax_origin)) {
      DiagnoseSymbolAlreadyExists(name, *existing_node);
      return;
    }
    for (const auto &type_specification : old_decl_info.type_specifications) {
      if (IsDataTypeNodeConflicting(type_specification,
                                    new_decl_info.syntax_origin)) {
        DiagnoseSymbolAlreadyExists(name, *existing_node);
        return;
      }
    }
    existing_node->Value().supplement_definitions.push_back(name);
    existing_node->Value().declared_type.type_specifications.push_back(
        declaration_type_info_->syntax_origin);
  }

  // Creates a named typed element in the current scope.
  // Suitable for SystemVerilog language elements: nets, parameter, variables,
  // instances, functions (using their return types).
  SymbolTableNode &EmplaceTypedElementInCurrentScope(
      const verible::Symbol &element, absl::string_view name,
      SymbolMetaType metatype) {
    VLOG(2) << __FUNCTION__ << ": " << name << " in " << CurrentScopeFullPath();
    VLOG(3) << "  type info: " << *ABSL_DIE_IF_NULL(declaration_type_info_);
    VLOG(3) << "  full text: " << AutoTruncate{StringSpanOfSymbol(element), 40};
    const auto [kv, passed] = current_scope_->TryEmplace(
        name, SymbolInfo{
                  metatype, source_, &element,
                  // associate this instance with its declared type
                  *ABSL_DIE_IF_NULL(declaration_type_info_),  // copy
              });
    if (!passed) {
      if (kv->second.Value().is_port_identifier) {
        CheckMultilinePortDeclarationCorrectness(&kv->second, name);
      } else {
        DiagnoseSymbolAlreadyExists(name, kv->second);
      }
    }
    VLOG(2) << "end of " << __FUNCTION__ << ": " << name;
    return kv->second;  // scope of the new (or pre-existing symbol)
  }

  // Creates a port identifier element in the current scope.
  // Suitable for SystemVerilog module port declarations, where
  // there are multiple lines defining the symbol.
  SymbolTableNode &EmplacePortIdentifierInCurrentScope(
      const verible::Symbol &element, absl::string_view name,
      SymbolMetaType metatype) {
    VLOG(2) << __FUNCTION__ << ": " << name << " in " << CurrentScopeFullPath();
    VLOG(3) << "  type info: " << *ABSL_DIE_IF_NULL(declaration_type_info_);
    VLOG(3) << "  full text: " << AutoTruncate{StringSpanOfSymbol(element), 40};
    const auto p = current_scope_->TryEmplace(
        name, SymbolInfo{
                  metatype, source_, &element,
                  // associate this instance with its declared type
                  *ABSL_DIE_IF_NULL(declaration_type_info_),  // copy
              });
    p.first->second.Value().is_port_identifier = true;
    if (!p.second) {
      // the symbol was already defined, add it to supplement_definitions
      CheckMultilinePortDeclarationCorrectness(&p.first->second, name);
    }
    VLOG(2) << "end of " << __FUNCTION__ << ": " << name;
    return p.first->second;  // scope of the new (or pre-existing symbol)
  }

  // Creates a named element in the current scope, and traverses its subtree
  // inside the new element's scope.
  // Returns the new scope.
  SymbolTableNode *DeclareScopedElementAndDescend(const SyntaxTreeNode &element,
                                                  absl::string_view name,
                                                  SymbolMetaType type) {
    SymbolTableNode *enter_scope =
        EmplaceElementInCurrentScope(element, name, type);
    Descend(element, enter_scope);
    return enter_scope;
  }

  void DeclareModule(const SyntaxTreeNode &module) {
    const SyntaxTreeLeaf *module_name = GetModuleName(module);
    if (!module_name) return;
    DeclareScopedElementAndDescend(module, module_name->get().text(),
                                   SymbolMetaType::kModule);
  }

  absl::string_view GetScopeNameFromGenerateBody(const SyntaxTreeNode &body) {
    if (body.MatchesTag(NodeEnum::kGenerateBlock)) {
      const SyntaxTreeNode *gen_block = GetGenerateBlockBegin(body);
      const TokenInfo *label =
          gen_block ? GetBeginLabelTokenInfo(*gen_block) : nullptr;
      if (label != nullptr) {
        // TODO: Check for a matching end-label here, and if its name matches
        // the begin label, then immediately create a resolved reference because
        // it only makes sense for it resolve to this begin.
        // Otherwise, do nothing with the end label.
        return label->text();
      }
    }
    return current_scope_->Value().CreateAnonymousScope("generate");
  }

  void DeclareGenerateIf(const SyntaxTreeNode &generate_if) {
    const SyntaxTreeNode *body(GetIfClauseGenerateBody(generate_if));
    if (body) {
      DeclareScopedElementAndDescend(generate_if,
                                     GetScopeNameFromGenerateBody(*body),
                                     SymbolMetaType::kGenerate);
    }
  }

  void DeclareGenerateElse(const SyntaxTreeNode &generate_else) {
    const SyntaxTreeNode *body(GetElseClauseGenerateBody(generate_else));
    if (!body) return;

    if (body->MatchesTag(NodeEnum::kConditionalGenerateConstruct)) {
      // else-if chained.  Flatten the else block by not creating a new scope
      // and let the if-clause inside create a scope directly under the current
      // scope.
      Descend(*body);
    } else {
      DeclareScopedElementAndDescend(generate_else,
                                     GetScopeNameFromGenerateBody(*body),
                                     SymbolMetaType::kGenerate);
    }
  }

  void DeclarePackage(const SyntaxTreeNode &package) {
    const auto *token = GetPackageNameToken(package);
    if (!token) return;
    DeclareScopedElementAndDescend(package, token->text(),
                                   SymbolMetaType::kPackage);
  }

  void DeclareClass(const SyntaxTreeNode &class_node) {
    const SyntaxTreeLeaf *class_name = GetClassName(class_node);
    if (!class_name) return;
    DeclareScopedElementAndDescend(class_node, class_name->get().text(),
                                   SymbolMetaType::kClass);
  }

  void DeclareInterface(const SyntaxTreeNode &interface) {
    const auto *token = GetInterfaceNameToken(interface);
    if (!token) return;
    DeclareScopedElementAndDescend(interface, token->text(),
                                   SymbolMetaType::kInterface);
  }

  void DeclareTask(const SyntaxTreeNode &task_node) {
    const ValueSaver<SymbolTableNode *> reserve_for_task_decl(
        &current_scope_);  // no scope change yet
    Descend(task_node);
  }

  void DeclareFunction(const SyntaxTreeNode &function_node) {
    // Reserve a slot for the function's scope on the stack, but do not set it
    // until we add it in HandleIdentifier().  This deferral allows us to
    // evaluate the return type of the declared function as a reference in the
    // current context.
    const ValueSaver<SymbolTableNode *> reserve_for_function_decl(
        &current_scope_);  // no scope change yet
    Descend(function_node);
  }

  void DeclareConstructor(const SyntaxTreeNode &constructor_node) {
    // Reserve a slot for the constructor's scope on the stack, but do not set
    // it until we add it in HandleIdentifier(). The effective return type of
    // the constructor is the class type.
    const ValueSaver<SymbolTableNode *> reserve_for_function_decl(
        &current_scope_);  // no scope change yet

    const SyntaxTreeLeaf *new_keyword =
        GetConstructorPrototypeNewKeyword(constructor_node);
    // Create a self-reference to this class.
    const ReferenceComponent class_type_ref{
        .identifier = new_keyword->get().text(),  // "new"
        .ref_type = ReferenceType::kImmediate,
        .required_metatype = SymbolMetaType::kClass,
        // pre-resolve this symbol to the enclosing class immediately
        .resolved_symbol = current_scope_,  // the current class
    };

    // Build-up a reference to the constructor, rooted at the class node.
    const CaptureDependentReference capture(this);
    capture.Ref().PushReferenceComponent(class_type_ref);

    DeclarationTypeInfo decl_type_info{
        // There is no actual source text that references the type here.
        // We arbitrarily designate the 'new' keyword as the reference point.
        .syntax_origin = new_keyword,
        .user_defined_type = capture.Ref().LastLeaf(),
    };
    const ValueSaver<DeclarationTypeInfo *> function_return_type(
        &declaration_type_info_, &decl_type_info);

    Descend(constructor_node);
  }

  void DeclarePorts(const SyntaxTreeNode &port_list) {
    // For out-of-line function declarations, do not re-declare ports that
    // already came from the method prototype.
    // We designate the prototype as the source-of-truth because in Verilog,
    // port *names* are part of the public interface (allowing calling with
    // named parameter assignments, unlike C++ function calls).
    // LRM 8.24: "The out-of-block method declaration shall match the prototype
    // declaration exactly, with the following exceptions..."
    {
      const SyntaxTreeNode *function_header =
          Context().NearestParentMatching([](const SyntaxTreeNode &node) {
            return node.MatchesTag(NodeEnum::kFunctionHeader);
          });
      if (function_header != nullptr) {
        const SyntaxTreeNode &id = verible::SymbolCastToNode(
            *ABSL_DIE_IF_NULL(GetFunctionHeaderId(*function_header)));
        if (id.MatchesTag(NodeEnum::kQualifiedId)) {
          // For now, ignore the out-of-line port declarations.
          // TODO: Diagnose port type/name mismatches between prototypes' and
          // out-of-line headers' ports.
          return;
        }
      }
    }
    {
      const SyntaxTreeNode *task_header =
          Context().NearestParentMatching([](const SyntaxTreeNode &node) {
            return node.MatchesTag(NodeEnum::kTaskHeader);
          });
      if (task_header != nullptr) {
        const SyntaxTreeNode &id = verible::SymbolCastToNode(
            *ABSL_DIE_IF_NULL(GetTaskHeaderId(*task_header)));
        if (id.MatchesTag(NodeEnum::kQualifiedId)) {
          // For now, ignore the out-of-line port declarations.
          // TODO: Diagnose port type/name mismatches between prototypes' and
          // out-of-line headers' ports.
          return;
        }
      }
    }
    // In all other cases, declare ports normally at the declaration site.
    Descend(port_list);
  }

  // Capture the declared function's return type.
  void SetupFunctionHeader(const SyntaxTreeNode &function_header) {
    DeclarationTypeInfo decl_type_info;
    const ValueSaver<DeclarationTypeInfo *> function_return_type(
        &declaration_type_info_, &decl_type_info);
    Descend(function_header);
    // decl_type_info will be safely copied away in HandleIdentifier().
  }

  // TODO: functions and tasks, which could appear as out-of-line definitions.

  void DeclareParameter(const SyntaxTreeNode &param_decl_node) {
    CHECK(param_decl_node.MatchesTag(NodeEnum::kParamDeclaration));
    DeclarationTypeInfo decl_type_info;
    // Set declaration_type_info_ to capture any user-defined type used to
    // declare data/variables/instances.
    const ValueSaver<DeclarationTypeInfo *> save_type(&declaration_type_info_,
                                                      &decl_type_info);
    Descend(param_decl_node);
  }

  // Declares one or more variables/instances/nets.
  void DeclareData(const SyntaxTreeNode &data_decl_node) {
    VLOG(2) << __FUNCTION__;
    DeclarationTypeInfo decl_type_info;
    // Set declaration_type_info_ to capture any user-defined type used to
    // declare data/variables/instances.
    const ValueSaver<DeclarationTypeInfo *> save_type(&declaration_type_info_,
                                                      &decl_type_info);
    // reset port direction
    Descend(data_decl_node);
    VLOG(2) << "end of " << __FUNCTION__;
  }

  // Declare one (of potentially multiple) instances in a single declaration
  // statement.
  void DeclareInstance(const SyntaxTreeNode &instance) {
    const verible::TokenInfo *instance_name_token =
        GetModuleInstanceNameTokenInfoFromGateInstance(instance);
    if (!instance_name_token) return;
    const absl::string_view instance_name(instance_name_token->text());
    const SymbolTableNode &new_instance(EmplaceTypedElementInCurrentScope(
        instance, instance_name, SymbolMetaType::kDataNetVariableInstance));

    // Also create a DependentReferences chain starting with this named instance
    // so that named port references are direct children of this reference root.
    // This is a self-reference.
    const CaptureDependentReference capture(this);
    capture.Ref().PushReferenceComponent(ReferenceComponent{
        .identifier = instance_name,
        .ref_type = ReferenceType::kUnqualified,
        .required_metatype = SymbolMetaType::kDataNetVariableInstance,
        // Start with its type already resolved to the node we just declared.
        .resolved_symbol = &new_instance,
    });

    // Inform that named port identifiers will yield parallel children from
    // this reference branch point.
    const ValueSaver<ReferenceComponentNode *> set_branch(
        &reference_branch_point_, capture.Ref().components.get());

    // No change of scope, but named ports will be resolved with respect to the
    // decl_type_info's scope later.
    Descend(instance);  // visit parameter/port connections, etc.
  }

  void DeclareNet(const SyntaxTreeNode &net_variable) {
    const SyntaxTreeLeaf *net_variable_name =
        GetNameLeafOfNetVariable(net_variable);
    if (!net_variable_name) return;
    const absl::string_view net_name(net_variable_name->get().text());
    EmplaceTypedElementInCurrentScope(net_variable, net_name,
                                      SymbolMetaType::kDataNetVariableInstance);
    Descend(net_variable);
  }

  void DeclareRegister(const SyntaxTreeNode &reg_variable) {
    const SyntaxTreeLeaf *register_variable_name =
        GetNameLeafOfRegisterVariable(reg_variable);
    if (!register_variable_name) return;
    const absl::string_view net_name(register_variable_name->get().text());
    EmplaceTypedElementInCurrentScope(reg_variable, net_name,
                                      SymbolMetaType::kDataNetVariableInstance);
    Descend(reg_variable);
  }

  void DeclareVariable(const SyntaxTreeNode &variable) {
    const SyntaxTreeLeaf *unqualified_id =
        GetUnqualifiedIdFromVariableDeclarationAssignment(variable);
    if (unqualified_id) {
      const absl::string_view var_name(unqualified_id->get().text());
      EmplaceTypedElementInCurrentScope(
          variable, var_name, SymbolMetaType::kDataNetVariableInstance);
    }
    Descend(variable);
  }

  void DiagnoseSymbolAlreadyExists(absl::string_view name,
                                   const SymbolTableNode &previous_symbol) {
    std::ostringstream here_print;
    here_print << source_->GetTextStructure()->GetRangeForText(name);

    std::ostringstream previous_print;
    previous_print << previous_symbol.Value()
                          .file_origin->GetTextStructure()
                          ->GetRangeForText(*previous_symbol.Key());

    // TODO(hzeller): output in some structured form easy to use downstream.
    diagnostics_.push_back(absl::AlreadyExistsError(absl::StrCat(
        source_->ReferencedPath(), ":", here_print.str(), " Symbol \"", name,
        "\" is already defined in the ", CurrentScopeFullPath(), " scope at ",
        previous_print.str())));
  }

  absl::StatusOr<SymbolTableNode *> LookupOrInjectOutOfLineDefinition(
      const SyntaxTreeNode &qualified_id, SymbolMetaType metatype,
      const SyntaxTreeNode *definition_syntax) {
    // e.g. "function int class_c::func(...); ... endfunction"
    // Use a DependentReference object to establish a self-reference.
    CaptureDependentReference capture(this);
    Descend(qualified_id);

    DependentReferences &ref(capture.Ref());
    // Expecting only two-level reference "outer::inner".
    CHECK_EQ(ABSL_DIE_IF_NULL(ref.components)->Children().size(), 1);

    // Must resolve base, instead of deferring to resolve phase.
    // Do not inject the outer_scope (class name) into the current scope.
    // Reject injections into non-classes.
    const auto outer_scope_or_status =
        ref.ResolveOnlyBaseLocally(current_scope_);
    if (!outer_scope_or_status.ok()) {
      return outer_scope_or_status.status();
    }
    SymbolTableNode *outer_scope = ABSL_DIE_IF_NULL(*outer_scope_or_status);

    // Lookup inner symbol in outer_scope, but also allow injection of the
    // inner symbol name into the outer_scope (with diagnostic).
    ReferenceComponent &inner_ref = ref.components->Children().front().Value();
    const absl::string_view inner_key = inner_ref.identifier;

    const auto p = outer_scope->TryEmplace(
        inner_key, SymbolInfo{metatype, source_, definition_syntax});
    SymbolTableNode *inner_symbol = &p.first->second;
    if (p.second) {
      // If injection succeeded, then the outer_scope did not already contain a
      // forward declaration of the inner symbol to be defined.
      // Diagnose this non-fatally, but continue.
      diagnostics_.push_back(
          DiagnoseMemberSymbolResolutionFailure(inner_key, *outer_scope));
    } else {
      // Use pre-existing symbol table entry created from the prototype.
      // Check that out-of-line and prototype symbol metatypes match.
      const SymbolMetaType original_metatype = inner_symbol->Value().metatype;
      if (original_metatype != metatype) {
        return absl::AlreadyExistsError(
            absl::StrCat(SymbolMetaTypeAsString(original_metatype), " ",
                         ContextFullPath(*inner_symbol),
                         " cannot be redefined out-of-line as a ",
                         SymbolMetaTypeAsString(metatype)));
      }
    }
    // Resolve this self-reference immediately.
    inner_ref.resolved_symbol = inner_symbol;
    return inner_symbol;  // mutable for purpose of constructing definition
  }

  void DescendThroughOutOfLineDefinition(const SyntaxTreeNode &qualified_id,
                                         SymbolMetaType type,
                                         const SyntaxTreeNode *decl_syntax) {
    const auto inner_symbol_or_status =
        LookupOrInjectOutOfLineDefinition(qualified_id, type, decl_syntax);
    // Change the current scope (which was set up on the stack by
    // kFunctionDeclaration or kTaskDeclaration) for the rest of the
    // definition.
    if (inner_symbol_or_status.ok()) {
      current_scope_ = *inner_symbol_or_status;
      Descend(qualified_id);
    } else {
      // On failure, skip the entire definition because there is no place
      // to add its local symbols.
      diagnostics_.push_back(inner_symbol_or_status.status());
    }
  }

  void HandleQualifiedId(const SyntaxTreeNode &qualified_id) {
    switch (static_cast<NodeEnum>(Context().top().Tag().tag)) {
      case NodeEnum::kFunctionHeader: {
        const SyntaxTreeNode *decl_syntax =
            Context().NearestParentMatching([](const SyntaxTreeNode &node) {
              return node.MatchesTagAnyOf({NodeEnum::kFunctionDeclaration,
                                           NodeEnum::kFunctionPrototype});
            });
        DescendThroughOutOfLineDefinition(qualified_id,
                                          SymbolMetaType::kFunction,
                                          ABSL_DIE_IF_NULL(decl_syntax));
        break;
      }
      case NodeEnum::kTaskHeader: {
        const SyntaxTreeNode *decl_syntax =
            Context().NearestParentMatching([](const SyntaxTreeNode &node) {
              return node.MatchesTagAnyOf(
                  {NodeEnum::kTaskDeclaration, NodeEnum::kTaskPrototype});
            });
        DescendThroughOutOfLineDefinition(qualified_id, SymbolMetaType::kTask,
                                          ABSL_DIE_IF_NULL(decl_syntax));
        break;
      }
      default:
        // Treat this as a reference, not an out-of-line definition.
        Descend(qualified_id);
        break;
    }
  }

  void EnterIncludeFile(const SyntaxTreeNode &preprocessor_include) {
    const SyntaxTreeLeaf *included_filename =
        GetFileFromPreprocessorInclude(preprocessor_include);
    if (included_filename == nullptr) return;

    const absl::string_view filename_text = included_filename->get().text();

    // Remove the double quotes from the filename.
    const absl::string_view filename_unquoted = StripOuterQuotes(filename_text);
    VLOG(3) << "got: `include \"" << filename_unquoted << "\"";

    // Opening included file requires a VerilogProject.
    // Open this file (could be first time, or previously opened).
    VerilogProject *project = symbol_table_->project_;
    if (project == nullptr) return;  // Without project, ignore.

    const auto status_or_file = project->OpenIncludedFile(filename_unquoted);
    if (!status_or_file.ok()) {
      diagnostics_.push_back(status_or_file.status());
      // Errors can be retrieved later.
      return;
    }

    VerilogSourceFile *const included_file = *status_or_file;
    if (included_file == nullptr) return;
    VLOG(3) << "opened include file: " << included_file->ResolvedPath();

    const auto parse_status = included_file->Parse();
    if (!parse_status.ok()) {
      diagnostics_.push_back(parse_status);
      // For now, don't bother attempting to parse a partial syntax tree.
      // This would be best handled in the future with actual preprocessing.
      return;
    }

    // Depending on application, one may wish to avoid re-processing the same
    // included file.  If desired, add logic to return early here.

    {  // Traverse included file's syntax tree.
      const ValueSaver<const VerilogSourceFile *> includer(&source_,
                                                           included_file);
      const ValueSaver<TokenInfo::Context> save_context_text(
          &token_context_, MakeTokenContext());
      included_file->GetTextStructure()->SyntaxTree()->Accept(this);
    }
  }

  std::string CurrentScopeFullPath() const {
    return ContextFullPath(*current_scope_);
  }

  verible::TokenWithContext VerboseToken(const TokenInfo &token) const {
    return verible::TokenWithContext{token, token_context_};
  }

  TokenInfo::Context MakeTokenContext() const {
    return TokenInfo::Context(
        source_->GetTextStructure()->Contents(),
        [](std::ostream &stream, int e) { stream << verilog_symbol_name(e); });
  }

 private:  // data
  // Points to the source file that is the origin of symbols.
  // This changes when opening preprocess-included files.
  // TODO(fangism): maintain a vector/stack of these for richer diagnostics
  const VerilogSourceFile *source_;

  // For human-readable debugging.
  // This should be constructed using MakeTokenContext(), after setting
  // 'source_'.
  TokenInfo::Context token_context_;

  // The symbol table to build, never nullptr.
  SymbolTable *const symbol_table_;

  // The remaining fields are mutable state:

  // This is the current scope where encountered definitions register their
  // symbols, never nullptr.
  // There is no need to maintain a stack because SymbolTableNodes already link
  // to their parents.
  SymbolTableNode *current_scope_;

  // Stack of references.
  // A stack is needed to support nested type references like "A#(B(#(C)))",
  // and nested expressions like "f(g(h))"
  std::stack<DependentReferences> reference_builders_;

  // When creating branched references, like with instances' named ports,
  // set this to the nearest branch point.
  // This will signal to the reference builder that parallel children
  // are to be added, as opposed to deeper descendants.
  ReferenceComponentNode *reference_branch_point_ = nullptr;

  // For a data/instance/variable declaration statement, this is the declared
  // type (could be primitive or named-user-defined).
  // For functions, this is the return type.
  // For constructors, this is the class type.
  // Set this type before traversing declared instances and variables to capture
  // the type of the declaration.  Unset this to prevent type capture.
  // Such declarations cannot nest, so a stack is not needed.
  DeclarationTypeInfo *declaration_type_info_ = nullptr;

  // Update to either "::" or '.'.
  const TokenInfo *last_hierarchy_operator_ = nullptr;

  // Collection of findings that might be considered compiler/tool errors in a
  // real toolchain.  For example: attempt to redefine symbol.
  std::vector<absl::Status> diagnostics_;
};

void ReferenceComponent::VerifySymbolTableRoot(
    const SymbolTableNode *root) const {
  if (resolved_symbol != nullptr) {
    CHECK_EQ(resolved_symbol->Root(), root)
        << "Resolved symbols must point to a node in the same SymbolTable.";
  }
}

absl::Status ReferenceComponent::MatchesMetatype(
    SymbolMetaType found_metatype) const {
  switch (required_metatype) {
    case SymbolMetaType::kUnspecified:
      return absl::OkStatus();
    case SymbolMetaType::kCallable:
      if (found_metatype == SymbolMetaType::kFunction ||
          found_metatype == SymbolMetaType::kTask) {
        return absl::OkStatus();
      }
      break;
    case SymbolMetaType::kClass:
      if (found_metatype == SymbolMetaType::kClass ||
          found_metatype == SymbolMetaType::kTypeAlias) {
        // Where a class is expected, a typedef could be accepted.
        return absl::OkStatus();
      }
      break;
    default:
      if (required_metatype == found_metatype) return absl::OkStatus();
      break;
  }
  // Otherwise, mismatched metatype.
  return absl::InvalidArgumentError(
      absl::StrCat("Expecting reference \"", identifier, "\" to resolve to a ",
                   SymbolMetaTypeAsString(required_metatype), ", but found a ",
                   SymbolMetaTypeAsString(found_metatype), "."));
}

absl::Status ReferenceComponent::ResolveSymbol(
    const SymbolTableNode &resolved) {
  // Verify metatype match.
  if (auto metatype_match_status = MatchesMetatype(resolved.Value().metatype);
      !metatype_match_status.ok()) {
    VLOG(2) << metatype_match_status.message();
    return metatype_match_status;
  }

  VLOG(2) << "  resolved: " << ContextFullPath(resolved);
  resolved_symbol = &resolved;
  return absl::OkStatus();
}

const ReferenceComponentNode *DependentReferences::LastLeaf() const {
  if (components == nullptr) return nullptr;
  const ReferenceComponentNode *node = components.get();
  while (!is_leaf(*node)) node = &node->Children().front();
  return node;
}

// RefType can be ReferenceComponentNode or const ReferenceComponentNode.
template <typename RefType>
static RefType *ReferenceLastTypeComponent(RefType *node) {
  // This references a type that may be nested and have name parameters.
  // From A#(.B())::C#(.D()), we want C as the desired type component.
  while (!is_leaf(*node)) {
    // There should be at most one non-parameter at each branch point,
    // so stop at the first one found.
    // In the above example, this would find "C" among {"B", "C"} inside "A".
    auto &branches(node->Children());
    const auto found = std::find_if(
        branches.begin(), branches.end(), [](const ReferenceComponentNode &n) {
          return n.Value().required_metatype != SymbolMetaType::kParameter;
        });
    // If there are only parameters referenced, then this code is the last
    // component we want.
    if (found == branches.end()) return node;
    // Otherwise, continue searching along the non-parameter branch.
    node = &*found;
  }
  return node;
}

const ReferenceComponentNode *DependentReferences::LastTypeComponent() const {
  // This references a type that may be nested and have name parameters.
  // From A#(.B())::C#(.D()), we want C as the desired type component.
  if (components == nullptr) return nullptr;
  const ReferenceComponentNode *node = components.get();
  return ReferenceLastTypeComponent(node);
}

ReferenceComponentNode *DependentReferences::LastTypeComponent() {
  if (components == nullptr) return nullptr;
  ReferenceComponentNode *node = components.get();
  return ReferenceLastTypeComponent(node);
}

ReferenceComponentNode *DependentReferences::PushReferenceComponent(
    const ReferenceComponent &component) {
  VLOG(3) << __FUNCTION__ << ", id: " << component.identifier;
  ReferenceComponentNode *new_child;
  if (Empty()) {
    components = std::make_unique<ReferenceComponentNode>(component);  // copy
    new_child = components.get();
  } else {
    // Find the last node from which references can be grown.
    // Exclude type named parameters.
    ReferenceComponentNode *node = LastTypeComponent();
    new_child = CheckedNewChildReferenceNode(node, component);
  }
  VLOG(3) << "end of " << __FUNCTION__ << ":\n" << *this;
  return new_child;
}

void DependentReferences::VerifySymbolTableRoot(
    const SymbolTableNode *root) const {
  if (components != nullptr) {
    ApplyPreOrder(*components, [=](const ReferenceComponent &component) {
      component.VerifySymbolTableRoot(root);
    });
  }
}

std::ostream &operator<<(std::ostream &stream,
                         const DependentReferences &dep_refs) {
  if (dep_refs.components == nullptr) return stream << "(empty-ref)";
  return stream << *dep_refs.components;
}

// Follow type aliases through canonical type.
static const SymbolTableNode *CanonicalizeTypeForMemberLookup(
    const SymbolTableNode &context) {
  VLOG(2) << __FUNCTION__;
  const SymbolTableNode *current_context = &context;
  do {
    VLOG(2) << "  -> " << ContextFullPath(*current_context);
    if (current_context->Value().metatype != SymbolMetaType::kTypeAlias) break;
    const ReferenceComponentNode *ref_type =
        current_context->Value().declared_type.user_defined_type;
    if (ref_type == nullptr) {
      // Could be a primitive type.
      return nullptr;
    }
    current_context = ref_type->Value().resolved_symbol;
    // TODO: We haven't guaranteed that typedefs have been resolved in order,
    // so these will need to be resolved on-demand in the future.
  } while (current_context != nullptr);
  // TODO: the return value currently does not distinguish between a failed
  // resolution of a dependent symbol and a primitive referenced type;
  // both yield a nullptr.
  return current_context;
}

// Search through base class's scopes for a symbol.
static const SymbolTableNode *LookupSymbolThroughInheritedScopes(
    const SymbolTableNode &context, absl::string_view symbol) {
  const SymbolTableNode *current_context = &context;
  do {
    // Look directly in current scope.
    const auto found = current_context->Find(symbol);
    if (found != current_context->end()) {
      return &found->second;
    }
    // TODO: lookup imported namespaces and symbols

    // Point to next inherited scope.
    const auto *base_type =
        current_context->Value().parent_type.user_defined_type;
    if (base_type == nullptr) break;

    const SymbolTableNode *resolved_base = base_type->Value().resolved_symbol;
    // TODO: attempt to resolve on-demand because resolve ordering is not
    // guaranteed.
    if (resolved_base == nullptr) return nullptr;

    // base type could be a typedef, so canonicalize
    current_context = CanonicalizeTypeForMemberLookup(*resolved_base);
  } while (current_context != nullptr);
  return nullptr;  // resolution failed
}

// Search up-scope, stopping at the first symbol found in the nearest scope.
static const SymbolTableNode *LookupSymbolUpwards(
    const SymbolTableNode &context, absl::string_view symbol) {
  const SymbolTableNode *current_context = &context;
  do {
    const SymbolTableNode *found =
        LookupSymbolThroughInheritedScopes(*current_context, symbol);
    if (found != nullptr) return found;

    // Point to next enclosing scope.
    current_context = current_context->Parent();
  } while (current_context != nullptr);
  return nullptr;  // resolution failed
}

static absl::Status DiagnoseUnqualifiedSymbolResolutionFailure(
    absl::string_view name, const SymbolTableNode &context) {
  return absl::NotFoundError(absl::StrCat("Unable to resolve symbol \"", name,
                                          "\" from context ",
                                          ContextFullPath(context), "."));
}

static void ResolveReferenceComponentNodeLocal(ReferenceComponentNode *node,
                                               const SymbolTableNode &context) {
  ReferenceComponent &component(node->Value());
  VLOG(2) << __FUNCTION__ << ": " << component;
  // If already resolved, skip.
  if (component.resolved_symbol != nullptr) return;  // already bound
  const absl::string_view key(component.identifier);
  CHECK(node->Parent() == nullptr);  // is root
  // root node: lookup this symbol from its context upward
  CHECK_EQ(component.ref_type, ReferenceType::kUnqualified);

  // Only try to resolve using the same scope in which the reference appeared,
  // local, without upward search.
  const auto found = context.Find(key);
  if (found != context.end()) {
    component.resolved_symbol = &found->second;
  }
}

static void ResolveUnqualifiedName(ReferenceComponent *component,
                                   const SymbolTableNode &context,
                                   std::vector<absl::Status> *diagnostics) {
  VLOG(2) << __FUNCTION__ << ": " << component;
  const absl::string_view key(component->identifier);
  // Find the first symbol whose name matches, without regard to its metatype.
  const SymbolTableNode *resolved = LookupSymbolUpwards(context, key);
  if (resolved == nullptr) {
    diagnostics->emplace_back(
        DiagnoseUnqualifiedSymbolResolutionFailure(key, context));
    return;
  }

  const auto resolve_status = component->ResolveSymbol(*resolved);
  if (!resolve_status.ok()) {
    diagnostics->push_back(resolve_status);
  }
  VLOG(2) << "end of " << __FUNCTION__;
}

// Search this scope directly for a symbol, without any upward/inheritance
// lookups.
static void ResolveImmediateMember(ReferenceComponent *component,
                                   const SymbolTableNode &context,
                                   std::vector<absl::Status> *diagnostics) {
  VLOG(2) << __FUNCTION__ << ": " << component;
  const absl::string_view key(component->identifier);
  const auto found = context.Find(key);
  if (found == context.end()) {
    diagnostics->emplace_back(
        DiagnoseMemberSymbolResolutionFailure(key, context));
    return;
  }

  const SymbolTableNode &found_symbol = found->second;
  const auto resolve_status = component->ResolveSymbol(found_symbol);
  if (!resolve_status.ok()) {
    diagnostics->push_back(resolve_status);
  }
  VLOG(2) << "end of " << __FUNCTION__;
}

static void ResolveDirectMember(ReferenceComponent *component,
                                const SymbolTableNode &context,
                                std::vector<absl::Status> *diagnostics) {
  VLOG(2) << __FUNCTION__ << ": " << component;

  // Canonicalize context if it an alias.
  const SymbolTableNode *canonical_context =
      CanonicalizeTypeForMemberLookup(context);
  if (canonical_context == nullptr) {
    // TODO: diagnostic could be improved by following each typedef indirection.
    diagnostics->push_back(absl::InvalidArgumentError(
        absl::StrCat("Canonical type of ", ContextFullPath(context),
                     " does not have any members.")));
    return;
  }

  const absl::string_view key(component->identifier);
  const auto *found =
      LookupSymbolThroughInheritedScopes(*canonical_context, key);
  if (found == nullptr) {
    diagnostics->emplace_back(
        DiagnoseMemberSymbolResolutionFailure(key, *canonical_context));
    return;
  }

  const SymbolTableNode &found_symbol = *found;
  const auto resolve_status = component->ResolveSymbol(found_symbol);
  if (!resolve_status.ok()) {
    diagnostics->push_back(resolve_status);
  }
  VLOG(2) << "end of " << __FUNCTION__;
}

// This is the primary function that resolves references.
// Dependent (parent) nodes must already be resolved before attempting to
// resolve children references (guaranteed by calling this in a pre-order
// traversal).
static void ResolveReferenceComponentNode(
    ReferenceComponentNode *node, const SymbolTableNode &context,
    std::vector<absl::Status> *diagnostics) {
  ReferenceComponent &component(node->Value());
  VLOG(2) << __FUNCTION__ << ": " << component;
  if (component.resolved_symbol != nullptr) return;  // already bound

  switch (component.ref_type) {
    case ReferenceType::kUnqualified: {
      // root node: lookup this symbol from its context upward
      if (node->Parent() != nullptr) {
        // TODO(hzeller): Is this a situation that should never happen thus
        // be dealt with further up-stream ? (changed from a CHECK()).
        LOG(WARNING) << *node << ": Parent exists " << *node->Parent() << "\n";
        return;
      }
      ResolveUnqualifiedName(&component, context, diagnostics);
      break;
    }
    case ReferenceType::kImmediate: {
      ResolveImmediateMember(&component, context, diagnostics);
      break;
    }
    case ReferenceType::kDirectMember: {
      // Use parent's scope (if resolved successfully) to resolve this node.
      const ReferenceComponent &parent_component(node->Parent()->Value());

      const SymbolTableNode *parent_scope = parent_component.resolved_symbol;
      if (parent_scope == nullptr) return;  // leave this subtree unresolved

      ResolveDirectMember(&component, *parent_scope, diagnostics);
      break;
    }
    case ReferenceType::kMemberOfTypeOfParent: {
      // Use parent's type's scope (if resolved successfully) to resolve this
      // node. Get the type of the object from the parent component.
      const ReferenceComponent &parent_component(node->Parent()->Value());
      const SymbolTableNode *parent_scope = parent_component.resolved_symbol;
      if (parent_scope == nullptr) return;  // leave this subtree unresolved

      const DeclarationTypeInfo &type_info =
          parent_scope->Value().declared_type;
      // Primitive types do not have members.
      if (type_info.user_defined_type == nullptr) {
        if (type_info.syntax_origin == nullptr) {
          diagnostics->push_back(absl::InvalidArgumentError(
              absl::StrCat("Type of parent reference ",
                           ReferenceNodeFullPathString(*node->Parent()),
                           " does not have syntax origin.")));
        }
        diagnostics->push_back(absl::InvalidArgumentError(absl::StrCat(
            "Type of parent reference ",
            ReferenceNodeFullPathString(*node->Parent()), " (",
            type_info.syntax_origin
                ? verible::StringSpanOfSymbol(*type_info.syntax_origin)
                : "nullptr",
            ") does not have any members.")));
        return;
      }

      // This referenced object's scope is not a parent of this node, and
      // thus, not guaranteed to have been resolved first.
      // TODO(fangism): resolve on-demand
      const SymbolTableNode *type_scope =
          type_info.user_defined_type->Value().resolved_symbol;
      if (type_scope == nullptr) return;

      ResolveDirectMember(&component, *type_scope, diagnostics);
      break;
    }
  }
  VLOG(2) << "end of " << __FUNCTION__;
}

ReferenceComponentMap ReferenceComponentNodeMapView(
    const ReferenceComponentNode &node) {
  ReferenceComponentMap map_view;
  for (const auto &child : node.Children()) {
    map_view.emplace(std::make_pair(child.Value().identifier, &child));
  }
  return map_view;
}

void DependentReferences::Resolve(
    const SymbolTableNode &context,
    std::vector<absl::Status> *diagnostics) const {
  VLOG(2) << __FUNCTION__;
  if (components == nullptr) return;
  // References are arranged in dependency trees.
  // Parent node references must be resolved before children nodes,
  // hence a pre-order traversal.
  ApplyPreOrder(*components,
                [&context, diagnostics](ReferenceComponentNode &node) {
                  ResolveReferenceComponentNode(&node, context, diagnostics);
                  // TODO: minor optimization, when resolution for a node fails,
                  // skip checking that node's subtree; early terminate.
                });
  VLOG(2) << "end of " << __FUNCTION__;
}

void DependentReferences::ResolveLocally(const SymbolTableNode &context) const {
  if (components == nullptr) return;
  // Only attempt to resolve the reference root, and none of its subtrees.
  ResolveReferenceComponentNodeLocal(components.get(), context);
}

absl::StatusOr<SymbolTableNode *> DependentReferences::ResolveOnlyBaseLocally(
    SymbolTableNode *context) {
  // Similar lookup to ResolveReferenceComponentNodeLocal() but allows
  // mutability of 'context' for injecting out-of-line definitions.

  ReferenceComponent &base(ABSL_DIE_IF_NULL(components)->Value());
  CHECK(base.ref_type == ReferenceType::kUnqualified ||
        base.ref_type == ReferenceType::kImmediate)
      << "Inconsistent reference type: " << base.ref_type;
  const absl::string_view key(base.identifier);
  const auto found = context->Find(key);
  if (found == context->end()) {
    return DiagnoseMemberSymbolResolutionFailure(key, *context);
  }
  SymbolTableNode &resolved = found->second;

  // If metatype doesn't match what is expected, then fail.
  if (absl::Status status = base.ResolveSymbol(resolved); !status.ok()) {
    return status;
  }
  return &resolved;
}

std::ostream &operator<<(std::ostream &stream, ReferenceType ref_type) {
  static const verible::EnumNameMap<ReferenceType> kReferenceTypeNames({
      // short-hand annotation for identifier reference type
      {"@", ReferenceType::kUnqualified},
      {"!", ReferenceType::kImmediate},
      {"::", ReferenceType::kDirectMember},
      {".", ReferenceType::kMemberOfTypeOfParent},
  });
  return kReferenceTypeNames.Unparse(ref_type, stream);
}

std::ostream &ReferenceComponent::PrintPathComponent(
    std::ostream &stream) const {
  stream << ref_type << identifier;
  if (required_metatype != SymbolMetaType::kUnspecified) {
    stream << '[' << required_metatype << ']';
  }
  return stream;
}

std::ostream &ReferenceComponent::PrintVerbose(std::ostream &stream) const {
  PrintPathComponent(stream) << " -> ";
  if (resolved_symbol == nullptr) {
    return stream << "<unresolved>";
  }
  return stream << ContextFullPath(*resolved_symbol);
}

std::ostream &operator<<(std::ostream &stream,
                         const ReferenceComponent &component) {
  return component.PrintVerbose(stream);
}

void DeclarationTypeInfo::VerifySymbolTableRoot(
    const SymbolTableNode *root) const {
  if (user_defined_type != nullptr) {
    ApplyPreOrder(*user_defined_type, [=](const ReferenceComponent &component) {
      component.VerifySymbolTableRoot(root);
    });
  }
}

absl::string_view SymbolInfo::CreateAnonymousScope(absl::string_view base) {
  const size_t n = anonymous_scope_names.size();
  anonymous_scope_names.emplace_back(std::make_unique<const std::string>(
      // Starting with a non-alpha character guarantees it cannot collide with
      // any user-given identifier.
      absl::StrCat("%", "anon-", base, "-", n)));
  return *anonymous_scope_names.back();
}

std::ostream &operator<<(std::ostream &stream,
                         const DeclarationTypeInfo &decl_type_info) {
  stream << "type-info { ";

  stream << "source: ";
  if (decl_type_info.syntax_origin != nullptr) {
    stream << "\""
           << AutoTruncate{.text = StringSpanOfSymbol(
                               *decl_type_info.syntax_origin),
                           .max_chars = 25}
           << "\"";
  } else {
    stream << "(unknown)";
  }

  stream << ", type ref: ";
  if (decl_type_info.user_defined_type != nullptr) {
    stream << *decl_type_info.user_defined_type;
  } else {
    stream << "(primitive)";
  }

  if (decl_type_info.implicit) {
    stream << ", implicit";
  }

  return stream << " }";
}

void SymbolInfo::VerifySymbolTableRoot(const SymbolTableNode *root) const {
  declared_type.VerifySymbolTableRoot(root);
  for (const auto &local_ref : local_references_to_bind) {
    local_ref.VerifySymbolTableRoot(root);
  }
}

void SymbolInfo::Resolve(const SymbolTableNode &context,
                         std::vector<absl::Status> *diagnostics) {
  for (auto &local_ref : local_references_to_bind) {
    local_ref.Resolve(context, diagnostics);
  }
}

void SymbolInfo::ResolveLocally(const SymbolTableNode &context) {
  for (auto &local_ref : local_references_to_bind) {
    local_ref.ResolveLocally(context);
  }
}

std::ostream &SymbolInfo::PrintDefinition(std::ostream &stream,
                                          size_t indent) const {
  // print everything except local_references_to_bind
  const verible::Spacer wrap(indent);
  stream << wrap << "metatype: " << metatype << std::endl;
  if (file_origin != nullptr) {
    stream << wrap << "file: " << file_origin->ResolvedPath() << std::endl;
  }
  // declared_type only makes sense for elements with potentially user-defined
  // types, and not for language element declarations like modules and classes.
  if (metatype == SymbolMetaType::kDataNetVariableInstance) {
    stream << wrap << declared_type << std::endl;
  }
  return stream;
}

std::ostream &SymbolInfo::PrintReferences(std::ostream &stream,
                                          size_t indent) const {
  // only print local_references_to_bind
  // TODO: support indentation
  std::string newline_wrap(indent + 1, ' ');
  newline_wrap.front() = '\n';
  stream << "refs:";
  // When there's at most 1 reference, print more compactly.
  if (local_references_to_bind.size() > 1) {
    stream << newline_wrap;
  } else {
    stream << ' ';
  }
  stream << absl::StrJoin(local_references_to_bind, newline_wrap,
                          absl::StreamFormatter());
  if (local_references_to_bind.size() > 1) stream << newline_wrap;
  return stream;
}

SymbolInfo::references_map_view_type
SymbolInfo::LocalReferencesMapViewForTesting() const {
  references_map_view_type map_view;
  for (const auto &local_ref : local_references_to_bind) {
    CHECK(!local_ref.Empty()) << "Never add empty DependentReferences.";
    map_view[local_ref.components->Value().identifier].emplace(&local_ref);
  }
  return map_view;
}

void SymbolTable::CheckIntegrity() const {
  const SymbolTableNode *root = &symbol_table_root_;
  symbol_table_root_.ApplyPreOrder(
      [=](const SymbolInfo &s) { s.VerifySymbolTableRoot(root); });
}

void SymbolTable::Resolve(std::vector<absl::Status> *diagnostics) {
  const absl::Time start = absl::Now();
  symbol_table_root_.ApplyPreOrder(
      [=](SymbolTableNode &node) { node.Value().Resolve(node, diagnostics); });
  VLOG(1) << "SymbolTable::Resolve took " << (absl::Now() - start);
}

void SymbolTable::ResolveLocallyOnly() {
  symbol_table_root_.ApplyPreOrder(
      [=](SymbolTableNode &node) { node.Value().ResolveLocally(node); });
}

std::ostream &SymbolTable::PrintSymbolDefinitions(std::ostream &stream) const {
  return symbol_table_root_.PrintTree(
      stream,
      [](std::ostream &s, const SymbolInfo &sym,
         size_t indent) -> std::ostream & {
        return sym.PrintDefinition(s << std::endl, indent + 4 /* wrap */)
               << verible::Spacer(indent);
      });
}

std::ostream &SymbolTable::PrintSymbolReferences(std::ostream &stream) const {
  return symbol_table_root_.PrintTree(stream,
                                      [](std::ostream &s, const SymbolInfo &sym,
                                         size_t indent) -> std::ostream & {
                                        return sym.PrintReferences(
                                            s, indent + 4 /* wrap */);
                                      });
}

static void ParseFileAndBuildSymbolTable(
    VerilogSourceFile *source, SymbolTable *symbol_table,
    VerilogProject *project, std::vector<absl::Status> *diagnostics) {
  const auto parse_status = source->Parse();
  if (!parse_status.ok()) diagnostics->push_back(parse_status);
  // Continue, in case syntax-error recovery left a partial syntax tree.

  // Amend symbol table by analyzing this translation unit.
  const std::vector<absl::Status> statuses =
      BuildSymbolTable(*source, symbol_table, project);
  // Forward diagnostics.
  diagnostics->insert(diagnostics->end(), statuses.begin(), statuses.end());
}

void SymbolTable::Build(std::vector<absl::Status> *diagnostics) {
  const absl::Time start = absl::Now();
  for (auto &translation_unit : *project_) {
    ParseFileAndBuildSymbolTable(translation_unit.second.get(), this, project_,
                                 diagnostics);
  }
  VLOG(1) << "SymbolTable::Build() took " << (absl::Now() - start);
}

void SymbolTable::BuildSingleTranslationUnit(
    absl::string_view referenced_file_name,
    std::vector<absl::Status> *diagnostics) {
  const auto translation_unit_or_status =
      project_->OpenTranslationUnit(referenced_file_name);
  if (!translation_unit_or_status.ok()) {
    diagnostics->push_back(translation_unit_or_status.status());
    return;
  }
  VerilogSourceFile *translation_unit = *translation_unit_or_status;

  ParseFileAndBuildSymbolTable(translation_unit, this, project_, diagnostics);
}

std::vector<absl::Status> BuildSymbolTable(const VerilogSourceFile &source,
                                           SymbolTable *symbol_table,
                                           VerilogProject *project) {
  VLOG(2) << __FUNCTION__ << " " << source.ResolvedPath();
  const auto *text_structure = source.GetTextStructure();
  if (text_structure == nullptr) return std::vector<absl::Status>();
  const auto &syntax_tree = text_structure->SyntaxTree();
  if (syntax_tree == nullptr) return std::vector<absl::Status>();

  SymbolTable::Builder builder(source, symbol_table, project);
  syntax_tree->Accept(&builder);
  return builder.TakeDiagnostics();  // move
}

}  // namespace verilog
