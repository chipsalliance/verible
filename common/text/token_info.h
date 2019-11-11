// Copyright 2017-2019 The Verible Authors.
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

#ifndef VERIBLE_COMMON_TEXT_TOKEN_INFO_H_
#define VERIBLE_COMMON_TEXT_TOKEN_INFO_H_

#include <algorithm>   // for std::distance, std::copy
#include <functional>  // for std::function
#include <iosfwd>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "common/text/constants.h"
#include "common/util/iterator_range.h"

namespace verible {

// TokenInfo describes the text and location of a lexed token.
// TokenInfo is a unit returned by the FlexLexerAdapter.
// Reminder: The text string_view doesn't own its memory, so the owner must
// always out-live the token.
//
// TODO(fangism): This struct is becoming too functional, make it a class.
// Fix names for readability in that update.
struct TokenInfo {
  int token_enum;

  // The substring of a larger text that this token represents.
  absl::string_view text;

  // Construct an EOF token.
  // Note, however, that the bounds of the internal string_view in this case
  // do not correspond to any subrange of valid string.
  // If you need the string_view range to refer to (the end of) another string,
  // explicitly initialize using one of the constructors with string_view.
  static TokenInfo EOFToken();

  // Hide default constructor, force explicit initialization or call to
  // EOFToken().
  TokenInfo() = delete;

  TokenInfo(int token_enum, absl::string_view text)
      : token_enum(token_enum), text(text) {}

  // Call this with length 0 to initialize a null token that points to
  // the beginning of string buffer, along which the string_view can advance.
  // This is particularly useful in lexer initialization, where tokenization
  // advances substring ranges along a string buffer.
  TokenInfo(int token_enum, absl::string_view text, int length)
      : token_enum(token_enum), text(text.substr(0, length)) {}

  TokenInfo(const TokenInfo&) = default;
  TokenInfo(TokenInfo&&) = default;
  TokenInfo& operator=(const TokenInfo&) = default;

  // Context contains the information needed to display meaningful information
  // about a TokenInfo.
  struct Context {
    // Full range of text in which a token appears.
    // This is used to calculate byte offsets.
    absl::string_view base;

    // Prints a human-readable interpretation form of a token enumeration.
    std::function<void(std::ostream&, int)> token_enum_translator;

    explicit Context(absl::string_view b);

    Context(absl::string_view b,
            std::function<void(std::ostream&, int)> translator)
        : base(b), token_enum_translator(translator) {}
  };

  // Return position of this token's text start relative to a base buffer.
  int left(absl::string_view base) const {
    return std::distance(base.begin(), text.begin());
  }

  // Return position of this token's text end relative to a base buffer.
  int right(absl::string_view base) const {
    return std::distance(base.begin(), text.end());
  }

  // Advances the text range along the same memory buffer to span the
  // next token of size token_length.  Successive calls to this yield
  // a series of abutting substring ranges.  Useful for lexer operation.
  void AdvanceText(int token_length) {
    // The end of the previous token is the beginning of the next.
    text = absl::string_view(text.end(), token_length);
  }

  // Writes a human-readable string representation of the token.
  std::ostream& ToStream(std::ostream&, const Context& context) const;

  // Prints token representation without byte offsets.
  std::ostream& ToStream(std::ostream&) const;

  // Returns a human-readable string representation of the token.
  std::string ToString(const Context&) const;

  // Prints token representation without byte offsets.
  std::string ToString() const;

  // 'Moves' text string_view to point to another buffer, where the
  // contents still matches.  This is useful for analyzing different copies
  // of text, and transplanting to buffers that belong to different
  // memory owners.
  // This is a potentially dangerous operation, which can be validated
  // using a combination of object lifetime management and range-checking.
  // It is the caller's responsibility that it points to valid memory.
  void RebaseStringView(absl::string_view new_text);

  // This overload assumes that the string of interest from other has the
  // same length as the current string_view.
  // string_view::iterator happens to be const char*, but don't rely on that
  // fact as it can be implementation-dependent.
  void RebaseStringView(const char* new_text) {
    RebaseStringView(absl::string_view(new_text, text.length()));
  }

  // Joins the text from a sequence of (text-disjoint) tokens, and also
  // transforms the sequence of tokens in-place to point to corresponding
  // substrings of the newly concatenated string (out).  The updated tokens'
  // string_views will be abutting *subranges* of 'out', and their left/right
  // offsets will be updated to be relative to out->begin().
  // This is very useful for lexer test case construction.
  static void Concatenate(std::string* out, std::vector<TokenInfo>* tokens);

  // The default comparison operator requires that not only the contents
  // of the internal string_view be equal, but that they point to the
  // same buffer range.  See EquivalentWithoutLocation() for the variant that
  // doesn't require range equality.
  bool operator==(const TokenInfo& token) const;
  bool operator!=(const TokenInfo& token) const { return !(*this == token); }

  // Returns true if tokens are considered equivalent, ignoring location.
  bool EquivalentWithoutLocation(const TokenInfo& token) const {
    return token_enum == token.token_enum &&
           (token_enum == TK_EOF || text == token.text);
  }

  bool isEOF() const { return token_enum == TK_EOF; }
};

std::ostream& operator<<(std::ostream&, const TokenInfo&);

// Joins a range of TokenInfo-like objects to form a string whose contents
// match those of the elements's ranges, and also points the elements
// to the corresponding matching substrings of the new string (rebase).
// TokenInfo must be a write-able iterator.
// TokenInfo's element type must have the same interface as TokenInfo,
// e.g. a (public) subclass of TokenInfo.
template <class TokenIter>
void ConcatenateTokenInfos(std::string* out, TokenIter begin, TokenIter end) {
  // Inspired by absl::StrCat implementation details.

  // Calculate total string length, used to allocate one-time.
  const auto token_range = make_range(begin, end);
  size_t total_length = 0;
  for (const auto& token : token_range) {
    total_length += token.text.length();
  }
  out->resize(total_length);
  const absl::string_view out_view(*out);

  // Copy text into new buffer.
  auto code_iter = out->begin();  // writeable iterator (like char*)
  int offset = 0;
  for (auto& token : token_range) {
    // Expect library/compiler to optimize this to a strcpy()/memcpy().
    code_iter = std::copy(token.text.begin(), token.text.end(), code_iter);
    const auto new_text = out_view.substr(offset, token.text.length());
    // Adjust locations relative to newly concatenated string.
    token.RebaseStringView(new_text);
    offset += token.text.length();
  }
}

}  // namespace verible

#endif  // VERIBLE_COMMON_TEXT_TOKEN_INFO_H_
