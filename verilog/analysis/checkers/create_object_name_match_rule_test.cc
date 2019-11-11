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

#include "verilog/analysis/checkers/create_object_name_match_rule.h"

#include <initializer_list>

#include "gtest/gtest.h"
#include "common/analysis/linter_test_utils.h"
#include "common/analysis/syntax_tree_linter_test_utils.h"
#include "common/text/symbol.h"
#include "verilog/CST/verilog_treebuilder_utils.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunLintTestCases;

TEST(CreateObjectNameMatchTest, FunctionFailures) {
  // {} implies that no lint errors are expected.
  // violation is the expected finding tag in this set of tests.
  constexpr int kToken = TK_StringLiteral;
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"class c; function f; endfunction endclass"},
      {"class c; function f; foo_h = foo::type_id::create(\"foo_h\"); "
       "endfunction endclass"},
      {"class c; function f; foo_h = foo::type_id::create(); "
       "endfunction endclass"},
      {"class c; function f; foo_h = foo::type_id::create(",
       {kToken, "\"foo\""},
       "); endfunction endclass"},  // most common type of mismatch
      {"class c; function f; foo = foo::type_id::create(",
       {kToken, "\"foo_h\""},
       "); endfunction endclass"},
      {"class c; function f; asdf = foo::type_id::create(",
       {kToken, "\"qwerty\""},
       ", \"z\"); endfunction endclass"},
      {"class c; function f; foo_h = fff::type_id::create(",
       {kToken, "\"foo\""},
       "); endfunction endclass"},
      {"class c; function f; foo_h = foo::type_id::create(); "
       "endfunction endclass"},
      {"class c; function f; foo_h = foo::type_id::uncreate(\"foo\"); "
       "endfunction endclass"},
      {"class c; function f; foo_h = foo::xtype_id::create(\"foo\"); "
       "endfunction endclass"},
      {"class c; function f; foo_h = type_id::uncreate(\"foo\"); "
       "endfunction endclass"},  // no type specified
      {"class c; function f; foo_h =\nfoo::type_id::create(",
       {kToken, "\"foo\""},
       "); endfunction endclass"},  // with newline
      {"class c; function f; foo_h =\nfoo::\ntype_id::create(\n    ",
       {kToken, "\"foo\""},
       "); endfunction endclass"},  // with newline and spaces
      {"class c; function f; foo_h =\nfoo :: type_id :: create ( ",
       {kToken, "\"foo\""},
       " ); endfunction endclass"},  // with spaces
      {"class c; function f; foo_h = foo::type_id::create(",
       {kToken, "\"foo\""},
       ", aaa, bbb); endfunction endclass"},
      {"class c; function f; "
       "foo_h = // comment\n"  // with comments
       "foo::type_id /*comment*/ ::create(",
       {kToken, "\"foo\""},
       ",\naaa,\tbbb); endfunction endclass"},
      {"class c; function f; "  // multiple violations
       "foo_h = foo::type_id::create(",
       {kToken, "\"foo\""},
       ");\n"
       "x = y;\n"
       "bar_h = bar::type_id::create(",
       {kToken, "\"bar_\""},
       "); endfunction endclass"},
      {"class c; function f; "
       "foo_h.bar = foo::type_id::create(\"foo\"); endfunction "
       "endclass"},  // hierarchy disqualifies this check
      {"class c; function f; "
       "bar.foo_h = foo::type_id::create(\"foo\"); endfunction "
       "endclass"},  // hierarchy disqualifies this check
      {"class c; function f; "
       "for (int i = 0; i<N; ++i)\n"
       "  foo_h = foo::type_id::create(",
       {kToken, "\"foo\""},
       "); endfunction endclass"},
      {"class c; function f; "
       "for (int i = 0; i<N; ++i)\n"
       "  foo_h[i] = foo::type_id::create(\"foo\"); endfunction "
       "endclass"},  // LHS is not scalar, so is ignored
      {"class c; function f; "
       "for (int i = 0; i<N; ++i)\n"
       "  foo_h[i] = foo::type_id::create($sformat(\"foo_h_%d\", i)); "
       "endfunction endclass"},  // array assignments are not checked
      {"class c; function f; "
       "for (int i = 0; i<N; ++i)\n"
       "  foo_h[i] = foo::type_id::create($sformat(\"asdf_%d\", i)); "
       "endfunction endclass"},  // array assignments are not checked
  };

  RunLintTestCases<VerilogAnalyzer, CreateObjectNameMatchRule>(kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
