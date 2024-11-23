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

#include "verible/common/analysis/command-file-lexer.h"

#include <algorithm>
#include <vector>

#include "absl/strings/string_view.h"
#include "verible/common/lexer/token-stream-adapter.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/token-stream-view.h"
#include "verible/common/util/iterator-range.h"
#include "verible/common/util/logging.h"

namespace verible {

CommandFileLexer::CommandFileLexer(absl::string_view config)
    : parent_lexer_type(config) {
  const auto lex_status = MakeTokenSequence(
      this, config, &tokens_, [&](const TokenInfo &error_token) {
        LOG(ERROR) << "erroneous token: " << error_token;
      });

  // Pre-process all tokens where its needed
  for (auto &t : tokens_) {
    switch (t.token_enum()) {
      case ConfigToken::kFlag:
        // Skip -- prefix
        t.set_text(t.text().substr(2, t.text().length() - 2));
        break;
      case ConfigToken::kFlagWithArg:
        // Skip -- prefix and = suffix
        t.set_text(t.text().substr(2, t.text().length() - 3));
        break;
      default:
        break;
    }
  }
  Restart(config);
}

bool CommandFileLexer::TokenIsError(const verible::TokenInfo &token) const {
  return false;
}

std::vector<TokenRange> CommandFileLexer::GetCommandsTokenRanges() {
  std::vector<TokenRange> commands;

  auto i = tokens_.cbegin();
  auto j = i;

  for (;;) {
    // Note that empty lines or lines with whitespace only are skipped
    // by the lexer and do not have to be handled here
    j = std::find_if(i + 1, tokens_.cend(), [](TokenInfo t) {
      return t.token_enum() == ConfigToken::kNewline;
    });

    if (j == tokens_.cend()) {
      break;
    }

    // Use const iterators, increment j to include last element
    commands.push_back(make_range(i, ++j));

    i = j;
  }

  return commands;
}

}  // namespace verible
