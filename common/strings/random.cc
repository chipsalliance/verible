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

#include "common/strings/random.h"

#include <string>
#include <string_view>

#include "common/util/interval_set.h"
#include "common/util/iterator_range.h"
#include "common/util/logging.h"

namespace verible {

char RandomAlphaChar() {
  static const IntervalSet<char> chars{{'a', 'z' + 1}, {'A', 'Z' + 1}};
  static const auto generator = chars.UniformRandomGenerator();
  return generator();
}

char RandomAlphaNumChar() {
  static const IntervalSet<char> chars{
      {'a', 'z' + 1}, {'A', 'Z' + 1}, {'0', '9' + 1}};
  static const auto generator = chars.UniformRandomGenerator();
  return generator();
}

std::string RandomEqualLengthIdentifier(std::string_view input) {
  CHECK(!input.empty());
  std::string s(input.length(), '?');
  s.front() = RandomAlphaChar();
  for (auto &ch : verible::make_range(s.begin() + 1, s.end())) {
    ch = RandomAlphaNumChar();
  }
  return s;
}

}  // namespace verible
