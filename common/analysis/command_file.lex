/* Copyright 2017-2020 The Verible Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

%{
#define _COMMANDFILE_FLEXLEXER_H_
#include "common/analysis/command_file_lexer.h"

#define yy_set_top_state(state)  { yy_pop_state(); yy_push_state(state); }
%}

%option c++
%option never-interactive
%option nodefault
%option nounistd
%option noyywrap
%option prefix="veribleCommandFile"
%option yyclass="verible::CommandFileLexer"
%option yylineno

LineTerminator \r|\n|\r\n
InputCharacter [^\r\n\0]
Space [ \t\f\b]

DecimalDigits [0-9]+
Alpha [a-zA-Z]

AnyChar ({Alpha}|{DecimalDigits}|-|_|:)

Name {Alpha}({Alpha}|-|_)*
Value ({AnyChar})*
QuotedValue ([^\\\"\n]|\\.)*

Quote \"

Command {Name}

ParamPrefix --

Flag {ParamPrefix}{Name}
FlagWithArg {ParamPrefix}{Name}=
Arg {Value}
Param {Value}

StartComment #
EOLComment {StartComment}({InputCharacter}*)

%x COMMAND
%x ARG
%x QUOTED_ARG
%x COMMENT

%%

{Command} {
  UpdateLocation();
  yy_push_state(COMMAND);
  return CFG_TK_COMMAND;
}

<COMMAND>{Param} {
  UpdateLocation();
  return CFG_TK_PARAM;
}

<COMMAND>{Flag} {
  UpdateLocation();
  return CFG_TK_FLAG;
}

<COMMAND>{FlagWithArg} {
  UpdateLocation();
  yy_push_state(ARG);
  return CFG_TK_FLAG_WITH_ARG;
}

<COMMAND>{EOLComment} {
  UpdateLocation();
  yy_push_state(COMMENT);
  return CFG_TK_COMMENT;
}

<ARG>{Quote} {
  UpdateLocation();
  yy_set_top_state(QUOTED_ARG);
}

<QUOTED_ARG>{QuotedValue} {
  UpdateLocation();
  return CFG_TK_ARG;
}

<QUOTED_ARG>{Quote} {
  UpdateLocation();
  yy_pop_state();
}

<ARG>{Value} {
  UpdateLocation();
  yy_pop_state();
  return CFG_TK_ARG;
}

<COMMAND>{LineTerminator} {
  UpdateLocation();
  yy_pop_state();
  return CFG_TK_NEWLINE;
}

<COMMAND><<EOF>> {
  UpdateLocation();
  yy_pop_state();
  return CFG_TK_NEWLINE;
}

<INITIAL>{LineTerminator} {
  UpdateLocation();
}

{EOLComment} {
  UpdateLocation();
  yy_push_state(COMMENT);
  return CFG_TK_COMMENT;
}

<COMMENT>{LineTerminator} {
  UpdateLocation();
  yy_pop_state();
  return CFG_TK_NEWLINE;
}

<COMMENT><<EOF>> {
  UpdateLocation();
  yy_pop_state();
  return CFG_TK_NEWLINE;
}

<*>{LineTerminator} {
  UpdateLocation();
  return CFG_TK_ERROR;
}

<*>{Space} {
  UpdateLocation();
}

<*>. {
  UpdateLocation();
  return CFG_TK_ERROR;
}

%%
