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

// MacroDefinition is the structural representation of a macro definition.
// The structure is language-agnostic, but was developed with Verilog in mind.

#ifndef VERIBLE_COMMON_TEXT_MACRO_DEFINITION_H_
#define VERIBLE_COMMON_TEXT_MACRO_DEFINITION_H_

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "common/text/token_info.h"

namespace verible {

// For use in containers, we need a TokenInfo that is default constructible.
// Apart from that, it is no different than TokenInfo.
struct DefaultTokenInfo : public TokenInfo {
  DefaultTokenInfo() : TokenInfo(TokenInfo::EOFToken()) {}

  // Accept a plain TokenInfo for copy purposes.
  explicit DefaultTokenInfo(const TokenInfo& t) : TokenInfo(t) {}

  // Accept a plain TokenInfo for assignment purposes.
  DefaultTokenInfo& operator=(const TokenInfo& t) {
    TokenInfo::operator=(t);
    return *this;
  }
};

// Macro formal parameter specification: name with optional default.
struct MacroParameterInfo {
  explicit MacroParameterInfo(const TokenInfo& n = TokenInfo::EOFToken(),
                              const TokenInfo& d = TokenInfo::EOFToken())
      : name(n), default_value(d) {}

  // Name of macro parameter.
  TokenInfo name;

  // Macro parameters may have default values.  [Verilog]
  TokenInfo default_value;

  bool HasDefaultText() const { return !default_value.text().empty(); }
};

// MacroCall is a reference to a macro, such as `MACRO or `MACRO(...).
struct MacroCall {
  // Name of macro.
  DefaultTokenInfo macro_name;

  // Distinguish between a definition without () vs. with empty ().
  bool has_parameters;

  // Positional arguments to macro call.
  std::vector<DefaultTokenInfo> positional_arguments;

  MacroCall() : has_parameters(false) {}
};

class MacroDefinition {
 public:
  MacroDefinition(const TokenInfo& header, const TokenInfo& name)
      : header_(header), name_(name), is_callable_(false) {}

  absl::string_view Name() const { return name_.text(); }
  const TokenInfo& NameToken() const { return name_; }

  const TokenInfo& DefinitionText() const { return definition_text_; }

  void SetDefinitionText(const TokenInfo& t) { definition_text_ = t; }

  // Macro definitions with empty () should call this.
  void SetCallable() { is_callable_ = true; }

  bool IsCallable() const { return is_callable_; }

  // Add a formal parameter to macro definition.
  // Returns true if parameter was successfully added.
  // Duplicate parameter names are rejected, and will return false.
  // This automatically sets is_callable_.
  bool AppendParameter(const MacroParameterInfo&);

  const std::vector<MacroParameterInfo>& Parameters() const {
    return parameter_info_array_;
  }

  typedef std::map<absl::string_view, DefaultTokenInfo> substitution_map_type;

  // Create a text substitution map to be used for macro expansion.
  absl::Status PopulateSubstitutionMap(const std::vector<TokenInfo>&,
                                       substitution_map_type*) const;
  absl::Status PopulateSubstitutionMap(const std::vector<DefaultTokenInfo>&,
                                       substitution_map_type*) const;

  // Replace formal parameter references with actuals.
  static const TokenInfo& SubstituteText(const substitution_map_type&,
                                         const TokenInfo&,
                                         int actual_token_enum = 0);

 private:
  TokenInfo header_;  // e.g. "#define" or "`define"

  // Name of macro definition.  Is const because name_ determines
  // ordering within an ordered set, and thus, should be immutable.
  TokenInfo name_;

  // Distinguish between a definition without () vs. with empty ().
  bool is_callable_;

  // These form an ordered dictionary on macro parameters.
  std::vector<MacroParameterInfo> parameter_info_array_;
  std::map<std::string, size_t> parameter_positions_;

  // un-tokenized text
  DefaultTokenInfo definition_text_;
};

}  // namespace verible

#endif  // VERIBLE_COMMON_TEXT_MACRO_DEFINITION_H_
