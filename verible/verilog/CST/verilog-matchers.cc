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

// Nothing in here: Just a c++ file place-holder used in the cc_library() so
// that bazel knows that the header-implementation in that libray is C++.
// Otherwise the compilation DB will emit it as C and clang-tidy/clangd would
// have trouble with the header (e.g. complains that <array> is not found
// as it would read it as C file).

// TL;DR
// Don't remove. Work around bazel's assumptions of C++-ness.
