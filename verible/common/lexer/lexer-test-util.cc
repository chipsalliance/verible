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

#include "verible/common/lexer/lexer-test-util.h"

#include <ostream>
#include <vector>

#include "absl/strings/string_view.h"
#include "verible/common/text/token-info.h"
#include "verible/common/util/logging.h"

namespace verible {

void FakeLexer::SetTokensData(const std::vector<TokenInfo> &tokens) {
  tokens_ = tokens;
  tokens_iter_ = tokens_.begin();
}

const TokenInfo &FakeLexer::DoNextToken() {
  CHECK(tokens_iter_ != tokens_.cend());
  return *tokens_iter_++;
}

std::ostream &operator<<(std::ostream &stream, const ShowCode &code) {
  return stream << " from code:\n" << code.text << "<<EOF>>";
}

}  // namespace verible
