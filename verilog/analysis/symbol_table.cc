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

#include "verilog/analysis/symbol_table.h"

#include <iostream>
#include <sstream>
#include <stack>

#include "absl/status/status.h"
#include "absl/strings/str_join.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/token_info.h"
#include "common/text/tree_context_visitor.h"
#include "common/text/tree_utils.h"
#include "common/text/visitors.h"
#include "common/util/enum_flags.h"
#include "common/util/logging.h"
#include "common/util/value_saver.h"
#include "verilog/CST/class.h"
#include "verilog/CST/declaration.h"
#include "verilog/CST/functions.h"
#include "verilog/CST/module.h"
#include "verilog/CST/net.h"
#include "verilog/CST/package.h"
#include "verilog/CST/parameters.h"
#include "verilog/CST/port.h"
#include "verilog/CST/seq_block.h"
#include "verilog/CST/statement.h"
#include "verilog/CST/type.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/parser/verilog_parser.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {

using verible::SyntaxTreeLeaf;
using verible::SyntaxTreeNode;
using verible::TokenInfo;
using verible::TreeContextVisitor;
using verible::ValueSaver;

// Root SymbolTableNode has no key, but we identify it as "$root"
static constexpr absl::string_view kRoot("$root");

std::ostream& SymbolTableNodeFullPath(std::ostream& stream,
                                      const SymbolTableNode& node) {
  if (node.Parent() != nullptr) {
    SymbolTableNodeFullPath(stream, *node.Parent()) << "::" << *node.Key();
  } else {
    stream << kRoot;
  }
  return stream;
}

static std::string ContextFullPath(const SymbolTableNode& context) {
  std::ostringstream stream;
  SymbolTableNodeFullPath(stream, context);
  return stream.str();
}

std::ostream& ReferenceNodeFullPath(std::ostream& stream,
                                    const ReferenceComponentNode& node) {
  if (node.Parent() != nullptr) {
    ReferenceNodeFullPath(stream, *node.Parent());
  }
  return stream << node.Value();
}

// Only for internal diagnostics.
static std::ostream& operator<<(std::ostream& stream,
                                const ReferenceComponentNode& node) {
  return ReferenceNodeFullPath(stream, node);
}

// Validates iterator/pointer stability when calling VectorTree::NewChild.
// Detects unwanted reallocation.
static void CheckedNewChildReferenceNode(ReferenceComponentNode* parent,
                                         const ReferenceComponent& component) {
  const auto* saved_begin = parent->Children().data();
  parent->NewChild(component);  // copy
  if (parent->Children().size() > 1) {
    // Check that iterators/pointers were not invalidated by a reallocation.
    CHECK_EQ(parent->Children().data(), saved_begin)
        << "Reallocation invalidated pointers to reference nodes at " << *parent
        << ".  Fix: pre-allocate child nodes.";
  }
  // Otherwise, this first node had no prior siblings, so no need to check.
}

class SymbolTable::Builder : public TreeContextVisitor {
 public:
  Builder(const VerilogSourceFile& source, SymbolTable* symbol_table)
      : source_(&source),
        token_context_(source_->GetTextStructure()->Contents(),
                       [](std::ostream& stream, int e) {
                         stream << verilog_symbol_name(e);
                       }),
        symbol_table_(symbol_table),
        current_scope_(&symbol_table_->MutableRoot()) {}

  std::vector<absl::Status> TakeDiagnostics() {
    return std::move(diagnostics_);
  }

 private:  // methods
  void Visit(const SyntaxTreeNode& node) final {
    const auto tag = static_cast<NodeEnum>(node.Tag().tag);
    VLOG(1) << __FUNCTION__ << " [node]: " << tag;
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
      case NodeEnum::kPortDeclaration:  // fall-through
      case NodeEnum::kNetDeclaration:   // fall-through
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
      case NodeEnum::kReferenceCallBase:
        DescendReferenceExpression(node);
        break;
      case NodeEnum::kActualParameterList:
        DescendActualParameterList(node);
        break;
      case NodeEnum::kPortActualList:
        DescendPortActualList(node);
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
      default:
        Descend(node);
        break;
    }
    VLOG(1) << "end of " << __FUNCTION__ << " [node]: " << tag;
  }

  // This overload enters 'scope' for the duration of the call.
  // New declared symbols will belong to that scope.
  void Descend(const SyntaxTreeNode& node, SymbolTableNode& scope) {
    const ValueSaver<SymbolTableNode*> save_scope(&current_scope_, &scope);
    Descend(node);
  }

  void Descend(const SyntaxTreeNode& node) {
    TreeContextVisitor::Visit(node);  // maintains syntax tree Context() stack.
  }

  // RAII-class balance the Builder::references_builders_ stack.
  // The work of moving collecting references into the current scope is done in
  // the destructor.
  class CaptureDependentReference {
   public:
    CaptureDependentReference(Builder* builder) : builder_(builder) {
      // Push stack space to capture references.
      builder_->reference_builders_.emplace(/* DependentReferences */);
    }

    ~CaptureDependentReference() {
      // This completes the capture of a chain of dependent references.
      // Ref() can be empty if the subtree doesn't reference any identifiers.
      // Empty refs are non-actionable and must be excluded.
      DependentReferences& ref(Ref());
      if (!ref.Empty()) {
        builder_->current_scope_->Value().local_references_to_bind.emplace_back(
            std::move(ref));
      }
      builder_->reference_builders_.pop();
    }

    // Returns the chain of dependent references that were built.
    DependentReferences& Ref() const {
      return builder_->reference_builders_.top();
    }

   private:
    Builder* builder_;
  };

  void DescendReferenceExpression(const SyntaxTreeNode& reference) {
    // capture exressions referenced from the current scope
    const CaptureDependentReference capture(this);

    // subexpressions' references will be collected before this one
    Descend(reference);  // no scope change
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
  void DescendDataType(const SyntaxTreeNode& data_type_node) {
    VLOG(1) << __FUNCTION__ << verible::StringSpanOfSymbol(data_type_node);
    const CaptureDependentReference capture(this);

    {
      // Clearing declaration_type_info_ prevents nested types from being
      // captured.  e.g. in "A_type#(B_type)", "B_type" will beget a chain of
      // DependentReferences in the current context, but will not be involved
      // with the current declaration.
      const ValueSaver<DeclarationTypeInfo*> not_decl_type(
          &declaration_type_info_, nullptr);

      // Inform that named parameter identifiers will yield parallel children
      // from this reference branch point.  Start this out as nullptr, and set
      // it once an unqualified identifier is encountered that starts a
      // reference tree.
      const ValueSaver<ReferenceComponentNode*> set_branch(
          &reference_branch_point_, nullptr);

      Descend(data_type_node);
      // declaration_type_info_ will be restored after this closes.
    }

    if (declaration_type_info_ != nullptr) {
      // This is the declared type we want to capture.
      declaration_type_info_->syntax_origin = &data_type_node;
      if (!capture.Ref()
               .Empty()) {  // then some user-defined type was referenced
        declaration_type_info_->user_defined_type = capture.Ref().LastLeaf();
      }
    }

    // In all cases, a type is being referenced from the current scope, so add
    // it to the list of references to resolve (done by 'capture').
    VLOG(1) << "end of " << __FUNCTION__;
  }

  void DescendActualParameterList(const SyntaxTreeNode& node) {
    // Pre-allocate siblings to guarantee pointer/iterator stability.
    // FindAll* will also catch actual port connections inside preprocessing
    // conditionals.
    const size_t num_ports = FindAllNamedParams(node).size();
    ABSL_DIE_IF_NULL(reference_branch_point_)->Children().reserve(num_ports);
    Descend(node);
  }

  void DescendPortActualList(const SyntaxTreeNode& node) {
    // Pre-allocate siblings to guarantee pointer/iterator stability.
    // FindAll* will also catch actual port connections inside preprocessing
    // conditionals.
    const size_t num_ports = FindAllActualNamedPort(node).size();
    ABSL_DIE_IF_NULL(reference_branch_point_)->Children().reserve(num_ports);
    Descend(node);
  }

  void HandleIdentifier(const SyntaxTreeLeaf& leaf) {
    const absl::string_view text = leaf.get().text();
    if (Context().DirectParentIs(NodeEnum::kParamType)) {
      // This identifier declares a parameter.
      EmplaceTypedElementInCurrentScope(leaf, text, SymbolType::kParameter);
      return;
    }
    if (Context().DirectParentsAre(
            {NodeEnum::kUnqualifiedId, NodeEnum::kPortDeclaration})) {
      // This identifier declares a (non-parameter) port (of a module,
      // function, task).
      EmplaceTypedElementInCurrentScope(leaf, text,
                                        SymbolType::kDataNetVariableInstance);
      // TODO(fangism): Add attributes to distinguish public ports from
      // private internals members.
      return;
    }

    // Capture only referencing identifiers, omit declarative identifiers.
    // This is set up when traversing references, e.g. types, expressions.
    if (reference_builders_.empty()) return;

    // In DeclareInstance(), we already planted a self-reference that is
    // resolved to the instance being declared.
    if (Context().DirectParentIs(NodeEnum::kGateInstance)) return;

    // Building a reference, possible part of a chain or qualified
    // reference.
    DependentReferences& ref(reference_builders_.top());

    const ReferenceComponent new_ref{
        .identifier = text,
        .ref_type = InferReferenceType(),
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

    // For all other cases, grow the reference chain deeper.
    ref.PushReferenceComponent(new_ref);
    if (reference_branch_point_ == nullptr) {
      // For type references, which may contained named parameters,
      // when encountering the first unqualified reference, establish its
      // reference node as the point from which named parameter references
      // get added as siblings.
      // e.g. "A#(.B(...), .C(...))" would result in a reference tree:
      //   A -+- ::B
      //      |
      //      \- ::C
      reference_branch_point_ = ref.components.get();
    }
  }

  void Visit(const SyntaxTreeLeaf& leaf) final {
    const auto tag = leaf.Tag().tag;
    VLOG(1) << __FUNCTION__ << " [leaf]: " << VerboseToken(leaf.get());
    switch (tag) {
      case verilog_tokentype::SymbolIdentifier:
        HandleIdentifier(leaf);
        break;

      case verilog_tokentype::TK_SCOPE_RES:  // "::"
      case '.':
        last_hierarchy_operator_ = &leaf.get();
        break;

      default:
        break;
    }
    VLOG(1) << "end " << __FUNCTION__ << " [leaf]:" << VerboseToken(leaf.get());
  }

  // Distinguish between '.' and "::" hierarchy in reference components.
  ReferenceType InferReferenceType() const {
    CHECK(!reference_builders_.empty())
        << "Not currently in a reference context.";
    const DependentReferences& ref(reference_builders_.top());
    if (ref.Empty() || last_hierarchy_operator_ == nullptr) {
      // The root component is always treated as unqualified.
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

  // Creates a named element in the current scope.
  // Suitable for SystemVerilog language elements: functions, tasks, packages,
  // classes, modules, etc...
  SymbolTableNode& EmplaceElementInCurrentScope(const verible::Symbol& element,
                                                absl::string_view name,
                                                SymbolType type) {
    const auto p =
        current_scope_->TryEmplace(name, SymbolInfo{
                                             .type = type,
                                             .file_origin = source_,
                                             .syntax_origin = &element,
                                         });
    if (!p.second) {
      DiagnoseSymbolAlreadyExists(name);
    }
    return p.first->second;  // scope of the new (or pre-existing symbol)
  }

  // Creates a named typed element in the current scope.
  // Suitable for SystemVerilog language elements: nets, parameter, variables,
  // instances.
  SymbolTableNode& EmplaceTypedElementInCurrentScope(
      const verible::Symbol& element, absl::string_view name, SymbolType type) {
    VLOG(1) << __FUNCTION__ << ": " << name;
    const auto p = current_scope_->TryEmplace(
        name,
        SymbolInfo{
            .type = type,
            .file_origin = source_,
            .syntax_origin = &element,
            // associate this instance with its declared type
            .declared_type = *ABSL_DIE_IF_NULL(declaration_type_info_),  // copy
        });
    if (!p.second) {
      DiagnoseSymbolAlreadyExists(name);
    }
    VLOG(1) << "end of " << __FUNCTION__ << ": " << name;
    return p.first->second;  // scope of the new (or pre-existing symbol)
  }

  // Creates a named element in the current scope, and traverses its subtree
  // inside the new element's scope.
  void DeclareScopedElementAndDescend(const SyntaxTreeNode& element,
                                      absl::string_view name, SymbolType type) {
    SymbolTableNode& enter_scope(
        EmplaceElementInCurrentScope(element, name, type));
    Descend(element, enter_scope);
  }

  void DeclareModule(const SyntaxTreeNode& module) {
    DeclareScopedElementAndDescend(module, GetModuleName(module).get().text(),
                                   SymbolType::kModule);
  }

  absl::string_view GetScopeNameFromGenerateBody(const SyntaxTreeNode& body) {
    if (body.MatchesTag(NodeEnum::kGenerateBlock)) {
      const TokenInfo* label =
          GetBeginLabelTokenInfo(GetGenerateBlockBegin(body));
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

  void DeclareGenerateIf(const SyntaxTreeNode& generate_if) {
    const SyntaxTreeNode& body(GetIfClauseGenerateBody(generate_if));

    DeclareScopedElementAndDescend(
        generate_if, GetScopeNameFromGenerateBody(body), SymbolType::kGenerate);
  }

  void DeclareGenerateElse(const SyntaxTreeNode& generate_else) {
    const SyntaxTreeNode& body(GetElseClauseGenerateBody(generate_else));

    if (body.MatchesTag(NodeEnum::kConditionalGenerateConstruct)) {
      // else-if chained.  Flatten the else block by not creating a new scope
      // and let the if-clause inside create a scope directly under the current
      // scope.
      Descend(body);
    } else {
      DeclareScopedElementAndDescend(generate_else,
                                     GetScopeNameFromGenerateBody(body),
                                     SymbolType::kGenerate);
    }
  }

  void DeclarePackage(const SyntaxTreeNode& package) {
    DeclareScopedElementAndDescend(package, GetPackageNameToken(package).text(),
                                   SymbolType::kPackage);
  }

  void DeclareClass(const SyntaxTreeNode& class_node) {
    DeclareScopedElementAndDescend(
        class_node, GetClassName(class_node).get().text(), SymbolType::kClass);
  }

  // TODO: functions and tasks, which could appear as out-of-line definitions.

  void DeclareParameter(const SyntaxTreeNode& param_decl_node) {
    CHECK(param_decl_node.MatchesTag(NodeEnum::kParamDeclaration));
    DeclarationTypeInfo decl_type_info;
    // Set declaration_type_info_ to capture any user-defined type used to
    // declare data/variables/instances.
    const ValueSaver<DeclarationTypeInfo*> save_type(&declaration_type_info_,
                                                     &decl_type_info);
    Descend(param_decl_node);
  }

  // Declares one or more variables/instances/nets.
  void DeclareData(const SyntaxTreeNode& data_decl_node) {
    DeclarationTypeInfo decl_type_info;
    // Set declaration_type_info_ to capture any user-defined type used to
    // declare data/variables/instances.
    const ValueSaver<DeclarationTypeInfo*> save_type(&declaration_type_info_,
                                                     &decl_type_info);
    Descend(data_decl_node);
  }

  // Declare one (of potentially multiple) instances in a single declaration
  // statement.
  void DeclareInstance(const SyntaxTreeNode& instance) {
    const absl::string_view instance_name(
        GetModuleInstanceNameTokenInfoFromGateInstance(instance).text());
    const SymbolTableNode& new_instance(EmplaceTypedElementInCurrentScope(
        instance, instance_name, SymbolType::kDataNetVariableInstance));

    // Also create a DependentReferences chain starting with this named instance
    // so that named port references are direct children of this reference root.
    // This is a self-reference.
    const CaptureDependentReference capture(this);
    capture.Ref().PushReferenceComponent(ReferenceComponent{
        .identifier = instance_name,
        .ref_type = ReferenceType::kUnqualified,
        // Start with its type already resolved to the node we just declared.
        .resolved_symbol = &new_instance,
    });

    // Inform that named port identifiers will yield parallel children from
    // this reference branch point.
    const ValueSaver<ReferenceComponentNode*> set_branch(
        &reference_branch_point_, capture.Ref().components.get());

    // No change of scope, but named ports will be resolved with respect to the
    // decl_type_info's scope later.
    Descend(instance);  // visit parameter/port connections, etc.
  }

  void DeclareNet(const SyntaxTreeNode& net_variable) {
    const absl::string_view net_name(
        GetNameLeafOfNetVariable(net_variable).get().text());
    EmplaceTypedElementInCurrentScope(net_variable, net_name,
                                      SymbolType::kDataNetVariableInstance);
    Descend(net_variable);
  }

  void DeclareRegister(const SyntaxTreeNode& reg_variable) {
    const absl::string_view net_name(
        GetNameLeafOfRegisterVariable(reg_variable).get().text());
    EmplaceTypedElementInCurrentScope(reg_variable, net_name,
                                      SymbolType::kDataNetVariableInstance);
    Descend(reg_variable);
  }

  void DiagnoseSymbolAlreadyExists(absl::string_view name) {
    diagnostics_.push_back(absl::AlreadyExistsError(
        absl::StrCat("Symbol \"", name, "\" is already defined in the ",
                     CurrentScopeFullPath(), " scope.")));
  }

  std::string CurrentScopeFullPath() const {
    return ContextFullPath(*current_scope_);
  }

  verible::TokenWithContext VerboseToken(const TokenInfo& token) const {
    return verible::TokenWithContext{token, token_context_};
  }

 private:  // data
  // Points to the source file that is the origin of symbols.
  // This changes when opening preprocess-included files.
  // TODO(fangism): maintain a vector/stack of these for richer diagnostics
  const VerilogSourceFile* const source_;

  // For human-readable debugging.
  const verible::TokenInfo::Context token_context_;

  // The symbol table to build, never nullptr.
  SymbolTable* const symbol_table_;

  // The remaining fields are mutable state:

  // This is the current scope where encountered definitions register their
  // symbols, never nullptr.
  // There is no need to maintain a stack because SymbolTableNodes already link
  // to their parents.
  SymbolTableNode* current_scope_;

  // Stack of references.
  // A stack is needed to support nested type references like "A#(B(#(C)))",
  // and nested expressions like "f(g(h))"
  std::stack<DependentReferences> reference_builders_;

  // When creating branched references, like with instances' named ports,
  // set this to the nearest branch point.
  // This will signal to the reference builder that parallel children
  // are to be added, as opposed to deeper descendants.
  ReferenceComponentNode* reference_branch_point_ = nullptr;

  // For a data/instance/variable declaration statement, this is the declared
  // type (could be primitive or named-user-defined).
  // Set this type before traversing declared instances and variables to capture
  // the type of the declaration.  Unset this to prevent type capture.
  // Such declarations cannot nest, so a stack is not needed.
  DeclarationTypeInfo* declaration_type_info_ = nullptr;

  // Update to either "::" or '.'.
  const TokenInfo* last_hierarchy_operator_ = nullptr;

  // Collection of findings that might be considered compiler/tool errors in a
  // real toolchain.  For example: attempt to redefine symbol.
  std::vector<absl::Status> diagnostics_;
};

void ReferenceComponent::VerifySymbolTableRoot(
    const SymbolTableNode* root) const {
  if (resolved_symbol != nullptr) {
    CHECK_EQ(resolved_symbol->Root(), root)
        << "Resolved symbols must point to a node in the same SymbolTable.";
  }
}

const ReferenceComponentNode* DependentReferences::LastLeaf() const {
  if (components == nullptr) return nullptr;
  const ReferenceComponentNode* node = components.get();
  while (!node->is_leaf()) node = &node->Children().front();
  return node;
}

void DependentReferences::PushReferenceComponent(
    const ReferenceComponent& component) {
  VLOG(3) << __FUNCTION__ << ", id: " << component.identifier;
  if (Empty()) {
    components = absl::make_unique<ReferenceComponentNode>(component);  // copy
  } else {
    // Find the deepest leaf node, and grow a new child from that.
    ReferenceComponentNode* node = components.get();
    while (!node->is_leaf()) node = &node->Children().front();
    // This is a leaf node, and this is the first child.
    CheckedNewChildReferenceNode(node, component);
  }
  VLOG(3) << "end of " << __FUNCTION__;
}

void DependentReferences::VerifySymbolTableRoot(
    const SymbolTableNode* root) const {
  if (components != nullptr) {
    components->ApplyPreOrder([=](const ReferenceComponent& component) {
      component.VerifySymbolTableRoot(root);
    });
  }
}

// Search up-scope, stopping at the first symbol found in the nearest scope.
static const SymbolTableNode* LookupSymbolUpwards(
    const SymbolTableNode& context, absl::string_view symbol) {
  const SymbolTableNode* current_context = &context;
  while (current_context != nullptr) {
    // TODO: lookup imported namespaces and symbols
    const auto found = current_context->Find(symbol);
    if (found != current_context->end()) return &found->second;
    current_context = current_context->Parent();
  }
  return nullptr;  // resolution failed
}

static absl::Status DiagnoseUnqualifiedSymbolResolutionFailure(
    absl::string_view name, const SymbolTableNode& context) {
  return absl::NotFoundError(absl::StrCat("Unable to resolve symbol \"", name,
                                          "\" from context ",
                                          ContextFullPath(context), "."));
}

static absl::Status DiagnoseMemberSymbolResolutionFailure(
    absl::string_view name, const SymbolTableNode& context) {
  const absl::string_view context_name =
      context.Parent() == nullptr ? kRoot : *context.Key();
  return absl::NotFoundError(absl::StrCat(
      "No member symbol \"", name, "\" in parent scope ", context_name, "."));
}

// This is the primary function that resolves references.
// Dependent (parent) nodes must already be resolved before attempting to
// resolve children references.
static void ResolveReferenceComponentNode(
    ReferenceComponentNode& node, const SymbolTableNode& context,
    std::vector<absl::Status>* diagnostics) {
  ReferenceComponent& component(node.Value());
  VLOG(2) << __FUNCTION__ << ": " << component;
  if (component.resolved_symbol != nullptr) return;  // already bound
  const absl::string_view key(component.identifier);

  if (node.Parent() == nullptr) {
    // root node: lookup this symbol from its context upward
    CHECK_EQ(component.ref_type, ReferenceType::kUnqualified);
    const SymbolTableNode* resolved = LookupSymbolUpwards(context, key);
    if (resolved == nullptr) {
      diagnostics->emplace_back(
          DiagnoseUnqualifiedSymbolResolutionFailure(key, context));
    }
    component.resolved_symbol = resolved;
  } else {
    // non-root node:
    // use parent's scope (if resolved successfully) to resolve this node.
    switch (component.ref_type) {
      case ReferenceType::kDirectMember: {
        const ReferenceComponent& parent_component(node.Parent()->Value());

        const SymbolTableNode* parent_scope = parent_component.resolved_symbol;
        if (parent_scope == nullptr) return;  // leave this subtree unresolved

        const auto found = parent_scope->Find(key);
        if (found == parent_scope->end()) {
          diagnostics->emplace_back(
              DiagnoseMemberSymbolResolutionFailure(key, *parent_scope));
        } else {
          component.resolved_symbol = &found->second;
        }
        break;
      }
      case ReferenceType::kMemberOfTypeOfParent: {
        // Get the type of the object from the parent component.
        const ReferenceComponent& parent_component(node.Parent()->Value());
        const SymbolTableNode* parent_scope = parent_component.resolved_symbol;
        if (parent_scope == nullptr) return;  // leave this subtree unresolved

        const DeclarationTypeInfo& type_info =
            parent_scope->Value().declared_type;
        // Primitive types do not have members.
        if (type_info.user_defined_type == nullptr) return;

        // This referenced object's scope is not a parent of this node, and
        // this, not guaranteed to have been resolved first.
        // TODO: cache/memoize the parent's resolved type.
        const SymbolTableNode* type_scope =
            type_info.user_defined_type->Value().resolved_symbol;
        if (type_scope == nullptr) return;

        const auto found = type_scope->Find(key);
        if (found == type_scope->end()) {
          diagnostics->emplace_back(
              DiagnoseMemberSymbolResolutionFailure(key, *type_scope));
        } else {
          component.resolved_symbol = &found->second;
        }
        break;
      }
      case ReferenceType::kUnqualified:
        CHECK_NE(component.ref_type, ReferenceType::kUnqualified);
    }
  }
  VLOG(2) << "end of " << __FUNCTION__;
}

ReferenceComponentMap ReferenceComponentNodeMapView(
    const ReferenceComponentNode& node) {
  ReferenceComponentMap map_view;
  for (const auto& child : node.Children()) {
    map_view.emplace(std::make_pair(child.Value().identifier, &child));
  }
  return map_view;
}

void DependentReferences::Resolve(const SymbolTableNode& context,
                                  std::vector<absl::Status>* diagnostics) {
  VLOG(1) << __FUNCTION__;
  if (components == nullptr) return;
  // References are arranged in dependency trees.
  // Parent node references must be resolved before children nodes,
  // hence a pre-order traversal.
  components->ApplyPreOrder(
      [&context, diagnostics](ReferenceComponentNode& node) {
        ResolveReferenceComponentNode(node, context, diagnostics);
        // TODO: minor optimization, when resolution for a node fails,
        // skip checking that node's subtree; early terminate.
      });
  VLOG(1) << "end of " << __FUNCTION__;
}

std::ostream& operator<<(std::ostream& stream, ReferenceType ref_type) {
  static const verible::EnumNameMap<ReferenceType> kReferenceTypeNames({
      // short-hand annotation for identifier reference type
      {"@", ReferenceType::kUnqualified},
      {"::", ReferenceType::kDirectMember},
      {".", ReferenceType::kMemberOfTypeOfParent},
  });
  return kReferenceTypeNames.Unparse(ref_type, stream);
}

std::ostream& operator<<(std::ostream& stream,
                         const ReferenceComponent& component) {
  return stream << component.ref_type << component.identifier;
}

void DeclarationTypeInfo::VerifySymbolTableRoot(
    const SymbolTableNode* root) const {
  if (user_defined_type != nullptr) {
    user_defined_type->ApplyPreOrder([=](const ReferenceComponent& component) {
      component.VerifySymbolTableRoot(root);
    });
  }
}

absl::string_view SymbolInfo::CreateAnonymousScope(absl::string_view base) {
  const size_t n = anonymous_scope_names.size();
  anonymous_scope_names.emplace_back(absl::make_unique<const std::string>(
      // Starting with a non-alpha character guarantees it cannot collide with
      // any user-given identifier.
      absl::StrCat("%", "anon-", base, "-", n)));
  return *anonymous_scope_names.back();
}

void SymbolInfo::VerifySymbolTableRoot(const SymbolTableNode* root) const {
  declared_type.VerifySymbolTableRoot(root);
  for (const auto& local_ref : local_references_to_bind) {
    local_ref.VerifySymbolTableRoot(root);
  }
}

void SymbolInfo::Resolve(const SymbolTableNode& context,
                         std::vector<absl::Status>* diagnostics) {
  for (auto& local_ref : local_references_to_bind) {
    local_ref.Resolve(context, diagnostics);
  }
}

SymbolInfo::references_map_view_type
SymbolInfo::LocalReferencesMapViewForTesting() const {
  references_map_view_type map_view;
  for (const auto& local_ref : local_references_to_bind) {
    CHECK(!local_ref.Empty()) << "Never add empty DependentReferences.";
    map_view[local_ref.components->Value().identifier].emplace(&local_ref);
  }
  return map_view;
}

void SymbolTable::CheckIntegrity() const {
  const SymbolTableNode* root = &symbol_table_root_;
  symbol_table_root_.ApplyPreOrder(
      [=](const SymbolInfo& s) { s.VerifySymbolTableRoot(root); });
}

void SymbolTable::Resolve(std::vector<absl::Status>* diagnostics) {
  symbol_table_root_.ApplyPreOrder(
      [=](SymbolTableNode& node) { node.Value().Resolve(node, diagnostics); });
}

std::vector<absl::Status> BuildSymbolTable(const VerilogSourceFile& source,
                                           SymbolTable* symbol_table) {
  const auto* text_structure = source.GetTextStructure();
  if (text_structure == nullptr) return std::vector<absl::Status>();
  const auto& syntax_tree = text_structure->SyntaxTree();
  if (syntax_tree == nullptr) return std::vector<absl::Status>();

  SymbolTable::Builder builder(source, symbol_table);
  syntax_tree->Accept(&builder);
  return builder.TakeDiagnostics();
}

}  // namespace verilog
