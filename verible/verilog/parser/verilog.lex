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
/* verilog.lex is a flex-generated lexer for Verilog and SystemVerilog.
 *
 * Token enumerations come from verilog_token_enum.h,
 * (generated from verilog.tab.hh, generated from verilog.y).
 *
 */

/**
 * Verilog language Standard (2006):
 *   http://ieeexplore.ieee.org/xpl/mostRecentIssue.jsp?punumber=10779
 * System Verilog:
 *   http://standards.ieee.org/getieee/1800/download/1800-2012.pdf
 **/
#define _FLEXLEXER_H_

#include <cstdio>

#include "verible/verilog/parser/verilog-lexer.h"
#include "verible/verilog/parser/verilog-token-enum.h"

/* When testing unstructured sequences of tokens in the unit-tests,
 * the start-condition stack may be unbalanced.
 * This safeguard just prevents underflow in those cases.
 * Only tokens that appear in the default INITIAL state need to call this;
 * tokens that are qualified by a start condition should just call
 * yy_pop_state(), because they are guaranteed to have a non-empty stack.
 *
 * This is defined as a macro because the symbols referenced are
 * only available in the lexer-actions context.
 */
#define yy_safe_pop_state() if (yy_start_stack_depth > 0) { yy_pop_state(); }

/* Replace the state on the top of the stack with a new one. */
#define yy_set_top_state(state)  { yy_pop_state(); yy_push_state(state); }

/* TODO(fangism): Track yylineno with column position.  */

%}

%option 8bit
%option c++
%option case-sensitive
%option never-interactive
%option nounistd
%option nounput
%option noyywrap
%option prefix="verilog"
/* to enable stack of initial condition states: */
%option stack
%option yyclass="verilog::VerilogLexer"

/* various lexer states, INITIAL = 0 */
%x TIMESCALE_DIRECTIVE
%x AFTER_DOT
%s UDPTABLE
%s EDGES
%x EDGES_POSSIBLY
%x REAL_SCALE
%x CONSUME_NEXT_SPACES
%x MACRO_CALL_EXPECT_OPEN
%x MACRO_CALL_ARGS
%x MACRO_ARG_IGNORE_LEADING_SPACE
%x MACRO_ARG_UNLEXED
%x ATTRIBUTE_START
%x ATTRIBUTE_MIDDLE
%s COVERGROUP
%s DISCIPLINE
%s PRIMITIVE
%x PP_EXPECT_DEF_ID
%x PP_EXPECT_IF_ID
%x PP_EXPECT_INCLUDE_FILE
%x PP_MACRO_FORMALS
%x PP_MACRO_DEFAULT
%x PP_BETWEEN_ID_AND_BODY
%x PP_CONSUME_BODY
%x DEC_BASE
%x BIN_BASE
%x OCT_BASE
%x HEX_BASE
%x ENCRYPTED
%x POST_MACRO_ID
%x IGNORE_REST_OF_LINE
%x IN_EOL_COMMENT
%x LIBRARY_EXPECT_ID
%x LIBRARY_FILEPATHS

/* identifier */
Alpha [a-zA-Z]
RejectChar [\x7F-\xFF]
IdentifierStart {Alpha}|"_"
Letter {IdentifierStart}|"$"
Digit [0-9]

BasicIdentifier {IdentifierStart}({Letter}|{Digit})*
/* treat `id constants like plain identifiers */
MacroIdentifier `{BasicIdentifier}

/* verilog escaped identifiers start with '\', and end with whitespace */
EscapedIdentifier "\\"[^ \t\f\b\n]+

Identifier  {BasicIdentifier}

SystemTFIdentifier "$"{BasicIdentifier}

/* LRM: 33.3.1: file_path_spec characters include [.*?/] */
/* Windows might need '\' for path separators. */
/* PathChars [^ ,;\t\r\n] */
/* LeadingPathChars [^ -,;\t\r\n] */
PathChars ({Letter}|{Digit}|[-_.?*/])
LeadingPathChars ({Letter}|{Digit}|[_.?*/])
FilePath {LeadingPathChars}{PathChars}*

/* white space */
LineTerminator \r|\n|\r\n|\0
InputCharacter [^\r\n\0]
InputCharacterNoBackslash [^\\\r\n\0]
Space [ \t\f\b]
/*
 * To better track line numbers, LineTerminator is handled separately from Space.
 */

/* Integer literal */
DecimalDigits [0-9]+
HexDigit [0-9a-fA-F]

DecimalIntegerLiteral {DecimalDigits}
HexademicalIntegerLiteral 0(x|X){HexDigit}+

IntegerLiteral {DecimalIntegerLiteral}|{HexademicalIntegerLiteral}
/* allow underscores */
DecNumber [0-9][0-9_]*
OrderOfMagnitude 1[0]*

DecBase  \'[sS]?[dD]
BinBase  \'[sS]?[bB]
OctBase  \'[sS]?[oO]
HexBase  \'[sS]?[hH]
AnyBase  {DecBase}|{BinBase}|{OctBase}|{HexBase}

XZDigits  [xzXZ?]_*
BinDigits [0-1xzXZ_\?]+
OctDigits [0-7xzXZ_\?]+
HexDigits [0-9a-fA-FxzXZ_\?]+

UnbasedNumber      \'[01xzXZ]

BadIdentifier {DecNumber}{Identifier}
BadMacroIdentifier `{DecNumber}{BasicIdentifier}

/* Escape sequence */
EscapeSequence "\\"{InputCharacter}

/* String literal */
/* TODO: make a sub-state for easier error reporting ? */
StringContent  ([^\r\n"\\]|{EscapeSequence}|\\{LineTerminator})*
UnterminatedStringLiteral  \"{StringContent}
StringLiteral  {UnterminatedStringLiteral}\"

/* Preprocessor-evaluated string literal */
EvalStringLiteralContent ([^`]|(`[^"]))*
UnterminatedEvalStringLiteral `\"{EvalStringLiteralContent}
EvalStringLiteral {UnterminatedEvalStringLiteral}`\"

/* Preprocessor angle-bracket `include */
UnterminatedAngleBracketString <{StringContent}
AngleBracketInclude {UnterminatedAngleBracketString}>

/* attribute lists, treated like comments */
AttributesBegin "(*"
AttributesEnd   "*)"
AttributesContent ([^)]|("("[^*]*")"))*
Attributes {AttributesBegin}{AttributesContent}{AttributesEnd}

/* comments */
RestOfLine {InputCharacter}*{LineTerminator}
CommentContent ([^*]|("*"+[^/*]))*
UnterminatedComment "/*"{CommentContent}
TraditionalComment {UnterminatedComment}"*"+"/"
EndOfLineCommentStart "//"
EndOfLineComment {EndOfLineCommentStart}{RestOfLine}
Comment {TraditionalComment}|{EndOfLineComment}

/* line continuation */
LineContinuation "\\"{LineTerminator}
ContinuedLine {InputCharacter}*{LineContinuation}
 /* DiscontinuedLine: last character before LineTerminator may not be '\\' */
DiscontinuedLine {InputCharacter}*[^\\\r\n]?{LineTerminator}

/* scientific units */
S [afpnumkKMGT]

/* time units */
TU [munpf]

/* macros */
MacroCallPrefix {MacroIdentifier}{Space}*"("
  /* LRM allowing spaces makes it difficult to know without looking up the macro
   * whether the open paren starts macro arguments or is a separate set of
   * tokens that follow the macro.
   */
TraditionalCommentOrSpace {TraditionalComment}|{Space}
LineTerminatorOrEndOfLineComment {LineTerminator}|{EndOfLineComment}
IgnoreToEndOfLine {TraditionalCommentOrSpace}*{LineTerminatorOrEndOfLineComment}

/* specific pragmas */
PragmaComment    "//"{Space}*pragma
PragmaDirective  `pragma
Pragma           {PragmaComment}|{PragmaDirective}
PragmaProtected    {Pragma}{Space}+protect{Space}+begin_protected
PragmaEndProtected {Pragma}{Space}+protect{Space}+end_protected
%%

{Space}+ { UpdateLocation(); return TK_SPACE; }
{LineTerminator} { UpdateLocation(); return TK_NEWLINE; }

  /* Internal-use-only tokens to trigger library map (LRM:33) parsing mode. */
`____verible_verilog_library_begin____ { UpdateLocation(); return PD_LIBRARY_SYNTAX_BEGIN; }
`____verible_verilog_library_end____ { UpdateLocation(); return PD_LIBRARY_SYNTAX_END; }

  /* Clarification:
   * `protected and `endprotected enclose an already encrypted section.
   * `protect and `endprotect tell the compiler *to* encrypt a section.
   */

{PragmaProtected}{IgnoreToEndOfLine} {
  UpdateLocation();
  yy_push_state(ENCRYPTED);
}
`protected{IgnoreToEndOfLine} {
  UpdateLocation();
  yy_push_state(ENCRYPTED);
}
<ENCRYPTED>{PragmaEndProtected}{IgnoreToEndOfLine} {
  yyless(yyleng -1);  /* return \n to stream */
  UpdateLocation();
  yy_pop_state();
}
<ENCRYPTED>`endprotected{IgnoreToEndOfLine} {
  yyless(yyleng -1);  /* return \n to stream */
  UpdateLocation();
  yy_pop_state();
}

  /* In ENCRYPTED state, ignore all text. */
<ENCRYPTED>{RestOfLine}             {  UpdateLocation(); /* ignore */ }


{TraditionalComment} {
  UpdateLocation();
  return TK_COMMENT_BLOCK;
}

  /* Explicitly handling EOL comments in state-driven manner prevents the lexer
   * from getting stuck in a slow internal loop.  In particular, as soon as the
   * '\0' character is encountered, break out and pass it onto the parent state
   * to handle/reject.  See b/129835188.  We might need to do this for
   * block-style comments as well.  */
{EndOfLineCommentStart} {
  yy_push_state(IN_EOL_COMMENT);
  yymore();
}
<IN_EOL_COMMENT>{
  <<EOF>> {
    UpdateLocationEOF();  /* return \0 to input stream */
    yy_pop_state();
    return TK_EOL_COMMENT;
  }
  {LineContinuation} {
    yyless(yyleng-2);  /* return \\\n to input stream */
    UpdateLocation();
    yy_pop_state();
    return TK_EOL_COMMENT;
  }
  {InputCharacterNoBackslash}* {
    yymore();
  }
  "\\" {
    yymore();
  }
  {LineTerminator} {
    yyless(yyleng-1);  /* return \n to input stream */
    UpdateLocation();
    yy_pop_state();
    return TK_EOL_COMMENT;
  }
  . {
    /* Reject all other characters, defer handling to parent state. */
    yyless(yyleng-1);  /* return offending character to input stream */
    UpdateLocation();
    yy_pop_state();
    return TK_EOL_COMMENT;
  }
}
{UnterminatedComment} { UpdateLocation(); return TK_OTHER; }

  /* keywords */
1step { UpdateLocation(); return TK_1step; }
always { UpdateLocation(); return TK_always; }
and { UpdateLocation(); return TK_and; }
assign { UpdateLocation(); return TK_assign; }
begin { UpdateLocation(); return TK_begin; }
buf { UpdateLocation(); return TK_buf; }
bufif0 { UpdateLocation(); return TK_bufif0; }
bufif1 { UpdateLocation(); return TK_bufif1; }
case { UpdateLocation(); return TK_case; }
casex { UpdateLocation(); return TK_casex; }
casez { UpdateLocation(); return TK_casez; }
cmos { UpdateLocation(); return TK_cmos; }
deassign { UpdateLocation(); return TK_deassign; }
default { UpdateLocation(); return TK_default; }
defparam { UpdateLocation(); return TK_defparam; }
disable { UpdateLocation(); return TK_disable; }
edge { UpdateLocation(); yy_push_state(EDGES_POSSIBLY); return TK_edge; }
else { UpdateLocation(); return TK_else; }
end { UpdateLocation(); return TK_end; }
endcase { UpdateLocation(); return TK_endcase; }
endfunction { UpdateLocation(); return TK_endfunction; }
endmodule { UpdateLocation(); return TK_endmodule; }
<PRIMITIVE>endprimitive { UpdateLocation(); yy_safe_pop_state(); return TK_endprimitive; }
endprimitive { UpdateLocation(); return TK_endprimitive; }
endspecify { UpdateLocation(); return TK_endspecify; }
<UDPTABLE>endtable {
  UpdateLocation();
  yy_pop_state();
  return TK_endtable;
}
endtable { UpdateLocation(); return SymbolIdentifier; }
endtask { UpdateLocation(); return TK_endtask; }
event { UpdateLocation(); return TK_event; }

  /* The find* functions built-in methods, but not keywords. */
<AFTER_DOT>find {
  UpdateLocation();
  yy_pop_state();
  return TK_find;
}
<AFTER_DOT>find_index {
  UpdateLocation();
  yy_pop_state();
  return TK_find_index;
}
<AFTER_DOT>find_first {
  UpdateLocation();
  yy_pop_state();
  return TK_find_first;
}
<AFTER_DOT>find_first_index {
  UpdateLocation();
  yy_pop_state();
  return TK_find_first_index;
}
<AFTER_DOT>find_last {
  UpdateLocation();
  yy_pop_state();
  return TK_find_last;
}
<AFTER_DOT>find_last_index {
  UpdateLocation();
  yy_pop_state();
  return TK_find_last_index;
}
<AFTER_DOT>sort {
  UpdateLocation();
  yy_pop_state();
  return TK_sort;
}
<AFTER_DOT>rsort {
  UpdateLocation();
  yy_pop_state();
  return TK_rsort;
}
<AFTER_DOT>reverse {
  UpdateLocation();
  yy_pop_state();
  return TK_reverse;
}
<AFTER_DOT>shuffle {
  UpdateLocation();
  yy_pop_state();
  return TK_shuffle;
}
<AFTER_DOT>sum {
  UpdateLocation();
  yy_pop_state();
  return TK_sum;
}
<AFTER_DOT>product {
  UpdateLocation();
  yy_pop_state();
  return TK_product;
}
<AFTER_DOT>and {
  UpdateLocation();
  yy_pop_state();
  return TK_and;
}
<AFTER_DOT>or {
  UpdateLocation();
  yy_pop_state();
  return TK_or;
}
<AFTER_DOT>xor {
  UpdateLocation();
  yy_pop_state();
  return TK_xor;
}

for { UpdateLocation(); return TK_for; }
force { UpdateLocation(); return TK_force; }
forever { UpdateLocation(); return TK_forever; }
fork { UpdateLocation(); return TK_fork; }
function { UpdateLocation(); return TK_function; }
highz0 { UpdateLocation(); return TK_highz0; }
highz1 { UpdateLocation(); return TK_highz1; }
if { UpdateLocation(); return TK_if; }
ifnone { UpdateLocation(); return TK_ifnone; }
initial { UpdateLocation(); return TK_initial; }
inout { UpdateLocation(); return TK_inout; }
input { UpdateLocation(); return TK_input; }
integer { UpdateLocation(); return TK_integer; }
join { UpdateLocation(); return TK_join; }
large { UpdateLocation(); return TK_large; }
macromodule { UpdateLocation(); return TK_macromodule; }
medium { UpdateLocation(); return TK_medium; }
module { UpdateLocation(); return TK_module; }
nand { UpdateLocation(); return TK_nand; }
negedge { UpdateLocation(); return TK_negedge; }
nmos { UpdateLocation(); return TK_nmos; }
nor { UpdateLocation(); return TK_nor; }
not { UpdateLocation(); return TK_not; }
notif0 { UpdateLocation(); return TK_notif0; }
notif1 { UpdateLocation(); return TK_notif1; }
or { UpdateLocation(); return TK_or; }
<COVERGROUP>option { UpdateLocation(); return TK_option; }
option { UpdateLocation(); return SymbolIdentifier; }
output { UpdateLocation(); return TK_output; }
parameter { UpdateLocation(); return TK_parameter; }
pmos { UpdateLocation(); return TK_pmos; }
posedge { UpdateLocation(); return TK_posedge; }
primitive { UpdateLocation(); yy_push_state(PRIMITIVE); return TK_primitive; }
pull0 { UpdateLocation(); return TK_pull0; }
pull1 { UpdateLocation(); return TK_pull1; }
pulldown { UpdateLocation(); return TK_pulldown; }
pullup { UpdateLocation(); return TK_pullup; }
rcmos { UpdateLocation(); return TK_rcmos; }
real { UpdateLocation(); return TK_real; }
realtime { UpdateLocation(); return TK_realtime; }
reg { UpdateLocation(); return TK_reg; }
release { UpdateLocation(); return TK_release; }
repeat { UpdateLocation(); return TK_repeat; }
rnmos { UpdateLocation(); return TK_rnmos; }
rpmos { UpdateLocation(); return TK_rpmos; }
rtran { UpdateLocation(); return TK_rtran; }
rtranif0 { UpdateLocation(); return TK_rtranif0; }
rtranif1 { UpdateLocation(); return TK_rtranif1; }
sample { UpdateLocation(); return TK_sample; }
scalared { UpdateLocation(); return TK_scalared; }
small { UpdateLocation(); return TK_small; }
specify { UpdateLocation(); return TK_specify; }
specparam { UpdateLocation(); return TK_specparam; }
strong0 { UpdateLocation(); return TK_strong0; }
strong1 { UpdateLocation(); return TK_strong1; }
supply0 { UpdateLocation(); return TK_supply0; }
supply1 { UpdateLocation(); return TK_supply1; }
<PRIMITIVE>table { UpdateLocation(); yy_push_state(UDPTABLE); return TK_table; }
table { UpdateLocation(); return SymbolIdentifier; }
task { UpdateLocation(); return TK_task; }
time { UpdateLocation(); return TK_time; }
tran { UpdateLocation(); return TK_tran; }
tranif0 { UpdateLocation(); return TK_tranif0; }
tranif1 { UpdateLocation(); return TK_tranif1; }
tri { UpdateLocation(); return TK_tri; }
tri0 { UpdateLocation(); return TK_tri0; }
tri1 { UpdateLocation(); return TK_tri1; }
triand { UpdateLocation(); return TK_triand; }
trior { UpdateLocation(); return TK_trior; }
trireg { UpdateLocation(); return TK_trireg; }
type_option { UpdateLocation(); return TK_type_option; }
vectored { UpdateLocation(); return TK_vectored; }
wait { UpdateLocation(); return TK_wait; }
wand { UpdateLocation(); return TK_wand; }
weak0 { UpdateLocation(); return TK_weak0; }
weak1 { UpdateLocation(); return TK_weak1; }
while { UpdateLocation(); return TK_while; }
wire { UpdateLocation(); return TK_wire; }
wor { UpdateLocation(); return TK_wor; }
xnor { UpdateLocation(); return TK_xnor; }
xor { UpdateLocation(); return TK_xor; }

  /* The 1364-1995 timing checks. */
\$hold { UpdateLocation(); return TK_Shold; }
\$nochange { UpdateLocation(); return TK_Snochange; }
\$period { UpdateLocation(); return TK_Speriod; }
\$recovery { UpdateLocation(); return TK_Srecovery; }
\$setup { UpdateLocation(); return TK_Ssetup; }
\$setuphold { UpdateLocation(); return TK_Ssetuphold; }
\$skew { UpdateLocation(); return TK_Sskew; }
\$width { UpdateLocation(); return TK_Swidth; }
\$attribute { UpdateLocation(); return TKK_attribute; }

bool { UpdateLocation(); return SymbolIdentifier; }  /* Icarus-specific; currently not used */
automatic { UpdateLocation(); return TK_automatic; }
endgenerate { UpdateLocation(); return TK_endgenerate; }
generate { UpdateLocation(); return TK_generate; }
genvar { UpdateLocation(); return TK_genvar; }
localparam { UpdateLocation(); return TK_localparam; }
noshowcancelled { UpdateLocation(); return TK_noshowcancelled; }
pulsestyle_onevent { UpdateLocation(); return TK_pulsestyle_onevent; }
pulsestyle_ondetect { UpdateLocation(); return TK_pulsestyle_ondetect; }
showcancelled { UpdateLocation(); return TK_showcancelled; }
signed { UpdateLocation(); return TK_signed; }
unsigned { UpdateLocation(); return TK_unsigned; }

  /* The new 1364-2001 timing checks. */
\$fullskew { UpdateLocation(); return TK_Sfullskew; }
\$recrem { UpdateLocation(); return TK_Srecrem; }
\$removal { UpdateLocation(); return TK_Sremoval; }
\$timeskew { UpdateLocation(); return TK_Stimeskew; }

cell { UpdateLocation(); return TK_cell; }
config { UpdateLocation(); return TK_config; }
design { UpdateLocation(); return TK_design; }
endconfig { UpdateLocation(); return TK_endconfig; }
incdir {
  UpdateLocation();
  yy_push_state(LIBRARY_FILEPATHS);
  return TK_incdir;
}
include {
  UpdateLocation();
  yy_push_state(LIBRARY_FILEPATHS);
  return TK_include;
}
instance { UpdateLocation(); return TK_instance; }
liblist { UpdateLocation(); return TK_liblist; }
library {
  UpdateLocation();
  yy_push_state(LIBRARY_EXPECT_ID);
  return TK_library;
}
use { UpdateLocation(); return TK_use; }
wone { UpdateLocation(); return TK_wone; }
uwire { UpdateLocation(); return TK_uwire; }
alias { UpdateLocation(); return TK_alias; }
always_comb { UpdateLocation(); return TK_always_comb; }
always_ff { UpdateLocation(); return TK_always_ff; }
always_latch { UpdateLocation(); return TK_always_latch; }
assert { UpdateLocation(); return TK_assert; }
assume { UpdateLocation(); return TK_assume; }
before { UpdateLocation(); return TK_before; }
bind { UpdateLocation(); return TK_bind; }
bins { UpdateLocation(); return TK_bins; }
binsof { UpdateLocation(); return TK_binsof; }
bit { UpdateLocation(); return TK_bit; }
break { UpdateLocation(); return TK_break; }
byte { UpdateLocation(); return TK_byte; }
chandle { UpdateLocation(); return TK_chandle; }
class { UpdateLocation(); return TK_class; }
clocking { UpdateLocation(); return TK_clocking; }
const { UpdateLocation(); return TK_const; }
constraint { UpdateLocation(); return TK_constraint; }
context { UpdateLocation(); return TK_context; }
continue { UpdateLocation(); return TK_continue; }
cover { UpdateLocation(); return TK_cover; }
covergroup {
  UpdateLocation();
  yy_push_state(COVERGROUP);
  return TK_covergroup;
}
coverpoint { UpdateLocation(); return TK_coverpoint; }
cross { UpdateLocation(); return TK_cross; }  /* covergroup and Verilog-AMS */
dist { UpdateLocation(); return TK_dist; }
do { UpdateLocation(); return TK_do; }
endclass { UpdateLocation(); return TK_endclass; }
endclocking { UpdateLocation(); return TK_endclocking; }
endgroup {
  UpdateLocation();
  yy_safe_pop_state();
  return TK_endgroup;
}
endinterface { UpdateLocation(); return TK_endinterface; }
endpackage { UpdateLocation(); return TK_endpackage; }
endprogram { UpdateLocation(); return TK_endprogram; }
endproperty { UpdateLocation(); return TK_endproperty; }
endsequence { UpdateLocation(); return TK_endsequence; }
enum { UpdateLocation(); return TK_enum; }
expect { UpdateLocation(); return TK_expect; }
export { UpdateLocation(); return TK_export; }
extends { UpdateLocation(); return TK_extends; }
extern { UpdateLocation(); return TK_extern; }
final { UpdateLocation(); return TK_final; }
first_match { UpdateLocation(); return TK_first_match; }
foreach { UpdateLocation(); return TK_foreach; }
forkjoin { UpdateLocation(); return TK_forkjoin; }
iff { UpdateLocation(); return TK_iff; }
ignore_bins { UpdateLocation(); return TK_ignore_bins; }
illegal_bins { UpdateLocation(); return TK_illegal_bins; }
import { UpdateLocation(); return TK_import; }
inside { UpdateLocation(); return TK_inside; }
int { UpdateLocation(); return TK_int; }
interface { UpdateLocation(); return TK_interface; }
intersect { UpdateLocation(); return TK_intersect; }
join_any { UpdateLocation(); return TK_join_any; }
join_none { UpdateLocation(); return TK_join_none; }
  /* yes, "local::" is an actual token according to the LRM. */
local:: { UpdateLocation(); return TK_local_SCOPE; }
local { UpdateLocation(); return TK_local; }
logic { UpdateLocation(); return TK_logic; }
longint { UpdateLocation(); return TK_longint; }
matches { UpdateLocation(); return TK_matches; }
modport { UpdateLocation(); return TK_modport; }
new { UpdateLocation(); return TK_new; }
null { UpdateLocation(); return TK_null; }
package { UpdateLocation(); return TK_package; }
packed { UpdateLocation(); return TK_packed; }
priority { UpdateLocation(); return TK_priority; }
program { UpdateLocation(); return TK_program; }
property { UpdateLocation(); return TK_property; }
protected { UpdateLocation(); return TK_protected; }
pure { UpdateLocation(); return TK_pure; }
rand { UpdateLocation(); return TK_rand; }
randc { UpdateLocation(); return TK_randc; }
randcase { UpdateLocation(); return TK_randcase; }
randomize|std::randomize { UpdateLocation(); return TK_randomize; }
<AFTER_DOT>randomize { UpdateLocation(); yy_pop_state(); return TK_randomize; }
  /* randomize is a special function, with its own syntax.
   * The spec says [ "std::" ] "randomize" is a randomize_call.
   */
randsequence { UpdateLocation(); return TK_randsequence; }
ref { UpdateLocation(); return TK_ref; }
return { UpdateLocation(); return TK_return; }
\$root { UpdateLocation(); return TK_Sroot; }
sequence { UpdateLocation(); return TK_sequence; }
shortint { UpdateLocation(); return TK_shortint; }
shortreal { UpdateLocation(); return TK_shortreal; }
solve { UpdateLocation(); return TK_solve; }
static { UpdateLocation(); return TK_static; }
string { UpdateLocation(); return TK_string; }
struct { UpdateLocation(); return TK_struct; }
super { UpdateLocation(); return TK_super; }
tagged { UpdateLocation(); return TK_tagged; }
this { UpdateLocation(); return TK_this; }
throughout { UpdateLocation(); return TK_throughout; }
timeprecision { UpdateLocation(); return TK_timeprecision; }
timeunit { UpdateLocation(); return TK_timeunit; }
type { UpdateLocation(); return TK_type; }
typedef { UpdateLocation(); return TK_typedef; }
union { UpdateLocation(); return TK_union; }
<AFTER_DOT>unique { UpdateLocation(); yy_pop_state(); return TK_unique; }
unique { UpdateLocation(); return TK_unique; }
<AFTER_DOT>unique_index {
  UpdateLocation();
  yy_pop_state();
  return TK_unique_index;
}
\$unit { UpdateLocation(); return TK_Sunit; }
var { UpdateLocation(); return TK_var; }
virtual { UpdateLocation(); return TK_virtual; }
void { UpdateLocation(); return TK_void; }
wait_order { UpdateLocation(); return TK_wait_order; }
wildcard { UpdateLocation(); return TK_wildcard; }
<COVERGROUP>with { UpdateLocation(); return TK_with__covergroup; }
with { UpdateLocation(); return TK_with; }
within { UpdateLocation(); return TK_within; }
timeprecision_check { UpdateLocation(); return TK_timeprecision_check; }
timeunit_check { UpdateLocation(); return TK_timeunit_check; }
accept_on { UpdateLocation(); return TK_accept_on; }
checker { UpdateLocation(); return TK_checker; }
endchecker { UpdateLocation(); return TK_endchecker; }
eventually { UpdateLocation(); return TK_eventually; }
global { UpdateLocation(); return TK_global; }
implies { UpdateLocation(); return TK_implies; }
let { UpdateLocation(); return TK_let; }
nexttime { UpdateLocation(); return TK_nexttime; }
reject_on { UpdateLocation(); return TK_reject_on; }
restrict { UpdateLocation(); return TK_restrict; }
s_always { UpdateLocation(); return TK_s_always; }
s_eventually { UpdateLocation(); return TK_s_eventually; }
s_nexttime { UpdateLocation(); return TK_s_nexttime; }
s_until { UpdateLocation(); return TK_s_until; }
s_until_with { UpdateLocation(); return TK_s_until_with; }
strong { UpdateLocation(); return TK_strong; }
sync_accept_on { UpdateLocation(); return TK_sync_accept_on; }
sync_reject_on { UpdateLocation(); return TK_sync_reject_on; }
unique0 { UpdateLocation(); return TK_unique0; }
until { UpdateLocation(); return TK_until; }
until_with { UpdateLocation(); return TK_until_with; }
untyped { UpdateLocation(); return TK_untyped; }
weak { UpdateLocation(); return TK_weak; }
implements { UpdateLocation(); return TK_implements; }
interconnect { UpdateLocation(); return TK_interconnect; }
nettype { UpdateLocation(); return TK_nettype; }
soft { UpdateLocation(); return TK_soft; }
above { UpdateLocation(); return SymbolIdentifier; }  /* Verilog-AMS; currently not used */
absdelay { UpdateLocation(); return TK_absdelay; }
abstol { UpdateLocation(); return TK_abstol; }  /* Verilog-AMS */
access { UpdateLocation(); return TK_access; }  /* Verilog-AMS */
ac_stim { UpdateLocation(); return TK_ac_stim; }
aliasparam { UpdateLocation(); return TK_aliasparam; }
analog { UpdateLocation(); return TK_analog; }  /* Verilog-AMS */
analysis { UpdateLocation(); return TK_analysis; }
branch { UpdateLocation(); return SymbolIdentifier; }  /* Verilog-AMS; currently not used */
connect { UpdateLocation(); return SymbolIdentifier; }  /* Verilog-AMS; currently not used */
connectmodule { UpdateLocation(); return TK_connectmodule; }
connectrules { UpdateLocation(); return TK_connectrules; }
continuous { UpdateLocation(); return TK_continuous; }
ddt { UpdateLocation(); return SymbolIdentifier; }  /* Verilog-AMS; currently not used */
ddt_nature { UpdateLocation(); return TK_ddt_nature; }  /* Verilog-AMS */
ddx { UpdateLocation(); return SymbolIdentifier; }  /* Verilog-AMS; currently not used */
discipline {  /* Verilog-AMS */
  UpdateLocation();
  yy_push_state(DISCIPLINE);
  return TK_discipline;
}
discrete { UpdateLocation(); return TK_discrete; }  /* Verilog-AMS */
<DISCIPLINE>domain { UpdateLocation(); return TK_domain; }
domain { UpdateLocation(); return SymbolIdentifier; }
driver_update { UpdateLocation(); return TK_driver_update; }
endconnectrules { UpdateLocation(); return TK_endconnectrules; }
enddiscipline {  /* Verilog-AMS */
  UpdateLocation();
  yy_safe_pop_state();
  return TK_enddiscipline;
}
endnature { UpdateLocation(); return TK_endnature; }  /* Verilog-AMS */
endparamset { UpdateLocation(); return TK_endparamset; }
exclude { UpdateLocation(); return TK_exclude; }  /* Verilog-AMS */
final_step { UpdateLocation(); return SymbolIdentifier; }  /* Verilog-AMS; currently not used */
flicker_noise { UpdateLocation(); return TK_flicker_noise; }
flow { UpdateLocation(); return TK_flow; }  /* Verilog-AMS */
from { UpdateLocation(); return TK_from; }  /* Verilog-AMS */
ground { UpdateLocation(); return SymbolIdentifier; }  /* Verilog-AMS; currently not used */
idt { UpdateLocation(); return SymbolIdentifier; }  /* Verilog-AMS; currently not used */
idtmod { UpdateLocation(); return SymbolIdentifier; }  /* Verilog-AMS; currently not used */
idt_nature { UpdateLocation(); return TK_idt_nature; }  /* Verilog-AMS */
inf { UpdateLocation(); return TK_inf; }
infinite { UpdateLocation(); return TK_infinite; }  /* `default_decay_time */
initial_step { UpdateLocation(); return SymbolIdentifier; }  /* Verilog-AMS; currently not used */
laplace_nd { UpdateLocation(); return TK_laplace_nd; }
laplace_np { UpdateLocation(); return TK_laplace_np; }
laplace_zd { UpdateLocation(); return TK_laplace_zd; }
laplace_zp { UpdateLocation(); return TK_laplace_zp; }
last_crossing { UpdateLocation(); return TK_last_crossing; }
limexp { UpdateLocation(); return TK_limexp; }
<AFTER_DOT>max { UpdateLocation(); yy_pop_state(); return TK_max; }
merged { UpdateLocation(); return SymbolIdentifier; }  /* Verilog-AMS; currently not used */
<AFTER_DOT>min { UpdateLocation(); yy_pop_state(); return TK_min; }
nature { UpdateLocation(); return TK_nature; }  /* Verilog-AMS */
net_resolution { UpdateLocation(); return TK_net_resolution; }
noise_table { UpdateLocation(); return TK_noise_table; }
paramset { UpdateLocation(); return TK_paramset; }
potential { UpdateLocation(); return TK_potential; }  /* Verilog-AMS */
resolveto { UpdateLocation(); return TK_resolveto; }
slew { UpdateLocation(); return SymbolIdentifier; }   /* Verilog-AMS; currently not used */
split { UpdateLocation(); return SymbolIdentifier; } /* Verilog-AMS; currently not used */
timer { UpdateLocation(); return SymbolIdentifier; }  /* Verilog-AMS; currently not used */
transition { UpdateLocation(); return TK_transition; }
units { UpdateLocation(); return TK_units; }  /* Verilog-AMS */
white_noise { UpdateLocation(); return TK_white_noise; }
wreal { UpdateLocation(); return TK_wreal; }
zi_nd { UpdateLocation(); return TK_zi_nd; }
zi_np { UpdateLocation(); return TK_zi_np; }
zi_zd { UpdateLocation(); return TK_zi_zd; }
zi_zp { UpdateLocation(); return TK_zi_zp; }

 /* Operators */
".*" { UpdateLocation(); return TK_DOTSTAR; }
"<<" { UpdateLocation(); return TK_LS; }
"<<<" { UpdateLocation(); return TK_LS; }
">>"  { UpdateLocation(); return TK_RS; }
">>>" { UpdateLocation(); return TK_RSS; }
"**" { UpdateLocation(); return TK_POW; }
"<=" { UpdateLocation(); return TK_LE; }
">=" { UpdateLocation(); return TK_GE; }
"=>" { UpdateLocation(); return TK_EG; }
"+=>"|"-=>" {
  /*
   * Resolve the ambiguity between the += assignment
   * operator and +=> polarity edge path operator
   *
   * +=> should be treated as two separate tokens '+' and
   * '=>' (TK_EG), therefore we only consume the first
   * character of the matched pattern i.e. either + or -
   * and push back the rest of the matches text (=>) in
   * the input stream.
   */
  yyless(1);
  UpdateLocation();
  return yytext[0];
}
"|->" { UpdateLocation(); return TK_PIPEARROW; }
"|=>" { UpdateLocation(); return TK_PIPEARROW2; }
"*>" { UpdateLocation(); return TK_SG; }
"==?" { UpdateLocation(); return TK_WILDCARD_EQ; }
"==" { UpdateLocation(); return TK_EQ; }
"!=?" { UpdateLocation(); return TK_WILDCARD_NE; }
"!=" { UpdateLocation(); return TK_NE; }
"===" { UpdateLocation(); return TK_CEQ; }
"!==" { UpdateLocation(); return TK_CNE; }
"||" { UpdateLocation(); return TK_LOR; }
"&&" { UpdateLocation(); return TK_LAND; }
"&&&" { UpdateLocation(); return TK_TAND; }
"~|" { UpdateLocation(); return TK_NOR; }
"~^" { UpdateLocation(); return TK_NXOR; }
"^~" { UpdateLocation(); return TK_NXOR; }
"~&" { UpdateLocation(); return TK_NAND; }
"->>" { UpdateLocation(); return TK_NONBLOCKING_TRIGGER; }
"->" { UpdateLocation(); return _TK_RARROW; }
"<->" { UpdateLocation(); return TK_LOGEQUIV; }
"+:" { UpdateLocation(); return TK_PO_POS; }
"-:" { UpdateLocation(); return TK_PO_NEG; }
"<+" { UpdateLocation(); return TK_CONTRIBUTE; }
"+=" { UpdateLocation(); return TK_PLUS_EQ; }
"-=" { UpdateLocation(); return TK_MINUS_EQ; }
"*=" { UpdateLocation(); return TK_MUL_EQ; }
"/=" { UpdateLocation(); return TK_DIV_EQ; }
"%=" { UpdateLocation(); return TK_MOD_EQ; }
"&=" { UpdateLocation(); return TK_AND_EQ; }
"|=" { UpdateLocation(); return TK_OR_EQ; }
"^=" { UpdateLocation(); return TK_XOR_EQ; }
"<<=" { UpdateLocation(); return TK_LS_EQ; }
">>=" { UpdateLocation(); return TK_RS_EQ; }
"<<<=" { UpdateLocation(); return TK_LS_EQ; }
">>>=" { UpdateLocation(); return TK_RSS_EQ; }
"++" { UpdateLocation(); return TK_INCR; }
"--" {UpdateLocation(); return TK_DECR; }
"'{" { UpdateLocation(); return TK_LP; }
"::" { UpdateLocation(); return TK_SCOPE_RES; }
":=" { UpdateLocation(); return TK_COLON_EQ; }
"://" { yyless(1); UpdateLocation(); return yytext[0]; }
":/*" { yyless(1); UpdateLocation(); return yytext[0]; }
  /* Prevent ":/" from consuming the start of a comment. */
":/" { UpdateLocation(); return TK_COLON_DIV; }
"#-#" { UpdateLocation(); return TK_POUNDMINUSPOUND; }
"#=#" { UpdateLocation(); return TK_POUNDEQPOUND; }
"##" { UpdateLocation(); return TK_POUNDPOUND; }
"[*]" { UpdateLocation(); return TK_LBSTARRB; }
"[+]" { UpdateLocation(); return TK_LBPLUSRB; }
"[*" { UpdateLocation(); return TK_LBSTAR; }
"[=" { UpdateLocation(); return TK_LBEQ; }
"[->" { UpdateLocation(); return TK_LBRARROW; }
"@@" { UpdateLocation(); return TK_ATAT; }

{Attributes} {
  UpdateLocation();
  return TK_ATTRIBUTE;
}

  /* Only enter the EDGES state if the next token is '[', otherwise, rewind. */
<EDGES_POSSIBLY>{
  "[" { UpdateLocation(); yy_set_top_state(EDGES); return yytext[0]; }
  {Space}+ { UpdateLocation(); return TK_SPACE; }
  {LineTerminator} { UpdateLocation(); return TK_NEWLINE; }
  {TraditionalComment} {
    UpdateLocation();
    return TK_COMMENT_BLOCK;
  }
  {EndOfLineComment} {
    yyless(yyleng-1);  /* return \n to input stream */
    UpdateLocation();
    return TK_EOL_COMMENT;
  }
  . { yyless(0); yy_pop_state(); }
}  /* <EDGES_POSSIBLY> */

  /* end EDGES state */
<EDGES>"]" { UpdateLocation(); yy_pop_state(); return yytext[0]; }

"."        { UpdateLocation(); yy_push_state(AFTER_DOT); return yytext[0]; }
  /* single-char tokens */
[}{;:\[\],()'#=@&!?<>%|^~+*/-] { UpdateLocation(); return yytext[0]; }

{StringLiteral} { UpdateLocation(); return TK_StringLiteral; }
{EvalStringLiteral} { UpdateLocation(); return TK_EvalStringLiteral; }
{UnterminatedStringLiteral} {
  /* TODO(fangism): Is it worth returning the \n back to the input stream? */
  UpdateLocation();
  return TK_OTHER;
}
{UnterminatedEvalStringLiteral} {
  UpdateLocation();
  return TK_OTHER;
}

<PP_EXPECT_INCLUDE_FILE>{
  {StringLiteral} {
     UpdateLocation();
     yy_pop_state();
     return TK_StringLiteral;
  }
  {AngleBracketInclude} {
    UpdateLocation();
    yy_pop_state();
    return TK_AngleBracketInclude;
  }
  {Space}+ { UpdateLocation(); return TK_SPACE; }
  . { yyless(0); yy_pop_state(); }  /* anything else: back to default mode */
}  /* <PP_EXPECT_INCLUDE_FILE> */

  /* The UDP Table is a unique lexical environment. These are most
     tokens that we can expect in a table. */
<UDPTABLE>\(\?0\)    { UpdateLocation(); return '_'; }
<UDPTABLE>\(\?1\)    { UpdateLocation(); return '+'; }
<UDPTABLE>\(\?[xX]\) { UpdateLocation(); return '%'; }
<UDPTABLE>\(\?\?\)  { UpdateLocation(); return '*'; }
<UDPTABLE>\(01\)    { UpdateLocation(); return 'r'; }
<UDPTABLE>\(0[xX]\) { UpdateLocation(); return 'Q'; }
<UDPTABLE>\(b[xX]\) { UpdateLocation(); return 'q'; }
<UDPTABLE>\(b0\)    { UpdateLocation(); return 'f'; /* b0 is 10|00, but only 10 is meaningful */}
<UDPTABLE>\(b1\)    { UpdateLocation(); return 'r'; /* b1 is 11|01, but only 01 is meaningful */}
<UDPTABLE>\(0\?\)   { UpdateLocation(); return 'P'; }
<UDPTABLE>\(10\)    { UpdateLocation(); return 'f'; }
<UDPTABLE>\(1[xX]\) { UpdateLocation(); return 'M'; }
<UDPTABLE>\(1\?\)   { UpdateLocation(); return 'N'; }
<UDPTABLE>\([xX]0\) { UpdateLocation(); return 'F'; }
<UDPTABLE>\([xX]1\) { UpdateLocation(); return 'R'; }
<UDPTABLE>\([xX]\?\) { UpdateLocation(); return 'B'; }
<UDPTABLE>[bB]     { UpdateLocation(); return 'b'; }
<UDPTABLE>[lL]     { UpdateLocation(); return 'l'; /* IVL extension */ }
<UDPTABLE>[hH]     { UpdateLocation(); return 'h'; /* IVL extension */ }
<UDPTABLE>[fF]     { UpdateLocation(); return 'f'; }
<UDPTABLE>[rR]     { UpdateLocation(); return 'r'; }
<UDPTABLE>[xX]     { UpdateLocation(); return 'x'; }
<UDPTABLE>[nN]     { UpdateLocation(); return 'n'; }
<UDPTABLE>[pP]     { UpdateLocation(); return 'p'; }
<UDPTABLE>[\?\*\-:;] { UpdateLocation(); return yytext[0]; }
<UDPTABLE>[01]+    {
  /* Return one digit at a time. */
  yyless(1);
  UpdateLocation();
  return yytext[0];
}
<UDPTABLE>{DecNumber} {
  UpdateLocation();
  return TK_OTHER; /* Should reject TK_DecNumber inside UDPTABLE */
}

<EDGES>"01" { UpdateLocation(); return TK_edge_descriptor; }
<EDGES>"0x" { UpdateLocation(); return TK_edge_descriptor; }
<EDGES>"0z" { UpdateLocation(); return TK_edge_descriptor; }
<EDGES>"10" { UpdateLocation(); return TK_edge_descriptor; }
<EDGES>"1x" { UpdateLocation(); return TK_edge_descriptor; }
<EDGES>"1z" { UpdateLocation(); return TK_edge_descriptor; }
<EDGES>"x0" { UpdateLocation(); return TK_edge_descriptor; }
<EDGES>"x1" { UpdateLocation(); return TK_edge_descriptor; }
<EDGES>"z0" { UpdateLocation(); return TK_edge_descriptor; }
<EDGES>"z1" { UpdateLocation(); return TK_edge_descriptor; }

<TIMESCALE_DIRECTIVE>{
  {TU}?s {
    /* timescale unit, like s, ms, us, ns, ps */
    UpdateLocation();
    return TK_timescale_unit;
  }
  "/" { UpdateLocation(); return yytext[0]; }
  {OrderOfMagnitude} {
    UpdateLocation();
    return TK_DecNumber;
  }
  {DecNumber}(\.{DecNumber})?{TU}?s {
    UpdateLocation();
    return TK_TimeLiteral;
  }
  {Space}+ { UpdateLocation(); return TK_SPACE; }
  {LineTerminator} { UpdateLocation(); return TK_NEWLINE; }
  {TraditionalComment} {
    UpdateLocation();
    return TK_COMMENT_BLOCK;
  }
  {EndOfLineComment} {
    yyless(yyleng-1);  /* return \n to input stream */
    UpdateLocation();
    return TK_EOL_COMMENT;
  }
  /* any other tokens, return to previous state */
  . { yyless(0); yy_pop_state(); }
}  /* <TIMESCALE_DIRECTIVE> */


{Identifier} {
  UpdateLocation();
  /* The original reference lexer looked up identifiers in the symbol table
   * to return an enumeral subtype of identifier (param, type, function),
   * which implemented essentially a context-sensitive grammar,
   * however, for outline generation, we just return a catch-all
   * vanilla identifier.
   */
  return SymbolIdentifier;
}
{EscapedIdentifier} {
  UpdateLocation();
  return EscapedIdentifier;
}

  /* All other $identifiers: */
{SystemTFIdentifier} { UpdateLocation(); return SystemTFIdentifier; }

{DecBase} {
  UpdateLocation();
  yy_push_state(DEC_BASE);
  return TK_DecBase;
}
<DEC_BASE>{
  {DecNumber} {
    UpdateLocation();
    yy_pop_state();
    return TK_DecDigits;
  }
  {XZDigits} {
    UpdateLocation();
    yy_pop_state();
    return TK_XZDigits;
  }
  {Space}+ { UpdateLocation(); return TK_SPACE; }
  {LineTerminator} { UpdateLocation(); return TK_NEWLINE; }
  /* any other tokens, return to previous state */
  . { yyless(0); yy_pop_state(); }
}

{BinBase} {
  UpdateLocation();
  yy_push_state(BIN_BASE);
  return TK_BinBase;
}
<BIN_BASE>{
  {BinDigits} {
    UpdateLocation();
    yy_pop_state();
    return TK_BinDigits;
  }
  {Space}+ { UpdateLocation(); return TK_SPACE; }
  {LineTerminator} { UpdateLocation(); return TK_NEWLINE; }
  /* any other tokens, return to previous state */
  . { yyless(0); yy_pop_state(); }
}

{OctBase} {
  UpdateLocation();
  yy_push_state(OCT_BASE);
  return TK_OctBase;
}
<OCT_BASE>{
  {OctDigits} {
    UpdateLocation();
    yy_pop_state();
    return TK_OctDigits;
  }
  {Space}+ { UpdateLocation(); return TK_SPACE; }
  {LineTerminator} { UpdateLocation(); return TK_NEWLINE; }
  /* any other tokens, return to previous state */
  . { yyless(0); yy_pop_state(); }
}

{HexBase} {
  UpdateLocation();
  yy_push_state(HEX_BASE);
  return TK_HexBase;
}
<HEX_BASE>{
  {HexDigits} {
    UpdateLocation();
    yy_pop_state();
    return TK_HexDigits;
  }
  {Space}+ { UpdateLocation(); return TK_SPACE; }
  {LineTerminator} { UpdateLocation(); return TK_NEWLINE; }
  /* any other tokens, return to previous state */
  . { yyless(0); yy_pop_state(); }
}

{UnbasedNumber}  { UpdateLocation(); return TK_UnBasedNumber; }

  /* Decimal numbers are the usual. But watch out for the UDPTABLE
     mode, where there are no decimal numbers. Reject the match if we
     are in the UDPTABLE state.

     Use of the REJECT macro causes flex to emit code that calls
     YY_FATAL_ERROR in an infinite loop, via an internal preprocessor
     macro named YY_USES_REJECT.  Thus, we don't catch it here, but let
     the parser reject it.  [b/20249425]
   */

{DecNumber} {
  UpdateLocation();
  return TK_DecNumber;
}

  /* This rule handles scaled time values for SystemVerilog. */
{DecNumber}(\.{DecNumber})?{TU}?s { UpdateLocation(); return TK_TimeLiteral; }
  /* There may be contexts where a space is allowed before the unit. */

  /* These rules handle the scaled real literals from Verilog-AMS. The
     value is a number with a single letter scale factor. If
     verilog-ams is not enabled, then reject this rule. If it is
     enabled, then collect the scale and use it to scale the value. */
{DecNumber}\.{DecNumber}/{S} {
      yy_push_state(REAL_SCALE);
      yymore();
}

{DecNumber}/{S} {
      yy_push_state(REAL_SCALE);
      yymore();
}

<REAL_SCALE>{S} {
      UpdateLocation();
      yy_pop_state();
      return TK_RealTime;
}

{DecNumber}\.{DecNumber}([Ee][+-]?{DecNumber})? {
      UpdateLocation();
      return TK_RealTime;
}

{DecNumber}[Ee][+-]?{DecNumber} {
      UpdateLocation();
      return TK_RealTime;
}

<IGNORE_REST_OF_LINE>{RestOfLine} {
  yyless(yyleng -1);  /* return \n to input stream */
  UpdateLocation();
  yy_pop_state();
  /* ignore */
}

`timescale {
  UpdateLocation();
  yy_push_state(TIMESCALE_DIRECTIVE);
  return DR_timescale;
}
`celldefine    { UpdateLocation(); return DR_celldefine; }
`endcelldefine { UpdateLocation(); return DR_endcelldefine; }
`resetall { UpdateLocation(); return DR_resetall; }
`unconnected_drive { UpdateLocation(); return DR_unconnected_drive; }
`nounconnected_drive { UpdateLocation(); return DR_nounconnected_drive; }

  /* From 1364-2005 Chapter 19. */
`pragma {
  UpdateLocation();
  yy_push_state(IGNORE_REST_OF_LINE);
  return DR_pragma;
}

  /* From 1364-2005 Annex D. */
`default_decay_time      {  UpdateLocation(); return DR_default_decay_time; }
`default_trireg_strength {  UpdateLocation(); return DR_default_trireg_strength; }
`delay_mode_distributed  {  UpdateLocation(); return DR_delay_mode_distributed; }
`delay_mode_path         {  UpdateLocation(); return DR_delay_mode_path; }
`delay_mode_unit         {  UpdateLocation(); return DR_delay_mode_unit; }
`delay_mode_zero         {  UpdateLocation(); return DR_delay_mode_zero; }

  /* From other places, e.g. Verilog-XL. */
`disable_portfaults      {  UpdateLocation(); return DR_disable_portfaults; }
`enable_portfaults       {  UpdateLocation(); return DR_enable_portfaults; }
`endprotect              {  UpdateLocation(); return DR_endprotect; }
`nosuppress_faults       {  UpdateLocation(); return DR_nosuppress_faults; }
`protect                 {  UpdateLocation(); return DR_protect; }
`suppress_faults         {  UpdateLocation(); return DR_suppress_faults; }
`uselib {
  UpdateLocation();
  yy_push_state(IGNORE_REST_OF_LINE);
  return DR_uselib;
}

`begin_keywords { UpdateLocation(); return DR_begin_keywords; }
`end_keywords   { UpdateLocation(); return DR_end_keywords; }

`default_nettype { UpdateLocation(); return DR_default_nettype; }

  /* This lexer is intended for a parser that accepts *un-preprocessed* source. */

`define { UpdateLocation(); yy_push_state(PP_EXPECT_DEF_ID); return PP_define; }
  /* TODO(fangism): store definition body token sequences
   * to enable preprocessing.
   */

  /* In the PP_BETWEEN_ID_AND_BODY state, ignore ignore spaces before
   * macro definition body/contents.
   */
<PP_BETWEEN_ID_AND_BODY>{
  {Space}+ {
    UpdateLocation();
    return TK_SPACE;
  }
  .|{LineTerminator} {
    yyless(0);  /* return any other character to stream */
    UpdateLocation();
    yy_set_top_state(PP_CONSUME_BODY);
  }
}  /* PP_BETWEEN_ID_AND_BODY */

  /* In the PP_CONSUME_BODY state, ignore text until end of non-continued line. */
<PP_CONSUME_BODY>{
  /* MacroDefinitionBody is effectively: {ContinuedLine}*{DiscontinuedLine} */
  {ContinuedLine} {
    /* If code abruptly terminates (EOF) after a line continuation,
     * just return accumulated text.  Fixes b/37984133. */
    if (YY_CURRENT_BUFFER->yy_buffer_status == YY_BUFFER_EOF_PENDING) {
      yyless(yyleng-1);  /* return \n to input stream */
      UpdateLocation();
      yy_pop_state();
      return PP_define_body;
    }
    yymore();
  }
  {DiscontinuedLine} {
    yyless(yyleng-1);  /* return \n to input stream */
    UpdateLocation();
    yy_pop_state();
    /* Return a dummy token so the Location range of the definition (@$) spans
     * the (ignored) definition body (in the parser).
     */
    return PP_define_body;
  }
  {InputCharacter}* {
    /* This case matches when a line does not end with a continuation or \n. */
    yymore();
  }
  <<EOF>> {
    UpdateLocationEOF();  /* return \0 to input stream */
    yy_pop_state();
    return PP_define_body;
  }
}

`else { UpdateLocation(); return PP_else; }
`elsif { UpdateLocation(); yy_push_state(PP_EXPECT_IF_ID); return PP_elsif; }
`endif { UpdateLocation(); return PP_endif; }
`ifdef { UpdateLocation(); yy_push_state(PP_EXPECT_IF_ID); return PP_ifdef; }
`ifndef { UpdateLocation(); yy_push_state(PP_EXPECT_IF_ID); return PP_ifndef; }
`undef { UpdateLocation(); yy_push_state(PP_EXPECT_IF_ID); return PP_undef; }
`include { UpdateLocation();
           yy_push_state(PP_EXPECT_INCLUDE_FILE);
           return PP_include;
}

<PP_EXPECT_IF_ID>{
  {Space}+ { UpdateLocation(); return TK_SPACE; }
  {TraditionalComment} {
    UpdateLocation();
    return TK_COMMENT_BLOCK;
  }
  {EndOfLineComment} {
    yyless(yyleng-1);  /* return \n to input stream */
    UpdateLocation();
    return TK_EOL_COMMENT;
  }
  {BasicIdentifier} {
    UpdateLocation();
    yy_pop_state();
    return PP_Identifier;
  }
  .|{LineTerminator} {
    /* Return to previous state and re-lex. */
    yyless(0);
    yy_pop_state();
    UpdateLocation();
  }
}  /* <PP_EXPECT_IF_ID> */

<PP_EXPECT_DEF_ID>{
  {Space}+ { UpdateLocation(); return TK_SPACE; }
  {LineTerminator} { UpdateLocation(); return TK_NEWLINE; }
  {BasicIdentifier}"(" {
    /* When open paren immediately follows the ID, expect formal parameters. */
    yyless(yyleng-1);  /* return '(' to stream */
    UpdateLocation();
    yy_set_top_state(PP_MACRO_FORMALS);
    return PP_Identifier;
  }
  {BasicIdentifier} {
    UpdateLocation();
    /* ignore spaces that may follow */
    yy_set_top_state(PP_BETWEEN_ID_AND_BODY);
    return PP_Identifier;
  }
}  /* <PP_EXPECT_DEF_ID> */

<PP_MACRO_FORMALS>{
  /* ignores trailing spaces */
  {Space}+ { UpdateLocation(); return TK_SPACE; }
  {LineTerminator} { UpdateLocation(); return TK_NEWLINE; }
  "(" {
    ++balance_;
    UpdateLocation();
    return yytext[0];
  }
  ")" {
    --balance_;
    UpdateLocation();
    if (balance_ == 0) {
      /* ignore spaces that may follow */
      yy_set_top_state(PP_BETWEEN_ID_AND_BODY);
    }
    return yytext[0];
  }
  "," { UpdateLocation(); return yytext[0]; }
  {BasicIdentifier} {
    UpdateLocation();
    return PP_Identifier;
  }
  = {
    macro_arg_length_ = 0;
    yy_push_state(PP_MACRO_DEFAULT);
    yy_push_state(CONSUME_NEXT_SPACES);  /* ignore leading space */
    UpdateLocation();
    /* balance_ == 1 */
    return yytext[0];
  }
}  /* PP_MACRO_FORMALS */

  /* The LRM permits empty default parameter value after the =. */
<PP_MACRO_DEFAULT>{
  {Space}+ { /* don't know yet if this is a trailing space */ yymore(); }
  {LineTerminator} { /* don't know yet if this is a trailing \n */ yymore(); }
  [^,(){}" \n\r]+ {
    yymore();
    macro_arg_length_ = yyleng;
  }
  "("|"{" {
    ++balance_;
    yymore();
    macro_arg_length_ = yyleng;
  }
  ")"|"}" {
    if (balance_ == 1) {
      /* defer balance_ adjustment to PP_MACRO_FORMALS state */
      yyless(macro_arg_length_);
      UpdateLocation();
      yy_pop_state();  /* back to PP_MACRO_FORMALS, which will ignore spaces */
      return PP_default_text;
    } else {
      --balance_;
      yymore();
      macro_arg_length_ = yyleng;
    }
  }
  , {
    if (balance_ == 1) {
      yyless(macro_arg_length_);
      UpdateLocation();
      yy_pop_state();  /* back to PP_MACRO_FORMALS, which will ignore spaces */
      return PP_default_text;
    } else {
      yymore();
      macro_arg_length_ = yyleng;
    }
  }
  {StringLiteral} {
    yymore();
    macro_arg_length_ = yyleng;
  }
  /* Do macro default values need {EvalStringLiteral}? */
}  /* <PP_MACRO_DEFAULT> */

<MACRO_CALL_EXPECT_OPEN>{
  {Space}+ {
    UpdateLocation();
    return TK_SPACE;
  }
  "(" {
    UpdateLocation();
    yy_set_top_state(MACRO_CALL_ARGS);
    yy_push_state(MACRO_ARG_IGNORE_LEADING_SPACE);
    return yytext[0];
  }
}

<MACRO_CALL_ARGS>{
  , {
    UpdateLocation();
    yy_push_state(MACRO_ARG_IGNORE_LEADING_SPACE);
    return yytext[0];
  }
  ")"{IgnoreToEndOfLine} {
    // let trailing comments spaces be handled by default lexer state
    yyless(1);
    UpdateLocation();
    yy_pop_state();
    return MacroCallCloseToEndLine;
  }
  ")" {
    UpdateLocation();
    yy_pop_state();
    return yytext[0];
  }
}  /* <MACRO_CALL_ARGS> */

<MACRO_ARG_IGNORE_LEADING_SPACE>{
  /* Ignore leading space before macro arguments. */
  {Space}+ { UpdateLocation(); return TK_SPACE; }
  {LineTerminator} { UpdateLocation(); return TK_NEWLINE; }
  /* We intentionally defer comment-lexing until macro argument expansion. */
  . {
    yyless(0);
    UpdateLocation();
    yy_set_top_state(MACRO_ARG_UNLEXED);
    macro_arg_length_ = 0;
  }
}  /* <MACRO_ARG_IGNORE_LEADING_SPACE> */

<MACRO_ARG_UNLEXED>{
  /* Accumulate text until next , or ) (balanced). */
  /* At this point, we do not know if this is a trailing space to be removed.
   * Keep track of macro_arg_length_ track the position of the last non-space
   * character, so that we can pass it to yyless() to backtrack.
   */
  {Space}+ { yymore(); }
  {LineTerminator} { yymore(); }

  {Comment} { macro_arg_length_ = yyleng; yymore(); }
  {UnterminatedComment} {
    macro_arg_length_ = yyleng;
    UpdateLocation();
    return TK_OTHER;
  }

  {StringLiteral} { macro_arg_length_ = yyleng; yymore(); }
  {EvalStringLiteral} { macro_arg_length_ = yyleng; yymore(); }
  /* [^(){},"]+ { yymore(); } */
  {UnterminatedStringLiteral} {
    macro_arg_length_ = yyleng;
    UpdateLocation();
    return TK_OTHER;
  }

  "{" { macro_arg_length_ = yyleng; yymore(); ++balance_; }
  "}" { macro_arg_length_ = yyleng; yymore(); --balance_; }
  /* TODO(fangism): check that unlexed text is balanced.
   * If it is not, return some error token.
   */
  "(" { macro_arg_length_ = yyleng; yymore(); ++balance_; }
  ")" {
    if (balance_ == 0) {
      /* Pass this to previous start condition. */

      /* Return ')' to input buffer, and rollback to the last non-space
       * position in the yytext buffer (before this ')').
       */
      yyless(macro_arg_length_);
      UpdateLocation();
      yy_set_top_state(CONSUME_NEXT_SPACES);
      if (yyleng > 0) {
        /* Return as an argument only if there was any non-whitespace text. */
        return MacroArg;
      }
    } else {
      macro_arg_length_ = yyleng;
      yymore();
      --balance_;
    }
  }
  , {
    if (balance_ == 0) {
      /* Pass this to previous start condition. */

      /* Return ',' to input buffer, and rollback to the last non-space
       * position in the yytext buffer (before this ',').
       */
      yyless(macro_arg_length_);
      UpdateLocation();
      yy_set_top_state(CONSUME_NEXT_SPACES);
      if (yyleng > 0) {
        /* Return as an argument only if there was any non-whitespace text. */
        return MacroArg;
      }
    } else {
      macro_arg_length_ = yyleng;
      yymore();
    }
  }
  . {
    /* catch-all in this start-condition */
    macro_arg_length_ = yyleng;
    yymore();
  }
}  /* <MACRO_ARG_UNLEXED> */

<CONSUME_NEXT_SPACES>{
  {Space}+ { UpdateLocation(); return TK_SPACE; }
  {LineTerminator} { UpdateLocation(); return TK_NEWLINE; }
  . {
    /* Defer to previous state on stack. */
    yyless(0);
    UpdateLocation();
    yy_pop_state();
  }
}  /* <CONSUME_NEXT_SPACES> */

  /* To prevent matching other `directives, this pattern must appear last. */
{MacroIdentifier} {
  /* If this text runs up to an EOF, handle it here,
   * rather than enter other state.  Fixes b/37984133.  */
  if (YY_CURRENT_BUFFER->yy_buffer_status == YY_BUFFER_EOF_PENDING) {
    UpdateLocation();
    return MacroIdentifier;
  }
  macro_id_length_ = yyleng;  /* save position of macro-id */
  yymore();
  yy_push_state(POST_MACRO_ID);
}

  /* Macro identifiers on their own line are treated as MacroIdItem. */
<POST_MACRO_ID>{
  {IgnoreToEndOfLine} {
    yyless(macro_id_length_);
    UpdateLocation();
    yy_pop_state();
    return MacroIdItem;
  }

  {Space}*{AnyBase} {
    /* Treat `MACRO '{base}{number} as a constant width. */
    yyless(macro_id_length_);
    UpdateLocation();
    yy_pop_state();
    return MacroNumericWidth;
  }

  /* Macro calls are treated as a special token that can serve as a placeholder
   * for statements or expressions. */
  {Space}*"(" {
    /* Interpret as macro-call.
     * Macro calls can be nested, but we don't need a stack of balance_
     * because the macro argument text is not lexed here, even if it contains
     * more macro calls.  Their expansion must be deferred.
     */
    balance_ = 0;
    yyless(macro_id_length_);
    UpdateLocation();
    yy_set_top_state(MACRO_CALL_EXPECT_OPEN);
    return MacroCallId;
  }

  . {
    /* Treat other macro identifiers like plain identifiers. */
    yyless(macro_id_length_);
    UpdateLocation();
    yy_pop_state();
    return MacroIdentifier;
  }
}  /* <POST_MACRO_ID> */

<AFTER_DOT>{
  {Identifier} {
    UpdateLocation();
    yy_pop_state();
    return SymbolIdentifier;
  }
  {EscapedIdentifier} {
    UpdateLocation();
    yy_pop_state();
    return EscapedIdentifier;
  }

  {Space}+ { UpdateLocation(); return TK_SPACE; }
  {LineTerminator} { UpdateLocation(); return TK_NEWLINE; }
  {TraditionalComment} {
    UpdateLocation();
    return TK_COMMENT_BLOCK;
  }
  {EndOfLineComment} {
    yyless(yyleng-1);  /* return \n to input stream */
    UpdateLocation();
    return TK_EOL_COMMENT;
  }

  . {
    yyless(0);
    yy_pop_state();
  }
}  /* <AFTER_DOT> */

<LIBRARY_EXPECT_ID>{
  {Identifier} {
    UpdateLocation();
    yy_set_top_state(LIBRARY_FILEPATHS);
    return SymbolIdentifier;
  }
  {Space}+ { UpdateLocation(); return TK_SPACE; }
  {LineTerminator} { UpdateLocation(); return TK_NEWLINE; }
  {TraditionalComment} {
    UpdateLocation();
    return TK_COMMENT_BLOCK;
  }
  {EndOfLineComment} {
    yyless(yyleng-1);  /* return \n to input stream */
    UpdateLocation();
    return TK_EOL_COMMENT;
  }
  . {
    yyless(0);
    yy_pop_state();
  }
}
<LIBRARY_FILEPATHS>{
  {FilePath} {
    UpdateLocation();
    return TK_FILEPATH;
  }
  , { UpdateLocation(); return ','; }
  {Space}+ { UpdateLocation(); return TK_SPACE; }
  {LineTerminator} { UpdateLocation(); return TK_NEWLINE; }

  /* Don't support comments here, because
   * slash-star could be interpreted as the start of a path.
   */

  . {
    yyless(0);
    yy_pop_state();
  }
}

{RejectChar} { UpdateLocation(); return TK_OTHER; }

`` {
  /* Preprocessing token concatenation:
   * Even though this should only be legal inside a macro definition,
   * we must support the token concatenation operator here so that
   * recursive lexing will work.
   */
  UpdateLocation(); return PP_TOKEN_CONCAT;
}

` { UpdateLocation(); return TK_OTHER; /* tick should never be alone */ }

  /* All other single-character tokens */
. { UpdateLocation(); return yytext[0]; }

{LineContinuation} {
  yyless(yyleng-1);  /* return \n to input stream */
  UpdateLocation();
  return TK_LINE_CONT;
}

{BadIdentifier} { UpdateLocation(); return TK_OTHER; }
{BadMacroIdentifier} { UpdateLocation(); return TK_OTHER; }

  /* Final catchall. something got lost or mishandled. */
<*>.|\n { UpdateLocation(); return TK_OTHER; }

%%
