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

#ifndef VERIBLE_COMMON_ANALYSIS_CITATION_H_
#define VERIBLE_COMMON_ANALYSIS_CITATION_H_

#include <string>
#include <string_view>

namespace verible {
// Given a styleguide topic, return a reference to some styleguide
// citation for the given topic (e.g. just the topic-name or an URL).
std::string GetStyleGuideCitation(std::string_view topic);
}  // namespace verible

#endif  // VERIBLE_COMMON_ANALYSIS_CITATION_H_
