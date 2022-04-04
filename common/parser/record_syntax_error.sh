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
Inserts an action that stores syntax errors in the parser_param struct
before allowing bison error-recovery to discard it.

usage: $0 < inputfile > outputfile

Assumptions:
 1) "++yynerrs;" is a unique line in the parser skeleton that corresponds
    to the point at which a syntax error is initially detected,
    prior to any error-recovery.

 2) "param" is a ParserParam struct (from common/parser_param.h), whose
    methods include RecordSyntaxError(const YYSTYPE&);
EOF
}

sed -e '/++yynerrs;/a\
      // Automatically patched by '"$0"':\
      param->RecordSyntaxError(yylval);\
      // end of automatic patch\
'

# Intended transformation in yy.tab.cc (diff -u syntax):
cat > /dev/null <<EOF
 /*------------------------------------.
 | yyerrlab -- here on detecting error |
 \`------------------------------------*/
 yyerrlab:
   /* If not already recovering from an error, report this error.  */
   if (!yyerrstatus)
     {
       ++yynerrs;
+      // Automatically patched by $0:
+      GetParam(param)->RecordSyntaxError(yyla.value);
+      // end of automatic patch
 #if ! YYERROR_VERBOSE
       yyerror (YY_("syntax error"));
 #else
EOF
