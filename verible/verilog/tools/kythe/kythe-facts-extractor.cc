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

#include "verible/verilog/tools/kythe/kythe-facts-extractor.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/container/node_hash_set.h"
#include "absl/hash/hash.h"
#include "absl/strings/str_cat.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "verible/common/util/auto-pop-stack.h"
#include "verible/common/util/logging.h"
#include "verible/common/util/tree-operations.h"
#include "verible/verilog/analysis/verilog-project.h"
#include "verible/verilog/tools/kythe/indexing-facts-tree.h"
#include "verible/verilog/tools/kythe/kythe-facts.h"
#include "verible/verilog/tools/kythe/kythe-schema-constants.h"
#include "verible/verilog/tools/kythe/scope-resolver.h"
#include "verible/verilog/tools/kythe/verilog-extractor-indexing-fact-type.h"

namespace verilog {
namespace kythe {
namespace {

// Returns the file path of the file from the given indexing facts tree node
// tagged with kFile.
std::string_view GetFilePathFromRoot(const IndexingFactNode &root) {
  CHECK_EQ(root.Value().GetIndexingFactType(), IndexingFactType::kFile);
  return root.Value().Anchors()[0].Text();
}

}  // namespace

// KytheFactsExtractor processes indexing facts for a single file.
// Responsible for traversing IndexingFactsTree and processing its different
// nodes to produce kythe indexing facts.
// Iteratively extracts facts and keeps running until no new facts are found in
// the last iteration.
class KytheFactsExtractor {
 public:
  KytheFactsExtractor(std::string_view file_path, std::string_view corpus,
                      KytheOutput *facts_output,
                      ScopeResolver *previous_files_scopes)
      : file_path_(file_path),
        corpus_(corpus),
        facts_output_(facts_output),
        scope_resolver_(previous_files_scopes) {}

 private:
  // Container with a stack of VNames to hold context of VNames during traversal
  // of an IndexingFactsTree.
  // This is used to generate to VNames inside the current scope.
  // e.g.
  // module foo();
  //  wire x; ==> "foo#x"
  // endmodule: foo
  //
  // module bar();
  //  wire x; ==> "bar#x"
  // endmodule: bar
  class VNameContext : public verible::AutoPopStack<const VName *> {
   public:
    using base_type = verible::AutoPopStack<const VName *>;

    // member class to handle push and pop of stack safely
    using AutoPop = base_type::AutoPop;

    // returns the top VName of the stack
    const VName &top() const { return *ABSL_DIE_IF_NULL(base_type::top()); }
  };

  // Returns the full path of the current source file.
  std::string_view FilePath() { return file_path_; }

  // Returns the corpus to which this file belongs.
  std::string_view Corpus() const { return corpus_; }

 public:
  // Extracts kythe facts from the given IndexingFactsTree root. The result is
  // written to Kythe output.
  void ExtractFile(const IndexingFactNode &);

 private:
  // Resolves the tag of the given node and directs the flow to the appropriate
  // function to extract kythe facts for that node. Returns true if any Kythe
  // fact was created.
  bool IndexingFactNodeTagResolver(const IndexingFactNode &);

  // Determines whether to create a scope for this node or not and visits the
  // children.
  void VisitAutoConstructScope(const IndexingFactNode &node,
                               const VName &vname);

  // Add the given VName to vnames_context (to be used in scope relative
  // signatures) and visits the children of the given node creating a new scope
  // for the given node.
  void VisitUsingVName(const IndexingFactNode &node, const VName &);

  // Directs the flow to the children of the given node.
  void Visit(const IndexingFactNode &node);

  // Determines whether or not to create a child of edge between the current
  // node and the previous node.
  void CreateChildOfEdge(IndexingFactType, const VName &);

  // Returns the scope of the parent's type. E.g. in case `my_class
  // my_instance`, `my_instance` gets the definition scope from `my_class` so
  // that `my_instance.method()` can be resolved as `method` exists in the
  // `my_class`s scope.
  std::optional<SignatureDigest> GetParentTypeScope(
      const IndexingFactNode &node) const;

  //=================================================================
  // Declare* methods create facts (some edges) and may introduce new scopes.
  // Reference* methods only create edges, and may not modify scopes' contents.

  // Extracts kythe facts from file node and returns it VName.
  VName DeclareFile(const IndexingFactNode &);

  // Extracts kythe facts for a reference to some user defined data type like
  // class or module.
  void ReferenceDataType(const IndexingFactNode &);

  // Extracts kythe facts for a constant like member in enums.
  VName DeclareConstant(const IndexingFactNode &);

  // Extracts kythe facts for structs or unions.
  VName DeclareStructOrUnion(const IndexingFactNode &);

  // Extracts kythe facts for a type declaration.
  VName DeclareTypedef(const IndexingFactNode &);

  // Extracts kythe facts from interface node and returns it VName.
  VName DeclareInterface(const IndexingFactNode &interface_fact_node);

  // Extracts kythe facts from program node and returns it VName.
  VName DeclareProgram(const IndexingFactNode &program_fact_node);

  // Extracts kythe facts from module named port node e.g("m(.in1(a))").
  void ReferenceModuleNamedPort(const IndexingFactNode &);

  // Extracts kythe facts from named param
  // e.g module_type #(.N(x)) extracts "N";
  void ReferenceNamedParam(const IndexingFactNode &);

  // Extracts kythe facts from module node and returns it VName.
  VName DeclareModule(const IndexingFactNode &);

  // Extracts kythe facts from class node and returns it VName.
  VName DeclareClass(const IndexingFactNode &);

  // Extracts kythe facts from class extends node.
  void ReferenceExtendsInheritance(const IndexingFactNode &);

  // Extracts kythe facts from module instance, class instance, variable
  // definition and param declaration nodes and returns its VName.
  VName DeclareVariable(const IndexingFactNode &node);

  // Extracts kythe facts from a module port reference node.
  void ReferenceVariable(const IndexingFactNode &node);

  // Creates a new anonymous scope for if conditions and loops.
  VName DeclareAnonymousScope(const IndexingFactNode &temp_scope);

  // Extracts kythe facts from a function or task node and returns its VName.
  VName DeclareFunctionOrTask(const IndexingFactNode &function_fact_node);

  // Extracts kythe facts from a function or task call node.
  void ReferenceFunctionOrTaskCall(
      const IndexingFactNode &function_call_fact_node);

  // Extracts kythe facts from a package declaration node and returns its VName.
  VName DeclarePackage(const IndexingFactNode &node);

  // Extracts kythe facts from package import node.
  void ReferencePackageImport(const IndexingFactNode &node);

  // Extracts kythe facts from a macro definition node and returns its VName.
  VName DeclareMacroDefinition(const IndexingFactNode &macro_definition_node);

  // Extracts kythe facts from a macro call node.
  void ReferenceMacroCall(const IndexingFactNode &macro_call_node);

  // Extracts kythe facts from a "`include" node.
  void ReferenceIncludeFile(const IndexingFactNode &include_node);

  // Extracts kythe facts from member reference statement.
  // e.g pkg::member or class::member or class.member
  // The names are treated as anchors e.g:
  // pkg::member => {Anchor(pkg), Anchor(member)}
  // pkg::class_name::var => {Anchor(pkg), Anchor(class_name), Anchor(var)}
  void ReferenceMember(const IndexingFactNode &member_reference_node);

  //============ end of Declare*, Reference* methods ===================

  // Create "ref" edges that point from the given anchor to the given
  // definition.
  void CreateAnchorReference(const Anchor &anchor, const VName &definition);

  // Generates an anchor VName for kythe.
  VName CreateAnchor(const Anchor &);

  // Appends the signatures of previous containing scope vname to make
  // signatures unique relative to scopes.
  Signature CreateScopeRelativeSignature(std::string_view) const;

  // Generates fact strings for Kythe facts.
  // Schema for this fact can be found here:
  // https://kythe.io/docs/schema/writing-an-indexer.html
  void CreateFact(const VName &vname, std::string_view name,
                  std::string_view value);

  // Generates edge strings for Kythe edges.
  // Schema for this edge can be found here:
  // https://kythe.io/docs/schema/writing-an-indexer.html
  void CreateEdge(const VName &source, std::string_view name,
                  const VName &target);

  // Holds the hashes of the output Kythe facts and edges (for deduplication).
  absl::flat_hash_set<int64_t> seen_kythe_hashes_;

  // The full path of the current source file.
  std::string_view file_path_;

  // The corpus to which this file belongs.
  std::string_view corpus_;

  // Output for produced Kythe facts. Not owned.
  KytheOutput *const facts_output_;

  // Keeps track of VNames of ancestors as the visitor traverses the facts
  // tree.
  VNameContext vnames_context_;

  // Keeps track and saves the explored scopes. Used to resolve symbols to their
  // definition.
  ScopeResolver *const scope_resolver_;

  // Location signature backing store.
  // Inside signatures, we record locations as string_views,
  // this is the backing store for assembled strings as
  // location markers such as "@123:456"
  // Needs to be a node_hash_set to provide value stability.
  absl::node_hash_set<std::string> signature_locations_;
};

void StreamKytheFactsEntries(KytheOutput *kythe_output,
                             const IndexingFactNode &file_list,
                             const VerilogProject &project) {
  VLOG(1) << __FUNCTION__;
  // TODO(fangism): re-implement root-level symbol lookup with a proper
  // project-wide symbol table, for efficient lookup.

  // TODO(fangism): infer dependency ordering automatically based on
  // the symbols defined in each file.

  // Process each file in the original listed order.
  ScopeResolver scope_resolver(Signature(""));
  for (const IndexingFactNode &root : file_list.Children()) {
    scope_resolver.SetCurrentScope(Signature(""));
    const absl::Time extraction_start = absl::Now();
    // 'root' corresponds to the fact tree for a particular file.
    // 'file_path' is path-resolved.
    const std::string_view file_path(GetFilePathFromRoot(root));
    VLOG(1) << "child file resolved path: " << file_path;

    // Create facts and edges.
    KytheFactsExtractor kythe_extractor(file_path, project.Corpus(),
                                        kythe_output, &scope_resolver);

    // Output facts and edges.
    kythe_extractor.ExtractFile(root);
    LOG(INFO) << "Extracted Kythe facts of " << file_path << " in "
              << (absl::Now() - extraction_start);
  }

  VLOG(1) << "end of " << __FUNCTION__;
}

void KytheFactsExtractor::ExtractFile(const IndexingFactNode &root) {
  // root corresponds to the indexing tree for a single file.

  // Fixed-point analysis: Repeat fact extraction until no new facts are found.
  // This approach handles cases where symbols can be defined later in the file
  // than their uses, e.g. class member declarations and references.
  while (IndexingFactNodeTagResolver(root)) {
  }
}

std::optional<SignatureDigest> KytheFactsExtractor::GetParentTypeScope(
    const IndexingFactNode &node) const {
  std::string_view node_name = node.Value().Anchors()[0].Text();
  if (node.Parent() == nullptr) {
    return std::nullopt;
  }
  const auto &parent_anchors = node.Parent()->Value().Anchors();
  if (parent_anchors.empty()) {
    VLOG(2) << "GetParentTypeScope for " << node_name << " FAILED -- no parent";
    return std::nullopt;
  }

  SignatureDigest focused_scope = scope_resolver_->CurrentScopeDigest();
  std::optional<ScopedVname> parent_type = std::nullopt;
  for (const auto &parent_anchor : parent_anchors) {
    parent_type = scope_resolver_->FindScopeAndDefinition(parent_anchor.Text(),
                                                          focused_scope);
    if (!parent_type) {
      VLOG(2) << "GetParentTypeScope for " << node_name
              << " FAILED -- no parent type at " << parent_anchor.Text()
              << " within scope " << scope_resolver_->ScopeDebug(focused_scope);
      return std::nullopt;
    }
    focused_scope = parent_type->type_scope;
  }

  if (!parent_type.has_value()) {
    return std::nullopt;
  }
  VLOG(2) << "GetParentTypeScope for " << node_name << " succeeded. Parent: "
          << scope_resolver_->ScopeDebug(parent_type->type_scope);
  return parent_type->type_scope;
}

bool KytheFactsExtractor::IndexingFactNodeTagResolver(
    const IndexingFactNode &node) {
  const size_t previously_extracted_facts_num = seen_kythe_hashes_.size();
  const auto tag = node.Value().GetIndexingFactType();

  // Dispatch a node handler based on the node's tag.
  // This VName is used to keep track of the new generated VName and it will be
  // used in scopes, finding variable definitions and creating childof
  // relations.
  VName vname;
  SignatureDigest current_scope = scope_resolver_->CurrentScopeDigest();
  switch (tag) {
    case IndexingFactType::kFile: {
      vname = DeclareFile(node);
      scope_resolver_->AddDefinitionToCurrentScope(vname);
      break;
    }
    case IndexingFactType::kModule: {
      vname = DeclareModule(node);
      scope_resolver_->AddDefinitionToCurrentScope(vname);
      break;
    }
    case IndexingFactType::kInterface: {
      vname = DeclareInterface(node);
      scope_resolver_->AddDefinitionToCurrentScope(vname);
      break;
    }
    case IndexingFactType::kProgram: {
      vname = DeclareProgram(node);
      scope_resolver_->AddDefinitionToCurrentScope(vname);
      break;
    }
    case IndexingFactType::kClassInstance:
    case IndexingFactType::kModuleInstance:
    case IndexingFactType::kParamDeclaration:
    case IndexingFactType::kVariableDefinition: {
      vname = DeclareVariable(node);
      const auto parent_type = GetParentTypeScope(node);

      if (parent_type) {
        scope_resolver_->AddDefinitionToCurrentScope(vname, *parent_type);
      } else {
        scope_resolver_->AddDefinitionToCurrentScope(vname,
                                                     vname.signature.Digest());
      }
      break;
    }
    case IndexingFactType::kConstant: {
      vname = DeclareConstant(node);
      scope_resolver_->AddDefinitionToCurrentScope(vname, current_scope);
      break;
    }
    case IndexingFactType::kMacro: {
      vname = DeclareMacroDefinition(node);
      scope_resolver_->AddDefinitionToCurrentScope(vname, current_scope);
      break;
    }
    case IndexingFactType::kClass: {
      vname = DeclareClass(node);
      scope_resolver_->AddDefinitionToCurrentScope(vname);
      break;
    }
    case IndexingFactType::kFunctionOrTask:
    case IndexingFactType::kFunctionOrTaskForwardDeclaration:
    case IndexingFactType::kConstructor: {
      vname = DeclareFunctionOrTask(node);
      scope_resolver_->AddDefinitionToCurrentScope(vname);
      break;
    }
    case IndexingFactType::kPackage: {
      vname = DeclarePackage(node);
      scope_resolver_->AddDefinitionToCurrentScope(vname);
      break;
    }
    case IndexingFactType::kStructOrUnion: {
      vname = DeclareStructOrUnion(node);
      scope_resolver_->AddDefinitionToCurrentScope(vname);
      break;
    }
    case IndexingFactType::kAnonymousScope: {
      vname = DeclareAnonymousScope(node);
      break;
    }
    case IndexingFactType::kTypeDeclaration: {
      vname = DeclareTypedef(node);
      scope_resolver_->AddDefinitionToCurrentScope(vname);
      break;
    }
      // end of definition extraction cases.

      // The following cases extract references:
    case IndexingFactType::kDataTypeReference: {
      ReferenceDataType(node);
      break;
    }
    case IndexingFactType::kModuleNamedPort: {
      ReferenceModuleNamedPort(node);
      break;
    }
    case IndexingFactType::kNamedParam: {
      ReferenceNamedParam(node);
      break;
    }
    case IndexingFactType::kExtends: {
      ReferenceExtendsInheritance(node);
      break;
    }
    case IndexingFactType::kVariableReference: {
      ReferenceVariable(node);
      break;
    }
    case IndexingFactType::kFunctionCall: {
      ReferenceFunctionOrTaskCall(node);
      break;
    }
    case IndexingFactType::kPackageImport: {
      ReferencePackageImport(node);
      break;
    }
    case IndexingFactType::kMacroCall: {
      ReferenceMacroCall(node);
      break;
    }
    case IndexingFactType::kMemberReference: {
      ReferenceMember(node);
      break;
    }
    case IndexingFactType::kInclude: {
      ReferenceIncludeFile(node);
      break;
    }
      // end of reference extraction cases.
    default: {
      break;
    }
  }

  CreateChildOfEdge(tag, vname);
  VisitAutoConstructScope(node, vname);

  return seen_kythe_hashes_.size() > previously_extracted_facts_num;
}

void KytheFactsExtractor::CreateChildOfEdge(IndexingFactType tag,
                                            const VName &vname) {
  // Determines whether to create a child of edge to the parent node or not.
  switch (tag) {
    case IndexingFactType::kFile:
    case IndexingFactType::kPackageImport:
    case IndexingFactType::kVariableReference:
    case IndexingFactType::kDataTypeReference:
    case IndexingFactType::kMacroCall:
    case IndexingFactType::kFunctionCall:
    case IndexingFactType::kMacro:
    case IndexingFactType::kModuleNamedPort:
    case IndexingFactType::kMemberReference:
    case IndexingFactType::kInclude:
    case IndexingFactType::kAnonymousScope: {
      break;
    }
    default: {
      if (!vnames_context_.empty()) {
        CreateEdge(vname, kEdgeChildOf, vnames_context_.top());
      }
      break;
    }
  }
}

void KytheFactsExtractor::VisitAutoConstructScope(const IndexingFactNode &node,
                                                  const VName &vname) {
  const auto tag = node.Value().GetIndexingFactType();

  // Must be copied (as Visit() can change the current scope).
  const Signature current_scope = scope_resolver_->CurrentScope();

  // Determines whether to create a scope for this node or not.
  switch (tag) {
    case IndexingFactType::kAnonymousScope:
    case IndexingFactType::kClass:
    case IndexingFactType::kConstructor:
    case IndexingFactType::kFile:
    case IndexingFactType::kFunctionOrTask:
    case IndexingFactType::kFunctionOrTaskForwardDeclaration:
    case IndexingFactType::kInterface:
    case IndexingFactType::kMacro:
    case IndexingFactType::kModule:
    case IndexingFactType::kPackage:
    case IndexingFactType::kProgram:
    case IndexingFactType::kParamDeclaration:
    case IndexingFactType::kStructOrUnion: {
      scope_resolver_->SetCurrentScope(vname.signature);
      VisitUsingVName(node, vname);
      break;
    }
    case IndexingFactType::kVariableDefinition: {
      if (!node.Children().empty()) {
        // Complex data type. Add it to the top of the signature.
        VisitUsingVName(node, vname);
      } else {
        Visit(node);
      }
      break;
    }
    default: {
      Visit(node);
    }
  }
  scope_resolver_->SetCurrentScope(current_scope);
}

void KytheFactsExtractor::VisitUsingVName(const IndexingFactNode &node,
                                          const VName &vname) {
  const VNameContext::AutoPop vnames_auto_pop(&vnames_context_, &vname);
  // Must be copied (as Visit() can change the current scope).
  const Signature current_scope = scope_resolver_->CurrentScope();
  Visit(node);
  scope_resolver_->SetCurrentScope(current_scope);
}

void KytheFactsExtractor::Visit(const IndexingFactNode &node) {
  for (const IndexingFactNode &child : node.Children()) {
    IndexingFactNodeTagResolver(child);
  }
}

VName KytheFactsExtractor::DeclareFile(const IndexingFactNode &file_fact_node) {
  VName file_vname = {.path = FilePath(),
                      .root = "",
                      .signature = Signature(""),
                      .corpus = Corpus(),
                      .language = kEmptyKytheLanguage};
  const auto &anchors(file_fact_node.Value().Anchors());
  CHECK_GE(anchors.size(), 2);
  const std::string_view code_text = file_fact_node.Value().Anchors()[1].Text();

  CreateFact(file_vname, kFactNodeKind, kNodeFile);
  CreateFact(file_vname, kFactText, code_text);

  // Update the signature of the file to be the global signature.
  // Used in scopes and makes signatures unique.
  file_vname.signature = Signature(FilePath());
  return file_vname;
}

VName KytheFactsExtractor::DeclareModule(
    const IndexingFactNode &module_fact_node) {
  const auto &anchors = module_fact_node.Value().Anchors();
  const Anchor &module_name = anchors[0];
  VName module_vname = {
      .path = FilePath(),
      .root = "",
      .signature = CreateScopeRelativeSignature(module_name.Text()),
      .corpus = Corpus()};
  const VName module_name_anchor = CreateAnchor(module_name);

  CreateFact(module_vname, kFactNodeKind, kNodeRecord);
  CreateFact(module_vname, kFactSubkind, kSubkindModule);
  CreateFact(module_vname, kFactComplete, kCompleteDefinition);
  CreateEdge(module_name_anchor, kEdgeDefinesBinding, module_vname);

  if (anchors.size() > 1) {
    const Anchor &module_end_label = anchors[1];
    const VName module_end_label_anchor = CreateAnchor(module_end_label);
    CreateEdge(module_end_label_anchor, kEdgeRef, module_vname);
  }

  return module_vname;
}

VName KytheFactsExtractor::DeclareProgram(
    const IndexingFactNode &program_fact_node) {
  const auto &anchors = program_fact_node.Value().Anchors();
  const Anchor &program_name = anchors[0];

  VName program_vname = {
      .path = FilePath(),
      .root = "",
      .signature = CreateScopeRelativeSignature(program_name.Text()),
      .corpus = Corpus()};
  const VName program_name_anchor = CreateAnchor(program_name);

  CreateFact(program_vname, kFactNodeKind, kNodeRecord);
  CreateFact(program_vname, kFactSubkind, kSubkindProgram);
  CreateEdge(program_name_anchor, kEdgeDefinesBinding, program_vname);

  if (anchors.size() > 1) {
    const Anchor &program_end_label = anchors[1];
    const VName program_end_label_anchor = CreateAnchor(program_end_label);
    CreateEdge(program_end_label_anchor, kEdgeRef, program_vname);
  }

  return program_vname;
}

VName KytheFactsExtractor::DeclareInterface(
    const IndexingFactNode &interface_fact_node) {
  const auto &anchors = interface_fact_node.Value().Anchors();
  const Anchor &interface_name = anchors[0];

  VName interface_vname = {
      .path = FilePath(),
      .root = "",
      .signature = CreateScopeRelativeSignature(interface_name.Text()),
      .corpus = Corpus()};
  const VName interface_name_anchor = CreateAnchor(interface_name);

  CreateFact(interface_vname, kFactNodeKind, kNodeInterface);
  CreateEdge(interface_name_anchor, kEdgeDefinesBinding, interface_vname);

  if (anchors.size() > 1) {
    const Anchor &interface_end_label = anchors[1];
    const VName interface_end_label_anchor = CreateAnchor(interface_end_label);
    CreateEdge(interface_end_label_anchor, kEdgeRef, interface_vname);
  }

  return interface_vname;
}

void KytheFactsExtractor::ReferenceDataType(
    const IndexingFactNode &data_type_reference) {
  const auto &anchors = data_type_reference.Value().Anchors();

  SignatureDigest focused_scope = scope_resolver_->CurrentScopeDigest();
  for (const auto &anchor : anchors) {
    const std::optional<ScopedVname> type =
        scope_resolver_->FindScopeAndDefinition(anchor.Text(), focused_scope);
    if (!type) {
      return;
    }
    CreateAnchorReference(anchor, type->vname);
    focused_scope = type->type_scope;
  }
}

VName KytheFactsExtractor::DeclareTypedef(
    const IndexingFactNode &type_declaration) {
  const auto &anchor = type_declaration.Value().Anchors()[0];
  VName type_vname = {.path = FilePath(),
                      .root = "",
                      .signature = CreateScopeRelativeSignature(anchor.Text()),
                      .corpus = Corpus()};
  const VName type_vname_anchor = CreateAnchor(anchor);

  CreateFact(type_vname, kFactNodeKind, kNodeTAlias);
  CreateEdge(type_vname_anchor, kEdgeDefinesBinding, type_vname);

  return type_vname;
}

void KytheFactsExtractor::ReferenceNamedParam(
    const IndexingFactNode &named_param_node) {
  // Get the anchors.
  const auto &param_name = named_param_node.Value().Anchors()[0];
  std::optional<ScopedVname> param_name_type = std::nullopt;
  const auto parent_type = GetParentTypeScope(named_param_node);
  if (parent_type) {
    param_name_type = scope_resolver_->FindScopeAndDefinition(param_name.Text(),
                                                              *parent_type);
  } else {
    param_name_type =
        scope_resolver_->FindScopeAndDefinition(param_name.Text());
  }

  if (!param_name_type) {
    // No definition. Skip.
    return;
  }

  // Create the facts for this parameter reference.
  const VName param_vname_anchor = CreateAnchor(param_name);
  CreateEdge(param_vname_anchor, kEdgeRef, param_name_type->vname);
}

void KytheFactsExtractor::ReferenceModuleNamedPort(
    const IndexingFactNode &named_port_node) {
  const auto &port_name = named_port_node.Value().Anchors()[0];

  std::optional<ScopedVname> port_name_type;
  const auto parent_type = GetParentTypeScope(named_port_node);
  if (parent_type) {
    port_name_type =
        scope_resolver_->FindScopeAndDefinition(port_name.Text(), *parent_type);
  }
  if (!port_name_type) {
    VLOG(2) << "Failed to find the port type";
    // No definition. Skip.
    return;
  }

  const VName port_vname_anchor = CreateAnchor(port_name);
  CreateEdge(port_vname_anchor, kEdgeRef, port_name_type->vname);

  // The case where '.z(z)' is shortened to '.z'
  if (is_leaf(named_port_node)) {
    // Search in the current scope, not the type's scope.
    const auto port_name_ref =
        scope_resolver_->FindScopeAndDefinition(port_name.Text());
    if (port_name_ref) {
      CreateEdge(port_vname_anchor, kEdgeRef, port_name_ref->vname);
    }
  }
}

VName KytheFactsExtractor::DeclareVariable(
    const IndexingFactNode &variable_definition_node) {
  const auto &anchors = variable_definition_node.Value().Anchors();
  CHECK(!anchors.empty());
  const Anchor &anchor = anchors[0];
  VName variable_vname = {
      .path = FilePath(),
      .root = "",
      .signature = CreateScopeRelativeSignature(anchor.Text()),
      .corpus = Corpus()};
  const VName variable_vname_anchor = CreateAnchor(anchor);

  CreateFact(variable_vname, kFactNodeKind, kNodeVariable);
  CreateFact(variable_vname, kFactComplete, kCompleteDefinition);
  CreateEdge(variable_vname_anchor, kEdgeDefinesBinding, variable_vname);

  return variable_vname;
}

void KytheFactsExtractor::ReferenceVariable(
    const IndexingFactNode &variable_reference_node) {
  const auto &anchors = variable_reference_node.Value().Anchors();

  SignatureDigest focused_scope = scope_resolver_->CurrentScopeDigest();
  for (const auto &anchor : anchors) {
    const auto type =
        scope_resolver_->FindScopeAndDefinition(anchor.Text(), focused_scope);
    if (!type) {
      return;
    }
    CreateAnchorReference(anchor, type->vname);
    focused_scope = type->type_scope;
  }
}

VName KytheFactsExtractor::DeclarePackage(
    const IndexingFactNode &package_declaration_node) {
  const auto &anchors = package_declaration_node.Value().Anchors();
  const Anchor &package_name = anchors[0];

  VName package_vname = {
      .path = FilePath(),
      .root = "",
      .signature = CreateScopeRelativeSignature(package_name.Text()),
      .corpus = Corpus()};
  const VName package_name_anchor = CreateAnchor(package_name);

  CreateFact(package_vname, kFactNodeKind, kNodePackage);
  CreateEdge(package_name_anchor, kEdgeDefinesBinding, package_vname);

  if (anchors.size() > 1) {
    const Anchor &package_end_label = anchors[1];
    const VName package_end_label_anchor = CreateAnchor(package_end_label);
    CreateEdge(package_end_label_anchor, kEdgeRef, package_vname);
  }

  return package_vname;
}

VName KytheFactsExtractor::DeclareMacroDefinition(
    const IndexingFactNode &macro_definition_node) {
  const Anchor &macro_name = macro_definition_node.Value().Anchors()[0];

  // The signature is relative to the global scope so no relative signature
  // created here.
  VName macro_vname = {.path = FilePath(),
                       .root = "",
                       .signature = Signature(macro_name.Text()),
                       .corpus = Corpus()};
  const VName module_name_anchor = CreateAnchor(macro_name);

  CreateFact(macro_vname, kFactNodeKind, kNodeMacro);
  CreateEdge(module_name_anchor, kEdgeDefinesBinding, macro_vname);

  return macro_vname;
}

void KytheFactsExtractor::ReferenceMacroCall(
    const IndexingFactNode &macro_call_node) {
  const Anchor &macro_name = macro_call_node.Value().Anchors()[0];
  const VName macro_vname_anchor = CreateAnchor(macro_name);

  // The signature is relative to the global scope so no relative signature
  // created here.
  const VName variable_definition_vname = {
      .path = FilePath(),
      .root = "",
      .signature = Signature(macro_name.Text()),
      .corpus = Corpus()};

  CreateEdge(macro_vname_anchor, kEdgeRefExpands, variable_definition_vname);
}

VName KytheFactsExtractor::DeclareFunctionOrTask(
    const IndexingFactNode &function_fact_node) {
  // TODO(hzeller): null check added. Underlying issue
  // needs more investigation; was encountered at
  // https://chipsalliance.github.io/sv-tests-results/?v=veribleextractor+ivtest+regress-vlg_pr1628300_iv
  if (function_fact_node.Value().Anchors().empty()) {
    LOG(ERROR) << FilePath() << ": encountered empty function name";
    return VName();
  }

  const auto &function_name = function_fact_node.Value().Anchors()[0];

  VName function_vname = {
      .path = FilePath(),
      .root = "",
      .signature = CreateScopeRelativeSignature(function_name.Text()),
      .corpus = Corpus()};

  const VName function_vname_anchor = CreateAnchor(function_name);

  CreateFact(function_vname, kFactNodeKind, kNodeFunction);
  CreateEdge(function_vname_anchor, kEdgeDefinesBinding, function_vname);

  auto tag = function_fact_node.Value().GetIndexingFactType();
  switch (tag) {
    case IndexingFactType::kFunctionOrTask: {
      CreateFact(function_vname, kFactComplete, kCompleteDefinition);
      break;
    }
    case IndexingFactType::kFunctionOrTaskForwardDeclaration: {
      CreateFact(function_vname, kFactComplete, kInComplete);
      break;
    }
    case IndexingFactType::kConstructor: {
      CreateFact(function_vname, kFactSubkind, kSubkindConstructor);
      break;
    }
    default: {
      break;
    }
  }

  // Check if there is a function with the same name in the current scope and if
  // exists output "overrides" edge.
  const auto function_type =
      scope_resolver_->FindScopeAndDefinition(function_name.Text());
  // TODO(minatoma): add a check to output this edge only if the parent is class
  // or interface.
  // TODO(minatoma): add a function like SyntaxTreeNode::MatchesTagAnyOf to
  // IndexingFactsTree.
  if (function_type && scope_resolver_->CurrentScopeDigest() ==
                           function_type->instantiation_scope) {
    const VName &overridden_function_vname = function_type->vname;
    CreateEdge(function_vname, kEdgeOverrides, overridden_function_vname);

    // Delete the overriden base class function from the current scope so that
    // any reference would reference the current function and not the function
    // in the base class.
    scope_resolver_->RemoveDefinitionFromCurrentScope(
        overridden_function_vname);
  }

  return function_vname;
}

void KytheFactsExtractor::ReferenceFunctionOrTaskCall(
    const IndexingFactNode &function_call_fact_node) {
  const auto &anchors = function_call_fact_node.Value().Anchors();

  std::optional<ScopedVname> last_type = std::nullopt;
  for (const auto &anchor : anchors) {
    const std::optional<ScopedVname> type =
        last_type ? scope_resolver_->FindScopeAndDefinition(
                        anchor.Text(), last_type->type_scope)
                  : scope_resolver_->FindScopeAndDefinition(anchor.Text());
    if (type) {
      CreateAnchorReference(anchor, type->vname);
      last_type = type;
    } else {
      // Failed to fully resolve the types.
      return;
    }
  }

  if (last_type) {
    // Create ref/call edge.
    CreateEdge(CreateAnchor(anchors.back()), kEdgeRefCall, last_type->vname);
  }
}

VName KytheFactsExtractor::DeclareClass(
    const IndexingFactNode &class_fact_node) {
  const auto &anchors = class_fact_node.Value().Anchors();
  const Anchor &class_name = anchors[0];

  VName class_vname = {
      .path = FilePath(),
      .root = "",
      .signature = CreateScopeRelativeSignature(class_name.Text()),
      .corpus = Corpus()};
  const VName class_name_anchor = CreateAnchor(class_name);

  CreateFact(class_vname, kFactNodeKind, kNodeRecord);
  CreateFact(class_vname, kFactComplete, kCompleteDefinition);
  CreateEdge(class_name_anchor, kEdgeDefinesBinding, class_vname);

  if (anchors.size() > 1) {
    const Anchor &class_end_label = anchors[1];
    const VName class_end_label_anchor = CreateAnchor(class_end_label);
    CreateEdge(class_end_label_anchor, kEdgeRef, class_vname);
  }

  return class_vname;
}

void KytheFactsExtractor::ReferenceExtendsInheritance(
    const IndexingFactNode &extends_node) {
  const auto &anchors = extends_node.Value().Anchors();

  std::optional<ScopedVname> last_type = std::nullopt;
  for (const auto &anchor : anchors) {
    const auto type = scope_resolver_->FindScopeAndDefinition(anchor.Text());
    if (type) {
      CreateAnchorReference(anchor, type->vname);
      last_type = type;
    }
  }

  if (last_type) {
    // Create kythe facts for extends.
    const VName &derived_class_vname = vnames_context_.top();
    CreateEdge(derived_class_vname, kEdgeExtends, last_type->vname);

    // Append the members of the parent class as members of the current class's
    // scope.
    scope_resolver_->AppendScopeToCurrentScope(last_type->type_scope);
  }
}

void KytheFactsExtractor::ReferencePackageImport(
    const IndexingFactNode &import_fact_node) {
  // TODO(minatoma): remove the imported vnames before exporting the scope as
  // imports aren't intended to be accessible from outside the enclosing parent.
  // Alternatively, maintain separate sets: exported, non-exported, or provide
  // an attribute to distinguish.
  const auto &anchors = import_fact_node.Value().Anchors();
  const Anchor &package_name_anchor = anchors[0];

  const std::optional<ScopedVname> package_name_anchor_type =
      scope_resolver_->FindScopeAndDefinition(package_name_anchor.Text());
  if (!package_name_anchor_type) {
    LOG(WARNING) << "Failed to find a definition of "
                 << package_name_anchor.Text() << " package.";
    return;
  }
  CreateEdge(CreateAnchor(package_name_anchor), kEdgeRefImports,
             package_name_anchor_type->vname);

  // case of import pkg::my_variable.
  if (anchors.size() > 1) {
    const Anchor &imported_item_name = anchors[1];
    const auto imported_item_name_type =
        scope_resolver_->FindScopeAndDefinition(
            imported_item_name.Text(), package_name_anchor_type->type_scope);
    if (imported_item_name_type) {
      CreateEdge(CreateAnchor(imported_item_name), kEdgeRef,
                 imported_item_name_type->vname);

      // Add the found definition to the current scope as if it was declared in
      // our scope so that it can be captured without "::".
      scope_resolver_->AddDefinitionToCurrentScope(
          imported_item_name_type->vname, package_name_anchor_type->type_scope);
    }
  } else {
    // case of import pkg::*.

    // Add all the definitions in that package to the current scope as if it was
    // declared in our scope so that it can be captured without "::".
    scope_resolver_->AppendScopeToCurrentScope(
        package_name_anchor_type->type_scope);
  }
}

void KytheFactsExtractor::ReferenceMember(
    const IndexingFactNode &member_reference_node) {
  // Resolve pkg::class::member case. `pkg` must be in scope, but `class` is in
  // `pkg`s scope, while `member` is in `class`es scope.
  const auto &anchors = member_reference_node.Value().Anchors();
  if (anchors.empty()) {
    return;
  }

  SignatureDigest focused_scope = scope_resolver_->CurrentScopeDigest();
  for (const auto &anchor : anchors) {
    const std::optional<ScopedVname> type =
        scope_resolver_->FindScopeAndDefinition(anchor.Text(), focused_scope);
    if (!type) {
      // No need to look further.
      return;
    }
    CreateAnchorReference(anchor, type->vname);

    focused_scope = type->type_scope;
  }
}

void KytheFactsExtractor::ReferenceIncludeFile(
    const IndexingFactNode &include_node) {
  const auto &anchors = include_node.Value().Anchors();
  CHECK_GE(anchors.size(), 2);
  const Anchor &file_name = anchors[0];
  const Anchor &file_path = anchors[1];

  const VName file_vname = {.path = file_path.Text(),
                            .root = "",
                            .signature = Signature(""),
                            .corpus = Corpus(),
                            .language = kEmptyKytheLanguage};
  const VName file_anchor = CreateAnchor(file_name);

  CreateEdge(file_anchor, kEdgeRefIncludes, file_vname);

  auto included_file_scope = scope_resolver_->FindScopeAndDefinition(
      file_path.Text(), ScopeResolver::GlobalScope());
  if (!included_file_scope) {
    LOG(INFO) << "File scope not found For file: " << file_path.Text();
    return;
  }

  // Create child of edge between the parent and the member of the included
  // file.
  const auto &included_file_content =
      scope_resolver_->ListScopeMembers(included_file_scope->type_scope);
  for (const auto &ref : included_file_content) {
    CreateEdge(ref, kEdgeChildOf, vnames_context_.top());
  }
}

VName KytheFactsExtractor::DeclareAnonymousScope(
    const IndexingFactNode &temp_scope) {
  const auto &scope_id = temp_scope.Value().Anchors()[0];
  VName vname = {.path = FilePath(),
                 .root = "",
                 .signature = CreateScopeRelativeSignature(scope_id.Text()),
                 .corpus = Corpus()};
  return vname;
}

VName KytheFactsExtractor::DeclareConstant(const IndexingFactNode &constant) {
  const auto &anchor = constant.Value().Anchors()[0];
  VName constant_vname = {
      .path = FilePath(),
      .root = "",
      .signature = CreateScopeRelativeSignature(anchor.Text()),
      .corpus = Corpus()};
  const VName variable_vname_anchor = CreateAnchor(anchor);

  CreateFact(constant_vname, kFactNodeKind, kNodeConstant);
  CreateEdge(variable_vname_anchor, kEdgeDefinesBinding, constant_vname);

  return constant_vname;
}

VName KytheFactsExtractor::DeclareStructOrUnion(
    const IndexingFactNode &struct_node) {
  const auto &anchors = struct_node.Value().Anchors();
  const Anchor &struct_name = anchors[0];

  VName struct_vname = {
      .path = FilePath(),
      .root = "",
      .signature = CreateScopeRelativeSignature(struct_name.Text()),
      .corpus = Corpus()};
  const VName struct_name_anchor = CreateAnchor(struct_name);

  CreateFact(struct_vname, kFactNodeKind, kNodeRecord);
  CreateEdge(struct_name_anchor, kEdgeDefinesBinding, struct_vname);

  return struct_vname;
}

void KytheFactsExtractor::CreateAnchorReference(const Anchor &anchor,
                                                const VName &definition) {
  CreateEdge(CreateAnchor(anchor), kEdgeRef, definition);
}

VName KytheFactsExtractor::CreateAnchor(const Anchor &anchor) {
  const auto &anchor_range = anchor.SourceTextRange();
  if (!anchor_range) {
    LOG(ERROR) << "Anchor not set! This is a bug. Skipping this Anchor. File: "
               << FilePath() << " Anchor text: " << anchor.Text();
    return VName();
  }
  const int start_location = anchor_range->begin;
  const int end_location = start_location + anchor_range->length;
  if (start_location == end_location) {
    LOG(ERROR)
        << "Zero-sized Anchor! This is a bug. Skipping this Anchor. File: "
        << FilePath() << " Anchor text: " << anchor.Text();
    return VName();
  }
  const auto [location_str, _] = signature_locations_.emplace(
      absl::StrCat("@", start_location, ":", end_location));
  VName anchor_vname = {.path = FilePath(),
                        .root = "",
                        .signature = Signature(*location_str),
                        .corpus = Corpus()};

  CreateFact(anchor_vname, kFactNodeKind, kNodeAnchor);
  // This is one of the only locations that passes a std::string&& to
  // CreateFact, everywhere else is string_view.
  CreateFact(anchor_vname, kFactAnchorStart, absl::StrCat(start_location));
  CreateFact(anchor_vname, kFactAnchorEnd, absl::StrCat(end_location));

  return anchor_vname;
}

Signature KytheFactsExtractor::CreateScopeRelativeSignature(
    std::string_view signature) const {
  // Append the given signature to the signature of the parent.
  return Signature(vnames_context_.top().signature, signature);
}

void KytheFactsExtractor::CreateFact(const VName &vname,
                                     std::string_view fact_name,
                                     std::string_view fact_value) {
  Fact fact(vname, fact_name, fact_value);
  auto hash = absl::HashOf(fact);
  if (!seen_kythe_hashes_.contains(hash)) {
    facts_output_->Emit(fact);
    seen_kythe_hashes_.insert(hash);
  }
}

void KytheFactsExtractor::CreateEdge(const VName &source_node,
                                     std::string_view edge_name,
                                     const VName &target_node) {
  Edge edge(source_node, edge_name, target_node);
  auto hash = absl::HashOf(edge);
  if (!seen_kythe_hashes_.contains(hash)) {
    facts_output_->Emit(edge);
    seen_kythe_hashes_.insert(hash);
  }
}

std::ostream &KytheFactsPrinter::PrintJsonStream(std::ostream &stream) const {
  // TODO(fangism): Print function should not be doing extraction work.
  class Printer final : public KytheOutput {
   public:
    explicit Printer(std::ostream &stream) : stream_(stream) {}
    void Emit(const Fact &fact) final {
      fact.FormatJSON(stream_, /*debug=*/false) << std::endl;
    }
    void Emit(const Edge &edge) final {
      edge.FormatJSON(stream_, /*debug=*/false) << std::endl;
    }

   private:
    std::ostream &stream_;
  } printer(stream);

  StreamKytheFactsEntries(&printer, file_list_facts_tree_, *project_);

  return stream;
}

std::ostream &KytheFactsPrinter::PrintJson(std::ostream &stream) const {
  // TODO(fangism): Print function should not be doing extraction work.
  class Printer final : public KytheOutput {
   public:
    explicit Printer(std::ostream &stream) : stream_(stream) {}
    void Emit(const Fact &fact) final {
      if (add_comma_) stream_ << "," << std::endl;
      fact.FormatJSON(stream_, /*debug=*/true) << std::endl;
      add_comma_ = true;
    }
    void Emit(const Edge &edge) final {
      if (add_comma_) stream_ << "," << std::endl;
      edge.FormatJSON(stream_, /*debug=*/true) << std::endl;
      add_comma_ = true;
    }

   private:
    std::ostream &stream_;
    bool add_comma_ = false;
  } printer(stream);

  stream << "[";
  StreamKytheFactsEntries(&printer, file_list_facts_tree_, *project_);
  stream << "]" << std::endl;

  return stream;
}

std::ostream &operator<<(std::ostream &stream,
                         const KytheFactsPrinter &kythe_facts_printer) {
  if (kythe_facts_printer.debug_) {
    kythe_facts_printer.PrintJson(stream);
  } else {
    kythe_facts_printer.PrintJsonStream(stream);
  }
  return stream;
}

}  // namespace kythe
}  // namespace verilog
