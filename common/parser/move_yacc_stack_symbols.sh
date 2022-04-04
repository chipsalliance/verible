#!/usr/bin/env bash
# Copyright 2017-2020 The Verible Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


usage() {
  cat <<EOF
Converts copy-assignments of yacc/bison-generator parsers into std::move.
This is useful if the symbol type is a std::unique_ptr.

TODO(b/126607844): This should be unnecessary with Bison 3.3 or later, as
it is supposed to handle move-only types natively.

usage: $0 < inputfile > outputfile
EOF
}

sed -e '/= yylval;/s|yylval|std::move(&)|' \
  -e '/= yyval;/s|yyval|std::move(&)|' \
  -e '/(= yyval_default)/s|yyval_default|std::move(&)|' \
  -e '/yyval = yyvsp\[1-yylen\];/s|yyval|// &|'

# Intended transformations:
#   *++yyvsp = yylval;       -> *++yyvsp = std::move(yylval);
#   *++yyvsp = yyval;        -> *++yyvsp = std::move(yyval);
#   (= yyval_default)        -> (= std::move(yyval_default))
#   yyval = yyvsp[1-yylen];  -> deleted (see comment in generated code)
