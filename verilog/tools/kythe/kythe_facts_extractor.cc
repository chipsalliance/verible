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

#include "verilog/tools/kythe/kythe_facts_extractor.h"

#include <deque>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/node_hash_set.h"
#include "absl/memory/memory.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "common/strings/compare.h"
#include "common/util/logging.h"
#include "common/util/tree_operations.h"
#include "verilog/tools/kythe/kythe_schema_constants.h"
#include "verilog/tools/kythe/scope_resolver.h"
#include "verilog/tools/kythe/verilog_extractor_indexing_fact_type.h"

namespace verilog {
namespace kythe {

namespace {

// Returns the file path of the file from the given indexing facts tree node
// tagged with kFile.
absl::string_view GetFilePathFromRoot(const IndexingFactNode& root) {
  CHECK_EQ(root.Value().GetIndexingFactType(), IndexingFactType::kFile);
  return root.Value().Anchors()[0].Text();
}

// Create the global signature for the given file.
Signature CreateGlobalSignature(absl::string_view file_path) {
  return Signature(file_path);
}

// From the given list of anchors returns the list of Anchor values.
std::vector<absl::string_view> GetListOfReferencesfromListOfAnchor(
    const std::vector<Anchor>& anchors) {
  std::vector<absl::string_view> references;
  references.reserve(anchors.size());
  for (const auto& anchor : anchors) {
    references.push_back(anchor.Text());
  }
  return references;
}

// Returns the list of references from the given anchors list and appends the
// second Anchor to the end of the list.
std::vector<absl::string_view> ConcatenateReferences(
    const std::vector<Anchor>& anchors, const Anchor& anchor) {
  std::vector<absl::string_view> references(
      GetListOfReferencesfromListOfAnchor(anchors));
  references.push_back(anchor.Text());
  return references;
}

}  // namespace

// Extracted Kythe indexing facts and edges.
struct KytheIndexingData {
  // Extracted Kythe indexing facts.
  absl::flat_hash_set<Fact> facts;

  // Extracted Kythe edges.
  absl::flat_hash_set<Edge> edges;
};

// KytheFactsExtractor processes indexing facts for a single file.
// Responsible for traversing IndexingFactsTree and processing its different
// nodes to produce kythe indexing facts.
// Iteratively extracts facts and keeps running until no new facts are found in
// the last iteration.
class KytheFactsExtractor {
 public:
  KytheFactsExtractor(const VerilogSourceFile& source,
                      ScopeResolver* previous_files_scopes)
      : source_(&source), scope_resolver_(previous_files_scopes) {}

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
  class VNameContext : public verible::AutoPopStack<const VName*> {
   public:
    typedef verible::AutoPopStack<const VName*> base_type;

    // member class to handle push and pop of stack safely
    using AutoPop = base_type::AutoPop;

    // returns the top VName of the stack
    const VName& top() const { return *ABSL_DIE_IF_NULL(base_type::top()); }
  };

  // Returns the full path of the current source file.
  absl::string_view FilePath() { return source_->ResolvedPath(); }

  // Returns the corpus to which this file belongs;
  absl::string_view Corpus() const { return source_->Corpus(); }

  // Returns the string_view that spans the source file's entire text.
  absl::string_view SourceText() const;

 public:
  // Extracts kythe facts from the given IndexingFactsTree root.
  KytheIndexingData ExtractFile(const IndexingFactNode&);

 private:
  // Resolves the tag of the given node and directs the flow to the appropriate
  // function to extract kythe facts for that node.
  void IndexingFactNodeTagResolver(const IndexingFactNode&);

  // Determines whether to create a scope for this node or not and visits the
  // children.
  void VisitAutoConstructScope(const IndexingFactNode& node,
                               const VName& vname);

  // Add the given VName to vnames_context (to be used in scope relative
  // signatures) and visits the children of the given node creating a new scope
  // for the given node.
  void VisitUsingVName(const IndexingFactNode& node, const VName&, Scope&);

  // Directs the flow to the children of the given node.
  void Visit(const IndexingFactNode& node);

  // Determines whether or not to add the definition to the current scope.
  void AddDefinitionToCurrentScope(IndexingFactType, const VName&);

  // Appends the extracted children vnames to the scope of the current node.
  void ConstructScope(const IndexingFactNode&, const VName&, Scope&);

  // Determines whether or not to create a child of edge between the current
  // node and the previous node.
  void CreateChildOfEdge(IndexingFactType, const VName&);

  //=================================================================
  // Declare* methods create facts (some edges) and may introduce new scopes.
  // Reference* methods only create edges, and may not modify scopes' contents.

  // Extracts kythe facts from file node and returns it VName.
  VName DeclareFile(const IndexingFactNode&);

  // Extracts kythe facts for a reference to some user defined data type like
  // class or module.
  void ReferenceDataType(const IndexingFactNode&);

  // Extracts kythe facts for a constant like member in enums.
  VName DeclareConstant(const IndexingFactNode&);

  // Extracts kythe facts for structs or unions.
  VName DeclareStructOrUnion(const IndexingFactNode&);

  // Extracts kythe facts for a type declaration.
  VName DeclareTypedef(const IndexingFactNode&);

  // Extracts kythe facts from interface node and returns it VName.
  VName DeclareInterface(const IndexingFactNode& interface_fact_node);

  // Extracts kythe facts from program node and returns it VName.
  VName DeclareProgram(const IndexingFactNode& program_fact_node);

  // Extracts kythe facts from module named port node e.g("m(.in1(a))").
  void ReferenceModuleNamedPort(const IndexingFactNode&);

  // Extracts kythe facts from named param
  // e.g module_type #(.N(x)) extracts "N";
  void ReferenceNamedParam(const IndexingFactNode&);

  // Extracts kythe facts from module node and returns it VName.
  VName DeclareModule(const IndexingFactNode&);

  // Extracts kythe facts from class node and returns it VName.
  VName DeclareClass(const IndexingFactNode&);

  // Extracts kythe facts from class extends node.
  void ReferenceExtendsInheritance(const IndexingFactNode&);

  // Extracts kythe facts from module instance, class instance, variable
  // definition and param declaration nodes and returns its VName.
  VName DeclareVariable(const IndexingFactNode& node);

  // Extracts kythe facts from a module port reference node.
  void ReferenceVariable(const IndexingFactNode& node);

  // Creates a new anonymous scope for if conditions and loops.
  VName DeclareAnonymousScope(const IndexingFactNode& temp_scope);

  // Extracts kythe facts from a function or task node and returns its VName.
  VName DeclareFunctionOrTask(const IndexingFactNode& function_fact_node);

  // Extracts kythe facts from a function or task call node.
  void ReferenceFunctionOrTaskCall(
      const IndexingFactNode& function_call_fact_node);

  // Extracts kythe facts from a package declaration node and returns its VName.
  VName DeclarePackage(const IndexingFactNode& node);

  // Extracts kythe facts from package import node.
  void ReferencePackageImport(const IndexingFactNode& node);

  // Extracts kythe facts from a macro definition node and returns its VName.
  VName DeclareMacroDefinition(const IndexingFactNode& macro_definition_node);

  // Extracts kythe facts from a macro call node.
  void ReferenceMacroCall(const IndexingFactNode& macro_call_node);

  // Extracts kythe facts from a "`include" node.
  void ReferenceIncludeFile(const IndexingFactNode& include_node);

  // Extracts kythe facts from member reference statement.
  // e.g pkg::member or class::member or class.member
  // The names are treated as anchors e.g:
  // pkg::member => {Anchor(pkg), Anchor(member)}
  // pkg::class_name::var => {Anchor(pkg), Anchor(class_name), Anchor(var)}
  void ReferenceMember(const IndexingFactNode& member_reference_node);

  //============ end of Declare*, Reference* methods ===================

  // Create "ref" edges that point from the given anchors to the given
  // definitions in order.
  void CreateAnchorReferences(
      const std::vector<Anchor>& anchors,
      const std::vector<std::pair<const VName*, const Scope*>>& definitions);

  // Generates an anchor VName for kythe.
  VName CreateAnchor(const Anchor&);

  // Appends the signatures of previous containing scope vname to make
  // signatures unique relative to scopes.
  Signature CreateScopeRelativeSignature(absl::string_view) const;

  // Generates fact strings for Kythe facts.
  // Schema for this fact can be found here:
  // https://kythe.io/docs/schema/writing-an-indexer.html
  void CreateFact(const VName& vname, absl::string_view name,
                  absl::string_view value);

  // Generates edge strings for Kythe edges.
  // Schema for this edge can be found here:
  // https://kythe.io/docs/schema/writing-an-indexer.html
  void CreateEdge(const VName& source, absl::string_view name,
                  const VName& target);

  // The verilog source file from which facts are extracted.
  const VerilogSourceFile* const source_;

 private:  // data
  // Keeps track of VNames of ancestors as the visitor traverses the facts
  // tree.
  VNameContext vnames_context_;

  // Keeps track and saves the explored scopes with a <key, value> and maps
  // every signature to its scope.
  ScopeResolver* scope_resolver_;

  // Contains resulting kythe facts and edges to output.
  KytheIndexingData kythe_data_;

  // Location signature backing store.
  // Inside signatures, we record locations as string_views,
  // this is the backing store for assembled strings as
  // location markers such as "@123:456"
  // Needs to be a node_hash_set to provide value stability.
  absl::node_hash_set<std::string> signature_locations_;
};

void StreamKytheFactsEntries(KytheOutput* kythe_output,
                             const IndexingFactNode& file_list,
                             const VerilogProject& project) {
  VLOG(1) << __FUNCTION__;
  // Create a new ScopeResolver and give the ownership to the scope_resolvers
  // vector so that it can outlive KytheFactsExtractor.
  // The ScopeResolver-s are created and linked together as a linked-list
  // structure so that the current ScopeResolver can search for definitions in
  // the previous files' scopes.
  // TODO(fangism): re-implement root-level symbol lookup with a proper
  // project-wide symbol table, for efficient lookup.
  std::vector<std::unique_ptr<ScopeResolver>> scope_resolvers;
  scope_resolvers.push_back(std::unique_ptr<ScopeResolver>(nullptr));

  // TODO(fangism): infer dependency ordering automatically based on
  // the symbols defined in each file.

  // Create a reverse map from resolved path to referenced path.
  // All string_views reference memory owned inside 'project'.
  absl::btree_map<absl::string_view, absl::string_view,
                  verible::StringViewCompare>
      file_path_reverse_map;
  for (const auto& file_entry : project) {
    const VerilogSourceFile& source(*file_entry.second);
    if (source.Status().ok()) {
      file_path_reverse_map[source.ResolvedPath()] = file_entry.first;
    }
  }

  // Process each file in the original listed order.
  for (const IndexingFactNode& root : file_list.Children()) {
    // 'root' corresponds to the fact tree for a particular file.
    // 'file_path' is path-resolved.
    const absl::string_view file_path(GetFilePathFromRoot(root));
    VLOG(1) << "child file resolved path: " << file_path;
    scope_resolvers.push_back(absl::make_unique<ScopeResolver>(
        CreateGlobalSignature(file_path), scope_resolvers.back().get()));

    // Lookup registered file by its referenced path.
    const auto found = file_path_reverse_map.find(file_path);
    if (found == file_path_reverse_map.end()) continue;
    const absl::string_view referenced_path(found->second);
    VLOG(1) << "child file referenced path: " << referenced_path;
    const VerilogSourceFile* source =
        project.LookupRegisteredFile(referenced_path);
    if (source == nullptr) continue;

    // Create facts and edges.
    KytheFactsExtractor kythe_extractor(*source, scope_resolvers.back().get());

    // Collect facts and edges.
    const auto indexing_data = kythe_extractor.ExtractFile(root);
    for (const Fact& fact : indexing_data.facts) kythe_output->Emit(fact);
    for (const Edge& edge : indexing_data.edges) kythe_output->Emit(edge);
  }

  VLOG(1) << "end of " << __FUNCTION__;
}

absl::string_view KytheFactsExtractor::SourceText() const {
  return source_->GetTextStructure()->Contents();
}

KytheIndexingData KytheFactsExtractor::ExtractFile(
    const IndexingFactNode& root) {
  // root corresponds to the indexing tree for a single file.

  // Fixed-point analysis: Repeat fact extraction until no new facts are found.
  // This approach handles cases where symbols can be defined later in the file
  // than their uses, e.g. class member declarations and references.
  // TODO(fangism): do this in two phases: 1) collect definition points,
  //   2) collect references.
  // TODO(hzeller): Store less in the first phase, then start streaming out
  //   right away without first storing in set; no need to keep all the
  //   facts and edges in memory.
  std::size_t number_of_extracted_facts = 0;
  do {
    number_of_extracted_facts = kythe_data_.facts.size();
    IndexingFactNodeTagResolver(root);
  } while (number_of_extracted_facts != kythe_data_.facts.size());
  return std::move(kythe_data_);
}

void KytheFactsExtractor::IndexingFactNodeTagResolver(
    const IndexingFactNode& node) {
  const auto tag = node.Value().GetIndexingFactType();

  // Dispatch a node handler based on the node's tag.
  // This VName is used to keep track of the new generated VName and it will be
  // used in scopes, finding variable definitions and creating childof
  // relations.
  VName vname;
  switch (tag) {
      // The following cases extract definitions:
    case IndexingFactType::kFile: {
      vname = DeclareFile(node);
      break;
    }
    case IndexingFactType::kModule: {
      vname = DeclareModule(node);
      break;
    }
    case IndexingFactType::kInterface: {
      vname = DeclareInterface(node);
      break;
    }
    case IndexingFactType::kProgram: {
      vname = DeclareProgram(node);
      break;
    }
    case IndexingFactType::kParamDeclaration:
    case IndexingFactType::kModuleInstance:
    case IndexingFactType::kClassInstance:
    case IndexingFactType::kVariableDefinition: {
      vname = DeclareVariable(node);
      break;
    }
    case IndexingFactType::kConstant: {
      vname = DeclareConstant(node);
      break;
    }
    case IndexingFactType::kMacro: {
      vname = DeclareMacroDefinition(node);
      break;
    }
    case IndexingFactType::kClass: {
      vname = DeclareClass(node);
      break;
    }
    case IndexingFactType::kFunctionOrTask:
    case IndexingFactType::kFunctionOrTaskForwardDeclaration:
    case IndexingFactType::kConstructor: {
      vname = DeclareFunctionOrTask(node);
      break;
    }
    case IndexingFactType::kPackage: {
      vname = DeclarePackage(node);
      break;
    }
    case IndexingFactType::kStructOrUnion: {
      vname = DeclareStructOrUnion(node);
      break;
    }
    case IndexingFactType::kAnonymousScope: {
      vname = DeclareAnonymousScope(node);
      break;
    }
    case IndexingFactType::kTypeDeclaration: {
      vname = DeclareTypedef(node);
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

  AddDefinitionToCurrentScope(tag, vname);
  CreateChildOfEdge(tag, vname);
  VisitAutoConstructScope(node, vname);
}

void KytheFactsExtractor::AddDefinitionToCurrentScope(IndexingFactType tag,
                                                      const VName& vname) {
  switch (tag) {
    case IndexingFactType::kModule:
    case IndexingFactType::kModuleInstance:
    case IndexingFactType::kVariableDefinition:
    case IndexingFactType::kMacro:
    case IndexingFactType::kStructOrUnion:
    case IndexingFactType::kClass:
    case IndexingFactType::kClassInstance:
    case IndexingFactType::kFunctionOrTaskForwardDeclaration:
    case IndexingFactType::kConstructor:
    case IndexingFactType::kFunctionOrTask:
    case IndexingFactType::kParamDeclaration:
    case IndexingFactType::kPackage:
    case IndexingFactType::kConstant:
    case IndexingFactType::kTypeDeclaration:
    case IndexingFactType::kInterface:
    case IndexingFactType::kProgram: {
      scope_resolver_->AddDefinitionToCurrentScope(vname);
      break;
    }
    default: {
      break;
    }
  }
}

void KytheFactsExtractor::CreateChildOfEdge(IndexingFactType tag,
                                            const VName& vname) {
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

void KytheFactsExtractor::VisitAutoConstructScope(const IndexingFactNode& node,
                                                  const VName& vname) {
  Scope current_scope(vname.signature);
  const auto tag = node.Value().GetIndexingFactType();

  // Determines whether to create a scope for this node or not.
  switch (tag) {
    case IndexingFactType::kFile:
    case IndexingFactType::kParamDeclaration:
    case IndexingFactType::kModule:
    case IndexingFactType::kStructOrUnion:
    case IndexingFactType::kVariableDefinition:
    case IndexingFactType::kFunctionOrTask:
    case IndexingFactType::kFunctionOrTaskForwardDeclaration:
    case IndexingFactType::kConstructor:
    case IndexingFactType::kClass:
    case IndexingFactType::kMacro:
    case IndexingFactType::kPackage:
    case IndexingFactType::kInterface:
    case IndexingFactType::kProgram: {
      // Get the old scope of this node (if it was extracted in a previous
      // iteration).
      const Scope* old_scope = scope_resolver_->SearchForScope(vname.signature);
      if (old_scope != nullptr) {
        current_scope.AppendScope(*old_scope);
      }

      VisitUsingVName(node, vname, current_scope);
      break;
    }
    case IndexingFactType::kAnonymousScope: {
      VisitUsingVName(node, vname, current_scope);
      break;
    }
    default: {
      Visit(node);
    }
  }

  ConstructScope(node, vname, current_scope);
}

void KytheFactsExtractor::ConstructScope(const IndexingFactNode& node,
                                         const VName& vname,
                                         Scope& current_scope) {
  const auto tag = node.Value().GetIndexingFactType();

  // Determines whether to add the current scope to the scope context or not.
  switch (tag) {
    case IndexingFactType::kFile:
    case IndexingFactType::kModule:
    case IndexingFactType::kStructOrUnion:
    case IndexingFactType::kClass:
    case IndexingFactType::kFunctionOrTask:
    case IndexingFactType::kMacro:
    case IndexingFactType::kPackage:
    case IndexingFactType::kFunctionOrTaskForwardDeclaration:
    case IndexingFactType::kConstructor:
    case IndexingFactType::kInterface:
    case IndexingFactType::kProgram: {
      scope_resolver_->MapSignatureToScope(vname.signature, current_scope);
      break;
    }
    case IndexingFactType::kVariableDefinition: {
      // Break if this variable has no type.
      if (node.Parent() == nullptr ||
          node.Parent()->Value().GetIndexingFactType() !=
              IndexingFactType::kDataTypeReference) {
        scope_resolver_->MapSignatureToScope(vname.signature, current_scope);
        break;
      }

      // TODO(minatoma): refactor this and the below case into function.
      // TODO(minatoma): move this case to below and make variable definitions
      // scope-less.
      // TODO(minatoma): use kAnonymousType and kAnonymousTypeReference to get
      // rid of this case (if possible).
      // TODO(minatoma): consider getting rid of kModuleInstance and
      // kClassInstance and use kVariableDefinition if they don't provide
      // anything new.
      const auto& parent_anchors = node.Parent()->Value().Anchors();
      const std::vector<std::pair<const VName*, const Scope*>> definitions =
          scope_resolver_->SearchForDefinitions(
              GetListOfReferencesfromListOfAnchor(parent_anchors));

      if (!definitions.empty() && definitions.size() == parent_anchors.size() &&
          definitions.back().second != nullptr) {
        current_scope.AppendScope(*definitions.back().second);
      }

      scope_resolver_->MapSignatureToScope(vname.signature, current_scope);

      break;
    }
    case IndexingFactType::kModuleInstance:
    case IndexingFactType::kClassInstance: {
      if (node.Parent() == nullptr ||
          node.Parent()->Value().GetIndexingFactType() !=
              IndexingFactType::kDataTypeReference) {
        break;
      }

      // Find the scope of the parent data type and append the members of it to
      // the scope of the current instance.
      const auto& parent_anchors = node.Parent()->Value().Anchors();
      const std::vector<std::pair<const VName*, const Scope*>> definitions =
          scope_resolver_->SearchForDefinitions(
              GetListOfReferencesfromListOfAnchor(parent_anchors));

      if (definitions.empty() || definitions.size() != parent_anchors.size() ||
          definitions.back().second == nullptr) {
        break;
      }

      scope_resolver_->MapSignatureToScope(vname.signature,
                                           *definitions.back().second);
      break;
    }
    default: {
      break;
    }
  }
}

void KytheFactsExtractor::VisitUsingVName(const IndexingFactNode& node,
                                          const VName& vname,
                                          Scope& current_scope) {
  const VNameContext::AutoPop vnames_auto_pop(&vnames_context_, &vname);
  const ScopeContext::AutoPop scope_auto_pop(
      &scope_resolver_->GetMutableScopeContext(), &current_scope);
  Visit(node);
}

void KytheFactsExtractor::Visit(const IndexingFactNode& node) {
  for (const IndexingFactNode& child : node.Children()) {
    IndexingFactNodeTagResolver(child);
  }
}

VName KytheFactsExtractor::DeclareFile(const IndexingFactNode& file_fact_node) {
  VName file_vname = {.path = FilePath(),
                      .root = "",
                      .signature = Signature(""),
                      .corpus = Corpus(),
                      .language = kEmptyKytheLanguage};
  const auto& anchors(file_fact_node.Value().Anchors());
  CHECK_GE(anchors.size(), 2);
  const absl::string_view code_text =
      file_fact_node.Value().Anchors()[1].Text();

  CreateFact(file_vname, kFactNodeKind, kNodeFile);
  CreateFact(file_vname, kFactText, code_text);

  // Update the signature of the file to be the global signature.
  // Used in scopes and makes signatures unique.
  file_vname.signature = CreateGlobalSignature(FilePath());
  return file_vname;
}

VName KytheFactsExtractor::DeclareModule(
    const IndexingFactNode& module_fact_node) {
  const auto& anchors = module_fact_node.Value().Anchors();
  const Anchor& module_name = anchors[0];
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
    const Anchor& module_end_label = anchors[1];
    const VName module_end_label_anchor = CreateAnchor(module_end_label);
    CreateEdge(module_end_label_anchor, kEdgeRef, module_vname);
  }

  return module_vname;
}

VName KytheFactsExtractor::DeclareProgram(
    const IndexingFactNode& program_fact_node) {
  const auto& anchors = program_fact_node.Value().Anchors();
  const Anchor& program_name = anchors[0];

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
    const Anchor& program_end_label = anchors[1];
    const VName program_end_label_anchor = CreateAnchor(program_end_label);
    CreateEdge(program_end_label_anchor, kEdgeRef, program_vname);
  }

  return program_vname;
}

VName KytheFactsExtractor::DeclareInterface(
    const IndexingFactNode& interface_fact_node) {
  const auto& anchors = interface_fact_node.Value().Anchors();
  const Anchor& interface_name = anchors[0];

  VName interface_vname = {
      .path = FilePath(),
      .root = "",
      .signature = CreateScopeRelativeSignature(interface_name.Text()),
      .corpus = Corpus()};
  const VName interface_name_anchor = CreateAnchor(interface_name);

  CreateFact(interface_vname, kFactNodeKind, kNodeInterface);
  CreateEdge(interface_name_anchor, kEdgeDefinesBinding, interface_vname);

  if (anchors.size() > 1) {
    const Anchor& interface_end_label = anchors[1];
    const VName interface_end_label_anchor = CreateAnchor(interface_end_label);
    CreateEdge(interface_end_label_anchor, kEdgeRef, interface_vname);
  }

  return interface_vname;
}

void KytheFactsExtractor::ReferenceDataType(
    const IndexingFactNode& data_type_reference) {
  const auto& anchors = data_type_reference.Value().Anchors();

  const std::vector<std::pair<const VName*, const Scope*>> type_vnames =
      scope_resolver_->SearchForDefinitions(
          GetListOfReferencesfromListOfAnchor(anchors));

  CreateAnchorReferences(anchors, type_vnames);
}

VName KytheFactsExtractor::DeclareTypedef(
    const IndexingFactNode& type_declaration) {
  const auto& anchor = type_declaration.Value().Anchors()[0];
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
    const IndexingFactNode& named_param_node) {
  // Get the anchors.
  const auto& param_name = named_param_node.Value().Anchors()[0];

  // Search for the module or class that contains this parameter.
  // Parent Node must be kDataTypeReference or kMemberReference or kExtends.
  const std::vector<Anchor>& parent_data_type =
      named_param_node.Parent()->Value().Anchors();

  // Search inside the found module or class for the referenced parameter.
  const std::vector<std::pair<const VName*, const Scope*>> param_vnames =
      scope_resolver_->SearchForDefinitions(
          ConcatenateReferences(parent_data_type, param_name));

  // Check if all the references are found.
  if (param_vnames.size() != parent_data_type.size() + 1) {
    return;
  }

  // Create the facts for this parameter reference.
  const VName param_vname_anchor = CreateAnchor(param_name);
  CreateEdge(param_vname_anchor, kEdgeRef, *param_vnames.back().first);
}

void KytheFactsExtractor::ReferenceModuleNamedPort(
    const IndexingFactNode& named_port_node) {
  const auto& port_name = named_port_node.Value().Anchors()[0];

  // Parent Node must be kModuleInstance and the grand parent node must be
  // kDataTypeReference.
  const std::vector<Anchor>& module_type =
      named_port_node.Parent()->Parent()->Value().Anchors();

  const std::vector<std::pair<const VName*, const Scope*>> actual_port_vnames =
      scope_resolver_->SearchForDefinitions(
          ConcatenateReferences(module_type, port_name));

  // Check if all the references are found.
  if (actual_port_vnames.size() != module_type.size() + 1) {
    return;
  }

  const VName port_vname_anchor = CreateAnchor(port_name);
  CreateEdge(port_vname_anchor, kEdgeRef, *actual_port_vnames.back().first);

  if (is_leaf(named_port_node)) {
    const std::vector<std::pair<const VName*, const Scope*>> definition_vnames =
        scope_resolver_->SearchForDefinitions({port_name.Text()});

    if (!definition_vnames.empty()) {
      CreateEdge(port_vname_anchor, kEdgeRef, *definition_vnames[0].first);
    }
  }
}

VName KytheFactsExtractor::DeclareVariable(
    const IndexingFactNode& variable_definition_node) {
  const auto& anchors = variable_definition_node.Value().Anchors();
  CHECK(!anchors.empty());
  const Anchor& anchor = anchors[0];
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
    const IndexingFactNode& variable_reference_node) {
  const auto& anchors = variable_reference_node.Value().Anchors();

  const std::vector<std::pair<const VName*, const Scope*>>
      variable_definition_vnames = scope_resolver_->SearchForDefinitions(
          GetListOfReferencesfromListOfAnchor(anchors));

  CreateAnchorReferences(anchors, variable_definition_vnames);
}

VName KytheFactsExtractor::DeclarePackage(
    const IndexingFactNode& package_declaration_node) {
  const auto& anchors = package_declaration_node.Value().Anchors();
  const Anchor& package_name = anchors[0];

  VName package_vname = {
      .path = FilePath(),
      .root = "",
      .signature = CreateScopeRelativeSignature(package_name.Text()),
      .corpus = Corpus()};
  const VName package_name_anchor = CreateAnchor(package_name);

  CreateFact(package_vname, kFactNodeKind, kNodePackage);
  CreateEdge(package_name_anchor, kEdgeDefinesBinding, package_vname);

  if (anchors.size() > 1) {
    const Anchor& package_end_label = anchors[1];
    const VName package_end_label_anchor = CreateAnchor(package_end_label);
    CreateEdge(package_end_label_anchor, kEdgeRef, package_vname);
  }

  return package_vname;
}

VName KytheFactsExtractor::DeclareMacroDefinition(
    const IndexingFactNode& macro_definition_node) {
  const Anchor& macro_name = macro_definition_node.Value().Anchors()[0];

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
    const IndexingFactNode& macro_call_node) {
  const Anchor& macro_name = macro_call_node.Value().Anchors()[0];
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
    const IndexingFactNode& function_fact_node) {
  // TODO(hzeller): null check added. Underlying issue
  // needs more investigation; was encountered at
  // https://chipsalliance.github.io/sv-tests-results/?v=veribleextractor+ivtest+regress-vlg_pr1628300_iv
  if (function_fact_node.Value().Anchors().empty()) {
    LOG(ERROR) << FilePath() << ": encountered empty function name";
    return VName();
  }

  const auto& function_name = function_fact_node.Value().Anchors()[0];

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
  const VName* overridden_function_vname =
      scope_resolver_->SearchForDefinitionInCurrentScope(function_name.Text());

  // TODO(minatoma): add a check to output this edge only if the parent is class
  // or interface.
  // TODO(minatoma): add a function like SyntaxTreeNode::MatchesTagAnyOf to
  // IndexingFactsTree.
  if (overridden_function_vname != nullptr) {
    CreateEdge(function_vname, kEdgeOverrides, *overridden_function_vname);

    // Delete the overriden base class function from the current scope so that
    // any reference would reference the current function and not the function
    // in the base class.
    scope_resolver_->RemoveDefinitionFromCurrentScope(
        *overridden_function_vname);
  }

  return function_vname;
}

void KytheFactsExtractor::ReferenceFunctionOrTaskCall(
    const IndexingFactNode& function_call_fact_node) {
  const auto& anchors = function_call_fact_node.Value().Anchors();

  // Search for member hierarchy in the scopes.
  const std::vector<std::pair<const VName*, const Scope*>> definitions =
      scope_resolver_->SearchForDefinitions(
          GetListOfReferencesfromListOfAnchor(anchors));

  CreateAnchorReferences(anchors, definitions);

  // creating ref/call edge.
  // If the sizes aren't equal that means we couldn't find the function
  // defintion.
  if (!definitions.empty() && definitions.size() == anchors.size()) {
    const VName current_anchor_vname = CreateAnchor(anchors.back());
    CreateEdge(current_anchor_vname, kEdgeRefCall, *definitions.back().first);
  }
}

VName KytheFactsExtractor::DeclareClass(
    const IndexingFactNode& class_fact_node) {
  const auto& anchors = class_fact_node.Value().Anchors();
  const Anchor& class_name = anchors[0];

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
    const Anchor& class_end_label = anchors[1];
    const VName class_end_label_anchor = CreateAnchor(class_end_label);
    CreateEdge(class_end_label_anchor, kEdgeRef, class_vname);
  }

  return class_vname;
}

void KytheFactsExtractor::ReferenceExtendsInheritance(
    const IndexingFactNode& extends_node) {
  const auto& anchors = extends_node.Value().Anchors();

  // Search for member hierarchy in the scopes.
  const std::vector<std::pair<const VName*, const Scope*>> definitions =
      scope_resolver_->SearchForDefinitions(
          GetListOfReferencesfromListOfAnchor(anchors));

  CreateAnchorReferences(anchors, definitions);

  // Check if all the definitions were found.
  if (definitions.size() != anchors.size() || definitions.empty()) {
    return;
  }

  // TODO(hzeller): should this have been detected before ?
  // NULL vname is not encountered, but NULL scope. Issue #1128
  if (!definitions.back().first) {
    LOG(ERROR) << "ReferenceExtendsInheritance: NULL vname";
    return;
  }
  if (!definitions.back().second) {
    LOG(ERROR) << "ReferenceExtendsInheritance: NULL scope for vname "
               << *definitions.back().first;
    return;
  }

  // Create kythe facts for extends.
  const VName& derived_class_vname = vnames_context_.top();
  CreateEdge(derived_class_vname, kEdgeExtends, *definitions.back().first);

  // Append the members of the parent class as members of the current class's
  // scope.
  scope_resolver_->AppendScopeToCurrentScope(*definitions.back().second);
}

void KytheFactsExtractor::ReferencePackageImport(
    const IndexingFactNode& import_fact_node) {
  // TODO(minatoma): remove the imported vnames before exporting the scope as
  // imports aren't intended to be accessible from outside the enclosing parent.
  // Alternatively, maintain separate sets: exported, non-exported, or provide
  // an attribute to distinguish.
  const auto& anchors = import_fact_node.Value().Anchors();
  const Anchor& package_name_anchor = anchors[0];

  // case of import pkg::my_variable.
  if (anchors.size() > 1) {
    const Anchor& imported_item_name = anchors[1];

    // Search for member hierarchy in the scopes.
    const std::vector<std::pair<const VName*, const Scope*>> definition_vnames =
        scope_resolver_->SearchForDefinitions(
            {package_name_anchor.Text(), imported_item_name.Text()});

    // Loop over the found definitions and create kythe facts.
    for (size_t i = 0; i < definition_vnames.size(); i++) {
      const VName current_anchor = CreateAnchor(anchors[i]);
      if (i == 0) {
        CreateEdge(current_anchor, kEdgeRefImports,
                   *definition_vnames[i].first);
      } else {
        CreateEdge(current_anchor, kEdgeRef, *definition_vnames[i].first);
      }
    }

    if (definition_vnames.size() != 2) {
      return;
    }

    // Add the found definition to the current scope as if it was declared in
    // our scope so that it can be captured without "::".
    scope_resolver_->AddDefinitionToCurrentScope(*definition_vnames[1].first);
  } else {
    // case of import pkg::*.
    // Add all the definitions in that package to the current scope as if it was
    // declared in our scope so that it can be captured without "::".

    // Search for member hierarchy in the scopes.
    const std::vector<std::pair<const VName*, const Scope*>> definition_vnames =
        scope_resolver_->SearchForDefinitions({package_name_anchor.Text()});
    if (definition_vnames.empty()) {
      return;
    }

    const VName current_anchor = CreateAnchor(package_name_anchor);
    CreateEdge(current_anchor, kEdgeRefImports, *definition_vnames[0].first);

    // TODO(hzeller): null check added. Underlying issue of nullptr
    // scope needs more investigation; was encountered at
    // https://chipsalliance.github.io/sv-tests-results/?v=veribleextractor+hdlconv_std2017+hdlconvertor_std2017_p600
    if (const VName* vname = definition_vnames[0].first; vname) {
      scope_resolver_->AddDefinitionToCurrentScope(*vname);
    } else {
      LOG(ERROR) << FilePath() << ": ReferencePackageImport: NULL vname";
    }

    if (const Scope* scope = definition_vnames[0].second; scope) {
      scope_resolver_->AppendScopeToCurrentScope(*scope);
    } else {
      LOG(ERROR) << FilePath() << ": ReferencePackageImport: NULL scope";
    }
  }
}

void KytheFactsExtractor::ReferenceMember(
    const IndexingFactNode& member_reference_node) {
  // TODO(fangism): [algorithm] For member references like "A::B::C::D",
  // we currently construct member reference chains "A", "A,B", "A,B,C"...
  // which is O(N^2), so "A" is being looked-up repeatedly, the result of
  // previous lookups is not being re-used.  Re-structure and fix this.

  const auto& anchors = member_reference_node.Value().Anchors();

  // Search for member hierarchy in the scopes.
  const std::vector<std::pair<const VName*, const Scope*>> definitions =
      scope_resolver_->SearchForDefinitions(
          GetListOfReferencesfromListOfAnchor(anchors));

  CreateAnchorReferences(anchors, definitions);
}

void KytheFactsExtractor::ReferenceIncludeFile(
    const IndexingFactNode& include_node) {
  const auto& anchors = include_node.Value().Anchors();
  CHECK_GE(anchors.size(), 2);
  const Anchor& file_name = anchors[0];
  const Anchor& file_path = anchors[1];

  const VName file_vname = {.path = file_path.Text(),
                            .root = "",
                            .signature = Signature(""),
                            .corpus = Corpus(),
                            .language = kEmptyKytheLanguage};
  const VName file_anchor = CreateAnchor(file_name);

  CreateEdge(file_anchor, kEdgeRefIncludes, file_vname);

  const Scope* included_file_scope =
      scope_resolver_->SearchForScope(Signature(file_path.Text()));
  if (included_file_scope == nullptr) {
    LOG(INFO) << "File scope not found For file: " << file_path.Text();
    return;
  }

  // Create child of edge between the parent and the member of the included
  // file.
  for (const auto& [_, member] : included_file_scope->Members()) {
    CreateEdge(member, kEdgeChildOf, vnames_context_.top());
  }

  // Append the scope of the included file to the current scope.
  scope_resolver_->AppendScopeToCurrentScope(*included_file_scope);
}

VName KytheFactsExtractor::DeclareAnonymousScope(
    const IndexingFactNode& temp_scope) {
  const auto& scope_id = temp_scope.Value().Anchors()[0];
  VName vname = {.path = FilePath(),
                 .root = "",
                 .signature = CreateScopeRelativeSignature(scope_id.Text()),
                 .corpus = Corpus()};
  return vname;
}

VName KytheFactsExtractor::DeclareConstant(const IndexingFactNode& constant) {
  const auto& anchor = constant.Value().Anchors()[0];
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
    const IndexingFactNode& struct_node) {
  const auto& anchors = struct_node.Value().Anchors();
  const Anchor& struct_name = anchors[0];

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

void KytheFactsExtractor::CreateAnchorReferences(
    const std::vector<Anchor>& anchors,
    const std::vector<std::pair<const VName*, const Scope*>>& definitions) {
  // Loop over the definitions and create kythe facts.
  for (size_t i = 0; i < definitions.size(); i++) {
    const VName current_anchor_vname = CreateAnchor(anchors[i]);
    CreateEdge(current_anchor_vname, kEdgeRef, *definitions[i].first);
  }
}

VName KytheFactsExtractor::CreateAnchor(const Anchor& anchor) {
  const absl::string_view source_text = SourceText();
  const int start_location =
      std::distance(source_text.begin(), anchor.Text().begin());
  const int end_location = start_location + anchor.Text().length();
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
    absl::string_view signature) const {
  // Append the given signature to the signature of the parent.
  return Signature(vnames_context_.top().signature, signature);
}

void KytheFactsExtractor::CreateFact(const VName& vname,
                                     absl::string_view fact_name,
                                     absl::string_view fact_value) {
  kythe_data_.facts.emplace(vname, fact_name, fact_value);
}

void KytheFactsExtractor::CreateEdge(const VName& source_node,
                                     absl::string_view edge_name,
                                     const VName& target_node) {
  kythe_data_.edges.emplace(source_node, edge_name, target_node);
}

std::ostream& KytheFactsPrinter::PrintJsonStream(std::ostream& stream) const {
  // TODO(fangism): Print function should not be doing extraction work.
  class Printer final : public KytheOutput {
   public:
    explicit Printer(std::ostream& stream) : stream_(stream) {}
    void Emit(const Fact& fact) final {
      fact.FormatJSON(stream_, /*debug=*/false) << std::endl;
    }
    void Emit(const Edge& edge) final {
      edge.FormatJSON(stream_, /*debug=*/false) << std::endl;
    }

   private:
    std::ostream& stream_;
  } printer(stream);

  StreamKytheFactsEntries(&printer, file_list_facts_tree_, *project_);

  return stream;
}

std::ostream& KytheFactsPrinter::PrintJson(std::ostream& stream) const {
  // TODO(fangism): Print function should not be doing extraction work.
  class Printer final : public KytheOutput {
   public:
    explicit Printer(std::ostream& stream) : stream_(stream) {}
    void Emit(const Fact& fact) final {
      if (add_comma_) stream_ << "," << std::endl;
      fact.FormatJSON(stream_, /*debug=*/true) << std::endl;
      add_comma_ = true;
    }
    void Emit(const Edge& edge) final {
      if (add_comma_) stream_ << "," << std::endl;
      edge.FormatJSON(stream_, /*debug=*/true) << std::endl;
      add_comma_ = true;
    }

   private:
    std::ostream& stream_;
    bool add_comma_ = false;
  } printer(stream);

  stream << "[";
  StreamKytheFactsEntries(&printer, file_list_facts_tree_, *project_);
  stream << "]" << std::endl;

  return stream;
}

std::ostream& operator<<(std::ostream& stream,
                         const KytheFactsPrinter& kythe_facts_printer) {
  if (kythe_facts_printer.debug_) {
    kythe_facts_printer.PrintJson(stream);
  } else {
    kythe_facts_printer.PrintJsonStream(stream);
  }
  return stream;
}

}  // namespace kythe
}  // namespace verilog
