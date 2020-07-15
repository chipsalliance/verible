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

%code requires{
#include "common/parser/parser_param.h"
}

%{
/**
verilog.y
yacc/bison LR(1) grammar for SystemVerilog.

The syntax tree constructed by the semantic actions in this file are
fragile and subject to change without notice.
Functionality that relies directly on this structure should be isolated under
//verilog/CST/... (concrete syntax tree) and unit-tested accordingly.
**/

#include "common/parser/bison_parser_common.h"
#include "common/text/tree_utils.h"
#include "common/util/casts.h"
#include "common/util/logging.h"
#include "verilog/CST/declaration.h"
#include "verilog/CST/DPI.h"
#include "verilog/CST/expression.h"
#include "verilog/CST/functions.h"
#include "verilog/CST/module.h"
#include "verilog/CST/parameters.h"
#include "verilog/CST/port.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/CST/verilog_treebuilder_utils.h"

/***
 * Verilog Language Standard (IEEE 1364-2005):
 *   http://ieeexplore.ieee.org/xpl/mostRecentIssue.jsp?punumber=10779
 *
 * System Verilog (IEEE 1800-2017):
 *   https://standards.ieee.org/standard/1800-2017.html
 *     This is the Language Reference Manual, or LRM.
 *
 * Verilog-AMS:
 *   http://www.accellera.org/images/downloads/standards/v-ams/VAMS-LRM-2-4.pdf
 *
 * System Verilog Assertions:
 *   http://www.eecs.umich.edu/courses/eecs578/eecs578.f15/miniprojects/SVAproject/manuals/SVA_manual.pdf
 *   http://vlsi.pro/sva-sequences-other-operators/
 **/


namespace verilog {

using verible::ParserParam;
using verible::MakeNode;
using verible::MakeTaggedNode;
using verible::ExtendNode;
using verible::ForwardChildren;
using verible::SetChild;
using verible::SymbolCastToNode;
using verible::SymbolPtr;
using verible::SyntaxTreeNode;
using verible::down_cast;

using N = NodeEnum;

constexpr std::nullptr_t qualifier_placeholder = nullptr;
constexpr std::nullptr_t expression_placeholder = nullptr;


using std::move;

static std::nullptr_t Recover() {
  // TODO(fangism): return a useful ErrorNode or ErrorToken, using the
  // most recent recovered error token in the ParserParam.
  return nullptr;
}

// Transforms:
// (sublist, separator), item -> sublist +separator +item
// TODO(fangism): if generally useful, factor into concrete_syntax_tree.h
static SymbolPtr ExtendFirstSublist(SymbolPtr& pair, SymbolPtr item) {
  // pair node is a 2-tuple (sublist, separator),
  // where sublist is a SyntaxTreeNode.
  auto& pair_node = down_cast<SyntaxTreeNode&>(*pair);
  auto& sublist = pair_node[0];
  auto& separator = pair_node[1];
  return ExtendNode(sublist, separator, item);
}

// Transforms:
// (..., sublist), item -> (..., sublist +item)
static SymbolPtr ExtendLastSublist(SymbolPtr& list, SymbolPtr& item) {
  auto& list_node = down_cast<SyntaxTreeNode&>(*list);
  auto& last_sublist = down_cast<SyntaxTreeNode&>(
      *list_node.mutable_children().back());
  last_sublist.Append(move(item));
  return move(list);
}

// Transforms:
// ((..., sublist), separator), item -> (..., sublist +separator +item)
static SymbolPtr ExtendLastSublistWithSeparator(
    SymbolPtr& pair, SymbolPtr& item) {
  auto& pair_node = down_cast<SyntaxTreeNode&>(*pair);
  auto& list = pair_node[0];
  auto& separator = pair_node[1];
  /* Extend the last sublist. */
  auto& list_node = down_cast<SyntaxTreeNode&>(*list);
  auto& sublist_node = down_cast<SyntaxTreeNode&>(
      *list_node.mutable_children().back());
  sublist_node.Append(move(separator), move(item));
  return move(list);
}

// (Below): These helper forwarding functions help ensure consistent structure,
// and check that the correct number of arguments are passed:

static SymbolPtr MakePackedDimensionsNode(SymbolPtr& arg) {
  return MakeTaggedNode(N::kPackedDimensions, arg);
}

static SymbolPtr MakeUnpackedDimensionsNode(SymbolPtr& arg) {
  return MakeTaggedNode(N::kUnpackedDimensions, arg);
}

%}

%debug
%verbose
%expect 0
%define api.pure
%param { ::verible::ParserParam* param }
/* TODO(fangism): this prefix name should point to an adaptation of yylex */
%define api.prefix {verilog_}
%define api.value.type {::verible::SymbolPtr}

// LINT.IfChange
%token PP_Identifier
%token PP_include "`include"
%token PP_define "`define"
%token PP_define_body "<<`define-tokens>>"
%token PP_ifdef "`ifdef"
%token PP_ifndef "`ifndef"
/* `if doesn't exist for Verilog */
%token PP_else "`else"
%token PP_elsif "`elsif"
%token PP_endif "`endif"
%token PP_undef "`undef"
%token PP_default_text "<<default-text>>"

%token DR_timescale "`timescale"
%token DR_resetall "`resetall"
%token DR_celldefine "`celldefine"
%token DR_endcelldefine "`endcelldefine"
%token DR_unconnected_drive "`unconnected_drive"
%token DR_nounconnected_drive "`nounconnected_drive"
%token DR_default_nettype "`default_nettype"
%token DR_suppress_faults "`suppress_faults"
%token DR_nosuppress_faults "`nosuppress_faults"
%token DR_enable_portfaults "`enable_portfaults"
%token DR_disable_portfaults "`disable_portfaults"
%token DR_delay_mode_distributed "`delay_mode_distributed"
%token DR_delay_mode_path "`delay_mode_path"
%token DR_delay_mode_unit "`delay_mode_unit"
%token DR_delay_mode_zero "`delay_mode_zero"
%token DR_default_decay_time "`default_decay_time"
%token DR_default_trireg_strength "`default_trireg_strength"
%token DR_pragma "`pragma"
%token DR_uselib "`uselib"
%token DR_begin_keywords "`begin_keywords"
%token DR_end_keywords "`end_keywords"
%token DR_protect "`protect"
%token DR_endprotect "`endprotect"


/**
The grammar in the SystemVerilog LRM is written in terms of type-specific
identifiers, like module_identifier and class_identifier, however,
without preprocessing, we cannot know the type of an identifier that
is not locally defined, so the grammar here uses only generic identifiers.
**/

// gen_tokenizer annotations existed in the original verilog.y grammar
// which was a way to generate a syntax highlighter.
// TODO(fangism): Remove these if they are not useful.

// gen_tokenizer start IDENTIFIER
%token SymbolIdentifier        // encapsulates all of the above
%token EscapedIdentifier        // starts with \ and ends with whitespace
%token SystemTFIdentifier       // $identifier
%token MacroIdentifier          // `identifier
%token MacroCallId
%token MacroIdItem
%token MacroNumericWidth
// gen_tokenizer stop

// gen_tokenizer start NUMERIC_LITERAL
%token TK_DecNumber             // DEC_NUMBER
%token TK_RealTime              // REALTIME
%token TK_TimeLiteral           // TIME_LITERAL
%token TK_DecBase
%token TK_DecDigits
%token TK_XZDigits
%token TK_BinBase
%token TK_BinDigits
%token TK_OctBase
%token TK_OctDigits
%token TK_HexBase
%token TK_HexDigits
%token TK_UnBasedNumber         // UNBASED_NUMBER
// gen_tokenizer stop

// gen_tokenizer start STRING_LITERAL
%token TK_StringLiteral         // STRING
%token TK_EvalStringLiteral     // STRING
// gen_tokenizer stop

// gen_tokenizer start KEYWORD
/* The base tokens from 1364-1995. */
%token TK_1step "1step"
%token TK_always "always"
%token TK_and "and"
%token TK_assign "assign"
%token TK_begin "begin"
%token TK_buf "buf"
%token TK_bufif0 "bufif0"
%token TK_bufif1 "bufif1"
%token TK_case "case"
%token TK_casex "casex"
%token TK_casez "casez"
%token TK_cmos "cmos"
%token TK_deassign "deassign"
%token TK_default "default"
%token TK_defparam "defparam"
%token TK_disable "disable"
%token TK_edge "edge"
%token TK_else "else"
%token TK_end "end"
%token TK_endcase "endcase"
%token TK_endfunction "endfunction"
%token TK_endmodule "endmodule"
%token TK_endprimitive "endprimitive"
%token TK_endspecify "endspecify"
%token TK_endtable "endtable"
%token TK_endtask "endtask"
%token TK_event "event"
%token TK_for "for"
%token TK_force "force"
%token TK_forever "forever"
%token TK_fork "fork"
%token TK_function "function"
%token TK_highz0 "highz0"
%token TK_highz1 "highz1"
%token TK_if "if"
%token TK_ifnone "ifnone"
%token TK_initial "initial"
%token TK_inout "inout"
%token TK_input "input"
%token TK_integer "integer"
%token TK_join "join"
%token TK_large "large"
%token TK_macromodule "macromodule"
%token TK_medium "medium"
%token TK_module "module"
%token TK_nand "nand"
%token TK_negedge "negedge"
%token TK_nmos "nmos"
%token TK_nor "nor"
%token TK_not "not"
%token TK_notif0 "notif0"
%token TK_notif1 "notif1"
%token TK_or "or"
%token TK_option "option"
%token TK_output "output"
%token TK_parameter "parameter"
%token TK_pmos "pmos"
%token TK_posedge "posedge"
%token TK_primitive "primitive"
%token TK_pull0 "pull0"
%token TK_pull1 "pull1"
%token TK_pulldown "pulldown"
%token TK_pullup "pullup"
%token TK_rcmos "rcmos"
%token TK_real "real"
%token TK_realtime "realtime"
%token TK_reg "reg"
%token TK_release "release"
%token TK_repeat "repeat"
%token TK_rnmos "rnmos"
%token TK_rpmos "rpmos"
%token TK_rtran "rtran"
%token TK_rtranif0 "rtranif0"
%token TK_rtranif1 "rtranif1"
%token TK_sample "sample"
%token TK_scalared "scalared"
%token TK_small "small"
%token TK_specify "specify"
%token TK_specparam "specparam"
%token TK_strong0 "strong0"
%token TK_strong1 "strong1"
%token TK_supply0 "supply0"
%token TK_supply1 "supply1"
%token TK_table "table"
%token TK_task "task"
%token TK_time "time"
%token TK_tran "tran"
%token TK_tranif0 "tranif0"
%token TK_tranif1 "tranif1"
%token TK_tri "tri"
%token TK_tri0 "tri0"
%token TK_tri1 "tri1"
%token TK_triand "triand"
%token TK_trior "trior"
%token TK_trireg "trireg"
%token TK_type_option "type_option"
%token TK_vectored "vectored"
%token TK_wait "wait"
%token TK_wand "wand"
%token TK_weak0 "weak0"
%token TK_weak1 "weak1"
%token TK_while "while"
%token TK_wire "wire"
%token TK_wor "wor"
%token TK_xnor "xnor"
%token TK_xor "xor"
%token TK_Shold "$hold"
%token TK_Snochange "$nochange"
%token TK_Speriod "$period"
%token TK_Srecovery "$recovery"
%token TK_Ssetup "$setup"
%token TK_Ssetuphold "$setuphold"
%token TK_Sskew "$skew"
%token TK_Swidth "$width"
/* Icarus specific tokens. */
%token TKK_attribute "$attribute"
%token TK_bool "bool"
/* The new tokens from 1364-2001. */
%token TK_automatic "automatic"
%token TK_endgenerate "endgenerate"
%token TK_generate "generate"
%token TK_genvar "genvar"
%token TK_localparam "localparam"
%token TK_noshowcancelled "noshowcancelled"
%token TK_pulsestyle_onevent "pulsestyle_onevent"
%token TK_pulsestyle_ondetect "pulsestyle_ondetect"
%token TK_showcancelled "showcancelled"
%token TK_signed "signed"
%token TK_unsigned "unsigned"
%token TK_Sfullskew "$fullskew"
%token TK_Srecrem "$recrem"
%token TK_Sremoval "$removal"
%token TK_Stimeskew "$timeskew"
/* The 1364-2001 configuration tokens. */
%token TK_cell "cell"
%token TK_config "config"
%token TK_design "design"
%token TK_endconfig "endconfig"
%token TK_incdir "incdir"
%token TK_include "include"
%token TK_instance "instance"
%token TK_liblist "liblist"
%token TK_library "library"
%token TK_use "use"
/* The new tokens from 1364-2005. */
%token TK_wone "wone"
%token TK_uwire "uwire"
/* The new tokens from 1800-2005. */
%token TK_alias "alias"
%token TK_always_comb "always_comb"
%token TK_always_ff "always_ff"
%token TK_always_latch "always_latch"
%token TK_assert "assert"
%token TK_assume "assume"
%token TK_before "before"
%token TK_bind "bind"
%token TK_bins "bins"
%token TK_binsof "binsof"
%token TK_bit "bit"
%token TK_break "break"
%token TK_byte "byte"
%token TK_chandle "chandle"
%token TK_class "class"
%token TK_clocking "clocking"
%token TK_const "const"
%token TK_constraint "constraint"
%token TK_context "context"
%token TK_continue "continue"
%token TK_cover "cover"
%token TK_covergroup "covergroup"
%token TK_coverpoint "coverpoint"
%token TK_cross "cross"
%token TK_dist "dist"
%token TK_do "do"
%token TK_endclass "endclass"
%token TK_endclocking "endclocking"
%token TK_endgroup "endgroup"
%token TK_endinterface "endinterface"
%token TK_endpackage "endpackage"
%token TK_endprogram "endprogram"
%token TK_endproperty "endproperty"
%token TK_endsequence "endsequence"
%token TK_enum "enum"
%token TK_expect "expect"
%token TK_export "export"
%token TK_extends "extends"
%token TK_extern "extern"
%token TK_final "final"
%token TK_first_match "first_match"
%token TK_foreach "foreach"
%token TK_forkjoin "forkjoin"
%token TK_iff "iff"
%token TK_ignore_bins "ignore_bins"
%token TK_illegal_bins "illegal_bins"
%token TK_import "import"
%token TK_inside "inside"
%token TK_int "int"
%token TK_interface "interface"
%token TK_intersect "intersect"
%token TK_join_any "join_any"
%token TK_join_none "join_none"
%token TK_local "local"
%token TK_local_SCOPE "local::"
%token TK_logic "logic"
%token TK_longint "longint"
%token TK_matches "matches"
%token TK_modport "modport"
%token TK_new "new"
%token TK_null "null"
%token TK_package "package"
%token TK_packed "packed"
%token TK_priority "priority"
%token TK_program "program"
%token TK_property "property"
%token TK_protected "protected"
%token TK_pure "pure"
%token TK_rand "rand"
%token TK_randc "randc"
%token TK_randcase "randcase"
%token TK_randsequence "randsequence"
%token TK_randomize "randomize"
%token TK_ref "ref"
%token TK_return "return"
%token TK_Sroot "$root"
%token TK_sequence "sequence"
%token TK_shortint "shortint"
%token TK_shortreal "shortreal"
%token TK_solve "solve"
%token TK_static "static"
%token TK_string "string"
%token TK_struct "struct"
%token TK_super "super"
%token TK_tagged "tagged"
%token TK_this "this"
%token TK_throughout "throughout"
%token TK_timeprecision "timeprecision"
%token TK_timeunit "timeunit"
%token TK_timescale_unit "(timescale_unit)"
%token TK_type "type"
%token TK_typedef "typedef"
%token TK_union "union"
%token TK_unique "unique"
%token TK_unique_index "unique_index"
%token TK_Sunit "$unit"
%token TK_var "var"
%token TK_virtual "virtual"
%token TK_void "void"
%token TK_wait_order "wait_order"
%token TK_wildcard "wildcard"
%token TK_with "with"
%token TK_with__covergroup "with(covergroup)"
%token TK_within "within"
/* Fake tokens that are passed once we have an initial token. */
%token TK_timeprecision_check "timeprecision_check"
/* The new tokens from 1800-2009. */
%token TK_timeunit_check "timeunit_check"
%token TK_accept_on "accept_on"
%token TK_checker "checker"
%token TK_endchecker "endchecker"
%token TK_eventually "eventually"
%token TK_global "global"
%token TK_implies "implies"
%token TK_let "let"
%token TK_nexttime "nexttime"
%token TK_reject_on "reject_on"
%token TK_restrict "restrict"
%token TK_s_always "s_always"
%token TK_s_eventually "s_eventually"
%token TK_s_nexttime "s_nexttime"
%token TK_s_until "s_until"
%token TK_s_until_with "s_until_with"
%token TK_strong "strong"
%token TK_sync_accept_on "sync_accept_on"
%token TK_sync_reject_on "sync_reject_on"
%token TK_unique0 "unique0"
%token TK_until "until"
%token TK_until_with "until_with"
%token TK_untyped "untyped"
%token TK_weak "weak"
/* The new tokens from 1800-2012. */
%token TK_implements "implements"
%token TK_interconnect "interconnect"
%token TK_nettype "nettype"
%token TK_soft "soft"
/* The new tokens for Verilog-AMS 2.3. */
%token TK_above "above"
%token TK_abs "abs"
%token TK_absdelay "absdelay"
%token TK_abstol "abstol"
%token TK_access "access"
%token TK_acos "acos"
%token TK_acosh "acosh"
%token TK_ac_stim "ac_stim"
%token TK_aliasparam "aliasparam"
%token TK_analog "analog"
%token TK_analysis "analysis"
%token TK_asin "asin"
%token TK_asinh "asinh"
%token TK_atan "atan"
%token TK_atan2 "atan2"
%token TK_atanh "atanh"
%token TK_branch "branch"
%token TK_ceil "ceil"
%token TK_connect "connect"
%token TK_connectmodule "connectmodule"
%token TK_connectrules "connectrules"
%token TK_continuous "continuous"
%token TK_cos "cos"
%token TK_cosh "cosh"
%token TK_ddt "ddt"
%token TK_ddt_nature "ddt_nature"
%token TK_ddx "ddx"
%token TK_discipline "discipline"
%token TK_discrete "discrete"
%token TK_domain "domain"
%token TK_driver_update "driver_update"
%token TK_endconnectrules "endconnectrules"
%token TK_enddiscipline "enddiscipline"
%token TK_endnature "endnature"
%token TK_endparamset "endparamset"
%token TK_exclude "exclude"
%token TK_exp "exp"
%token TK_final_step "final_step"
%token TK_flicker_noise "flicker_noise"
%token TK_floor "floor"
%token TK_flow "flow"
%token TK_from "from"
%token TK_ground "ground"
%token TK_hypot "hypot"
%token TK_idt "idt"
%token TK_idtmod "idtmod"
%token TK_idt_nature "idt_nature"
%token TK_inf "inf"
%token TK_infinite "infinite"  /* `default_decay_time argument */
%token TK_initial_step "initial_step"
%token TK_laplace_nd "laplace_nd"
%token TK_laplace_np "laplace_np"
%token TK_laplace_zd "laplace_zd"
%token TK_laplace_zp "laplace_zp"
%token TK_last_crossing "last_crossing"
%token TK_limexp "limexp"
%token TK_ln "ln"
%token TK_log "log"
%token TK_max "max"
%token TK_merged "merged"
%token TK_min "min"
%token TK_nature "nature"
%token TK_net_resolution "net_resolution"
%token TK_noise_table "noise_table"
%token TK_paramset "paramset"
%token TK_potential "potential"
%token TK_pow "pow"
%token TK_resolveto "resolveto"
%token TK_sin "sin"
%token TK_sinh "sinh"
%token TK_slew "slew"
%token TK_split "split"
%token TK_sqrt "sqrt"
%token TK_tan "tan"
%token TK_tanh "tanh"
%token TK_timer "timer"
%token TK_transition "transition"
%token TK_units "units"
%token TK_white_noise "white_noise"
%token TK_wreal "wreal"
%token TK_zi_nd "zi_nd"
%token TK_zi_np "zi_np"
%token TK_zi_zd "zi_zd"
%token TK_zi_zp "zi_zp"

// built-in methods
%token TK_find "find"
%token TK_find_index "find_index"
%token TK_find_first "find_first"
%token TK_find_first_index "find_first_index"
%token TK_find_last "find_last"
%token TK_find_last_index "find_last_index"

%token TK_sort "sort"
%token TK_rsort "rsort"
%token TK_reverse "reverse"
%token TK_shuffle "shuffle"

%token TK_sum "sum"
%token TK_product "product"
// gen_tokenizer stop

// operator tokens
%token TK_PLUS_EQ "+="
%token TK_MINUS_EQ "-="
%token TK_MUL_EQ "*="
%token TK_DIV_EQ "/="
%token TK_MOD_EQ "%="

%token TK_AND_EQ "&="
%token TK_OR_EQ "|="
%token TK_XOR_EQ "^="

%token TK_INCR "++"
%token TK_DECR "--"
// TODO(b/63595640): Disambiguate between use as less-equal,
//   nonblocking_assignment, and clocking_drive.
%token TK_LE "<="
%token TK_GE ">="
%token TK_EG "=>"
%token TK_EQ "=="
%token TK_WILDCARD_EQ "==?"
%token TK_NE "!="
%token TK_WILDCARD_NE "!=?"
%token TK_CEQ "==="
%token TK_CNE "!=="
%token TK_LP "'{"
%token TK_LS "<<"
// or "<<<"
%token TK_RS ">>"
%token TK_RSS ">>>"
%token TK_SG "*>"
%token TK_CONTRIBUTE "<+"
%token TK_PO_POS "+:"
%token TK_PO_NEG "-:"
%token TK_POW "**"
%token TK_PSTAR "(*"
%token TK_STARP "*)"
%token TK_DOTSTAR ".*"
%token TK_LOR "||"
%token TK_LAND "&&"
%token TK_TAND "&&&"
%token TK_NAND "~&"
%token TK_NOR "~|"
%token TK_NXOR "~^"
// or "^~"
%token TK_LOGEQUIV "<->"

%token TK_NONBLOCKING_TRIGGER "->>"
%token _TK_RARROW "->"
// _TK_RARROW is disambiguated into one of the following symbols
// (see verilog_lexical_context.cc):
%token TK_TRIGGER "->(trigger)"
%token TK_LOGICAL_IMPLIES "->(logical-implies)"
%token TK_CONSTRAINT_IMPLIES "->(constraint-implies)"

%token TK_SCOPE_RES "::"
%token TK_COLON_EQ ":="
%token TK_COLON_DIV ":/"
%token TK_POUNDPOUND "##"
%token TK_edge_descriptor
%token TK_LBSTARRB "[*]"
%token TK_LBPLUSRB "[+]"
%token TK_LBSTAR "[*"
%token TK_LBEQ "[="
%token TK_LBRARROW "[->"
%token TK_PIPEARROW "|->"
%token TK_PIPEARROW2 "|=>"
%token TK_POUNDMINUSPOUND "#-#"
%token TK_POUNDEQPOUND "#=#"
%token TK_ATAT "@@"

%token MacroArg
%token MacroCallCloseToEndLine

/* These token types exist for the lexer, but are not used in this parser. */
// The &lowast; escaping is needed here because Bison 3.5.91+ puts the string
// literal in a block comment in the generated parser, and the "*/" in the
// literal causes the block comment to end prematurely and causes a syntax
// error. Note that this literal does not affect the grammar in any way.
%token TK_COMMENT_BLOCK "/&lowast;comment&lowast;/"
%token TK_EOL_COMMENT "// end of line comment"
%token TK_SPACE "<<space>>"  /* includes tabs */
%token TK_NEWLINE "<<\\n>>"
%token TK_LINE_CONT "<<\\line-cont>>"
%token TK_ATTRIBUTE "(*attribute*)"

/* most likely a lexical error */
%token TK_OTHER
// LINT.ThenChange(../formatting/verilog_token.cc)

/* A glorified ';' specialized to mark the end of an
   assertion_variable_declaration list inside the
   body of a property_declaration (which ends with a property_spec)
   or a sequence_declaration (which ends with a sequence_expr).
   This re-enumeration is set by a contextualizing pass between the
   lexer and parser.
 */
%token SemicolonEndOfAssertionVariableDeclarations ";(after-assertion-variable-decls)"

// right-associative modify-assignment operators
%right TK_PLUS_EQ
%right TK_MINUS_EQ
%right TK_MUL_EQ
%right TK_DIV_EQ
%right TK_MOD_EQ
%right TK_AND_EQ
%right TK_OR_EQ
%right TK_XOR_EQ
%right TK_LS_EQ
%right TK_RS_EQ
%right TK_RSS_EQ
%right '?'
%right ':'
%right TK_inside
%right TK_LOGICAL_IMPLIES
%right TK_LOGEQUIV
/* left-associative operators */
%left TK_LOR
%left TK_LAND
%left '|'
%left '^' TK_NXOR TK_NOR
%left '&' TK_NAND
%left TK_EQ TK_NE TK_CEQ TK_CNE
%left TK_WILDCARD_EQ TK_WILDCARD_NE
%left TK_GE TK_LE '<' '>'
%left TK_LS TK_RS TK_RSS
%left '+' '-'
%left '*' '/' '%'
%left TK_POW
/**
%left UNARY_PREC
**/

/* to resolve dangling else ambiguity. */
/* alternatively, rewrite using matched/unmatched-if */
%nonassoc less_than_TK_else
%nonassoc TK_else
/* to resolve exclude (... ambiguity */
%nonassoc '('
%nonassoc TK_exclude

/* root of syntax tree: */
%start source_text

%%

source_text
  : description_list
    { param->SetRoot(move($1)); }
  | /* empty */
    { param->SetRoot(MakeNode()); }
  ;
GenericIdentifier
  // TODO(fangism): re-tag these to look like GenericIdentifier
  : SymbolIdentifier
    { $$ = move($1); }
  | EscapedIdentifier
    { $$ = move($1); }
  | MacroIdentifier
    { $$ = move($1); }
  | KeywordIdentifier
    { $$ = move($1); }
  /* Treat built-in function identifiers as plain identifiers. */
  | unary_builtin_function
    { $$ = move($1); }
  | binary_builtin_function
    { $$ = move($1); }
  ;

KeywordIdentifier
/* The following are keywords in certain dialects of Verilog.
 * For now, allow these to be used as identifiers until we actually find
 * examples that use the dialects, and will require additional context.
 */
  /* Verilog-AMS: */
  : TK_branch
    { $$ = move($1); }
  | TK_access
    { $$ = move($1); }
  | TK_exclude
    { $$ = move($1); }
  | TK_flow
    { $$ = move($1); }
  | TK_from
    { $$ = move($1); }
  | TK_ground
    { $$ = move($1); }
  | TK_connect
    { $$ = move($1); }
  /* Verilog-A derivative operators */
  | TK_ddx
    { $$ = move($1); }
  | TK_ddt
    { $$ = move($1); }
  | TK_idt
    { $$ = move($1); }
  | TK_idtmod
    { $$ = move($1); }
  /* Verilog-AMS connect keywords: */
  | TK_split
    { $$ = move($1); }
  | TK_merged
    { $$ = move($1); }
  /* Verilog-AMS event statements: */
  | TK_timer
    { $$ = move($1); }
  /* | TK_cross is used in SystemVerilog covergroups */
  | TK_above
    { $$ = move($1); }
  | TK_discrete
    { $$ = move($1); }
  | TK_initial_step
    { $$ = move($1); }
  | TK_final_step
    { $$ = move($1); }
  /* TK_sample is in SystemVerilog coverage_event */
  | TK_sample
    { $$ = move($1); }
  /* TODO(fangism): 'bool' is not a SystemVerilog keyword, but we may
   *   still want to discourage it (style-guide).  */
  | TK_bool
    { $$ = move($1); }
  ;

/* TODO(fangism): phase this out:
 *   Separate into preprocessor_balanced_* and preprocessor_action.
 *   Require uses of preprocess control flow to be balanced.
 */
preprocessor_directive
  : preprocessor_control_flow
    { $$ = move($1); }
  | preprocessor_action
    { $$ = move($1); }
  ;

preprocessor_if_header
  : PP_ifdef PP_Identifier
    { $$ = MakeTaggedNode(N::kPreprocessorIfdefClause, $1, $2); }
    /* consumer of $$ is expected to ExtendNode */
  | PP_ifndef PP_Identifier
    { $$ = MakeTaggedNode(N::kPreprocessorIfndefClause, $1, $2); }
    /* consumer of $$ is expected to ExtendNode */
  /* | PP_if expression '\n' */  /* doesn't exist for Verilog */
  ;

preprocessor_elsif_header
  : PP_elsif PP_Identifier
    { $$ = MakeTaggedNode(N::kPreprocessorElsifClause, $1, $2); }
    /* consumer of $$ is expected to ExtendNode */
    /* `elsif is interpreted as else-if-macro-is-defined,
     * not the traditional expression predicate that follows else-if.
     */
  ;

/* TODO(fangism): Phase this out.
 *   Require uses of preprocess control flow to be balanced.
 */
preprocessor_control_flow
  : preprocessor_if_header
    { $$ = move($1); }
  | preprocessor_elsif_header
    { $$ = move($1); }
  | PP_else
    { $$ = move($1); }
  | PP_endif
    { $$ = move($1); }
  ;

preprocessor_action
  : PP_undef PP_Identifier
    { $$ = MakeTaggedNode(N::kPreprocessorUndef, $1, $2); }
  | PP_include preprocess_include_argument
    { $$ = MakeTaggedNode(N::kPreprocessorInclude, $1, $2); }
  /* The body of a `define macro can be any unstructured sequence of tokens,
   * which this parser just accumulates in an un-lexer manner.
   * Verilog preprocessing even supports `defines in the bodies of `defines!
   */
  | PP_define PP_Identifier PP_define_body
    { $$ = MakeTaggedNode(N::kPreprocessorDefine, $1, $2, $3); }
    /* $3 is unlexed text, and may even be empty. */
  | PP_define PP_Identifier '(' macro_formals_list_opt ')' PP_define_body
    { $$ = MakeTaggedNode(N::kPreprocessorDefine, $1, $2, MakeParenGroup($3, $4, $5), $6); }
    /* $6 is unlexed text, and may even be empty. */
  ;

macro_formals_list_opt
  : macro_formals_list
    { $$ = move($1); }
  | /* empty */
    { $$.reset(); }
  ;
macro_formals_list
  : macro_formals_list ',' macro_formal_parameter
    { $$ = ExtendNode($1, $2, $3); }
  | macro_formal_parameter
    { $$ = MakeTaggedNode(N::kMacroFormalParameterList, $1); }
  ;
macro_formal_parameter
  : PP_Identifier
    { $$ = MakeTaggedNode(N::kMacroFormalArg, $1); }
  | PP_Identifier '=' PP_default_text
    { $$ = MakeTaggedNode(N::kMacroFormalArg, $1, $2, $3); }
    /* $3 is unlexed text */
  ;

preprocess_include_argument
  : string_literal
    { $$ = move($1); }
  | MacroIdentifier
    { $$ = move($1); }
  | MacroGenericItem
    { $$ = move($1); }
  | MacroCall
    { $$ = move($1); }
  ;

MacroGenericItem
  : MacroCallItem
    { $$ = move($1); }
  | MacroIdItem
    { $$ = MakeTaggedNode(N::kMacroGenericItem, $1); }
  ;
MacroCallItem
  /* suitable for use as list items */
  : MacroCallId '(' macro_args_opt MacroCallCloseToEndLine
    { $$ = MakeTaggedNode(N::kMacroCall, $1, MakeParenGroup($2, $3, $4)); }
  ;
MacroCall
  /* suitable for use in expressions */
  : MacroCallId '(' macro_args_opt ')'
    { $$ = MakeTaggedNode(N::kMacroCall, $1, MakeParenGroup($2, $3, $4)); }
  ;
macro_args_opt
  : macro_args_opt ',' macro_arg_opt
    { $$ = ExtendNode($1, $2, $3); }
  | macro_arg_opt
    { $$ = MakeTaggedNode(N::kMacroArgList, $1);}
  ;
macro_arg_opt
  : MacroArg
    /* un-lexed text */
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;

procedural_assertion_statement
  : concurrent_assertion_statement
    { $$ = move($1); }
  | immediate_assertion_statement
    { $$ = move($1); }
  /* TODO(fangism):
  | checker_instantiation
  */
  ;

assertion_item
  : concurrent_assertion_item
    { $$ = move($1); }
  | deferred_immediate_assertion_item
    { $$ = move($1); }
  ;
assignment_pattern
  : TK_LP expression_list_proper '}'
    { $$ = MakeTaggedNode(N::kAssignmentPattern, $1, $2, $3); }
  | TK_LP structure_or_array_pattern_expression_list '}'
    { $$ = MakeTaggedNode(N::kAssignmentPattern, $1, $2, $3); }
  | TK_LP expression '{' expression_list_proper '}' '}'
    { $$ = MakeTaggedNode(N::kAssignmentPattern, $1, $2, MakeBraceGroup($3, $4, $5), $6); }
    /* replication construct: $2 must be a constant expression */
  | TK_LP '}'
    { $$ = MakeTaggedNode(N::kAssignmentPattern, $1, $2); }
  ;

structure_or_array_pattern_expression_list
  : structure_or_array_pattern_expression_list ',' structure_or_array_pattern_expression
    { $$ = ExtendNode($1, $2, $3); }
  | structure_or_array_pattern_expression
    { $$ = MakeNode($1); }
  ;
structure_or_array_pattern_expression
  : structure_or_array_pattern_key ':' expression
    { $$ = MakeTaggedNode(N::kPatternExpression, $1, $2, $3); }
  ;
structure_or_array_pattern_key
  /* structure_pattern_key : member_identifier | assignment_pattern_key
   * array_pattern_key : constant_expression | assignment_pattern_key
   * assignment_pattern_key : simple_type | TK_default
   */
  : expression
    { $$ = move($1); }
    /* $1 should be a constant expression or GenericIdentifier (member). */
  | simple_type
    { $$ = move($1); }
  | TK_default
    { $$ = move($1); }
  ;
simple_type
  : integer_atom_type
    { $$ = move($1); }
  | integer_vector_type
    { $$ = move($1); }
  /* TODO(fangism): support package scope or class scope type here:
  | qualified_id
  */
  ;

block_identifier_opt
  : unqualified_id ':'
    { $$ = MakeTaggedNode(N::kBlockIdentifier, $1, $2); }
    /* $1 should be a GenericIdentifier, no parameter_value */
  | /* empty */
    { $$ = nullptr; }
  ;

interface_class_declaration
  : TK_interface TK_class GenericIdentifier
    module_parameter_port_list_opt
    declaration_extends_list_opt ';'  /* multiple base interfaces allowed */
    interface_class_item_list_opt
    TK_endclass label_opt
    { $$ = MakeTaggedNode(N::kInterfaceClassDeclaration, $1, $2, $3, $4, $5, $6, $7, $8, $9); }
  ;
declaration_extends_list_opt
  : declaration_extends_list
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
declaration_extends_list
  : TK_extends class_id
    { $$ = MakeTaggedNode(N::kDeclarationExtendsList, $1, $2); }
  | declaration_extends_list ',' class_id
    { $$ = ExtendNode($1, $2, $3); }
  ;
implements_interface_list_opt
  : implements_interface_list
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
implements_interface_list
  : implements_interface_list ',' class_id
    { $$ = ExtendNode($1, $2, $3); }
  | TK_implements class_id
    { $$ = MakeTaggedNode(N::kImplementsList, $1, $2); }
  ;

interface_class_item_list_opt
  : interface_class_item_list
    { $$ = move($1); }
  | /* empty */
  ;
interface_class_item_list
  : interface_class_item_list interface_class_item
    { $$ = ExtendNode($1, $2); }
  | interface_class_item
    { $$ = MakeTaggedNode(N::kInterfaceClassItemList, $1); }
  ;
interface_class_item
  : type_declaration
    { $$ = move($1); }
  | any_param_declaration
    { $$ = move($1); }
  | interface_class_method
    { $$ = move($1); }
  | ';'
    { $$ = move($1); }
  ;
interface_class_method
  : TK_pure TK_virtual method_prototype ';'
    { $$ = MakeTaggedNode(N::kInterfaceClassMethod, $1, $2, $3, $4); }
  ;

method_prototype
  : task_prototype
    { $$ = move($1); }
  | function_prototype
    { $$ = move($1); }
  ;

class_declaration
  : TK_virtual_opt TK_class lifetime_opt GenericIdentifier
    module_parameter_port_list_opt
    class_declaration_extends_opt /* only one base type allowed */
    implements_interface_list_opt ';'
    class_items_opt TK_endclass
    label_opt
    { $$ = MakeTaggedNode(N::kClassDeclaration,
                          MakeTaggedNode(N::kClassHeader,
                                         $1, $2, $3, $4, $5, $6, $7, $8),
                          $9, $10, $11); }
  ;
class_constraint
  : constraint_prototype
    { $$ = move($1); }
  | constraint_declaration
    { $$ = move($1); }
  ;
class_declaration_extends_opt
  : TK_extends class_id
    { $$ = MakeTaggedNode(N::kExtendsList, $1, $2); }
  | /* empty */
    { $$ = nullptr; }
  ;

unqualified_id
  : GenericIdentifier parameter_value_opt
    { $$ = MakeTaggedNode(N::kUnqualifiedId, $1, $2); }
  /* If the root identifier is a package, it should not have parameters. */
  /* TODO(fangism): parameter_value_opt is too permissive here,
     should only allow parenthesized parameter lists following '#'. */
  /* Normally, a type reference does not declare a new symbol, however, this rule
   * is used in an overloaded way as an implicitly typed declaration in:
   *   type_identifier_or_implicit_basic_followed_by_id_and_dimensions_opt
   *   type_identifier_or_implicit_basic_followed_by_id
   */
  ;
qualified_id
  /* This also serves as a scoped class member identifier, which is useful for
   * out-of-line method (function/task) definitions.
   * For the purposes of emitting code blocks, we take only the final
   * identifier in the hierarchy (here, $3).
   * Once we migrate to a full AST, we can resolve the complete class scopes of
   * hierarchical identifiers.
   */
  : qualified_id TK_SCOPE_RES unqualified_id
    { $$ = ExtendNode($1, $2, $3); }
  | unqualified_id TK_SCOPE_RES unqualified_id
    { $$ = MakeTaggedNode(N::kQualifiedId, $1, $2, $3); }
  | TK_Sunit TK_SCOPE_RES unqualified_id
    /* $unit refers to a package scope, but can never appear alone. */
    { $$ = MakeTaggedNode(N::kQualifiedId, $1, $2, $3); }
  | qualified_id TK_SCOPE_RES TK_new
    { $$ = ExtendNode($1, $2, $3); }
  | unqualified_id TK_SCOPE_RES TK_new
    { $$ = MakeTaggedNode(N::kQualifiedId, $1, $2, $3); }
    /* Allow class_name::new to refer to out-of-line constructor. */
  ;
class_id
  : qualified_id
    { $$ = move($1); }
  | unqualified_id
    { $$ = move($1); }
  ;
class_items_opt
  : class_items
    { $$ = move($1); }
  | /* empty */
    { $$ = MakeTaggedNode(N::kClassItems);}
  ;
class_items
  : class_items class_item
    { $$ = ExtendNode($1, $2); }
  | class_item
    { $$ = MakeTaggedNode(N::kClassItems, $1);}
  ;

class_constructor_prototype
  : TK_function TK_new tf_port_list_paren_opt
    { $$ = MakeTaggedNode(N::kClassConstructorPrototype, $1, $2, $3); }
  /* users of this rule may append a trailing ';' to this node
   * TODO(fangism): move the ';' into this rule
   */
  ;

class_constructor
  : class_constructor_prototype ';'
    /* merged: function_item_list_opt statement_or_null_list_opt */
    tf_item_or_statement_or_null_list_opt
    TK_endfunction endnew_opt
    { $$ = MakeTaggedNode(N::kClassConstructor, qualifier_placeholder,
                          ExtendNode($1, $2), $3, $4, $5); }
    /* TODO(fangism) Probably want to include the qualifier_placeholder
     * in the prototype/header as well.  Reshape this.  */
  ;

  /* TODO(fangism): should some of these function declarations be METHODs?
   * lexer will need to keep track of in_class (level)
   * and function_declaration will need to use it.
   **/
class_item
  /* The keyword 'virtual' is overloaded both as a method qualifier and as
   * a keyword to start an interface data type.
   * To avoid conflict, we expand method_qualifier_list_opt and property_qualifier_opt
   * by the first item, to discern those that start with 'virtual' and those that do not.
   */
  /* originally: method_qualifier_list_opt class_constructor */
  : method_property_qualifier_list_not_starting_with_virtual class_constructor
    { SetChild($2, 0, $1);
      $$ = move($2); }
  | class_constructor
    { $$ = move($1); }
  | TK_virtual method_qualifier_list_opt class_constructor
    { SetChild($3, 0, MakeTaggedNode(N::kQualifierList, $1, ForwardChildren($2)));
      $$ = move($3); }

  /* originally: property_qualifier_list_opt data_type list_of_variable_decl_assignments ';' */
  /* or: property_qualifier_list_opt data_declaration */
  | method_property_qualifier_list_not_starting_with_virtual
    const_opt var_opt data_type list_of_variable_decl_assignments ';'
    { $$ = MakeDataDeclaration(
                          ExtendNode($1, $2, $3),
                          MakeInstantiationBase(
                              MakeTaggedNode(N::kInstantiationType, $4),
                              $5),
                          $6); }
  | data_type list_of_variable_decl_assignments ';'
    { $$ = MakeDataDeclaration(
                          qualifier_placeholder,
                          MakeInstantiationBase(
                              MakeTaggedNode(N::kInstantiationType, $1),
                              $2),
                          $3); }
  | TK_const class_item_qualifier_list_opt data_type list_of_variable_decl_assignments ';'
    { $$ = MakeDataDeclaration(
                          MakeTaggedNode(N::kQualifierList, $1, ForwardChildren($2)),
                          MakeInstantiationBase(
                              MakeTaggedNode(N::kInstantiationType, $3),
                              $4),
                          $5); }
  | interface_data_declaration
    { $$ = move($1); }

  | package_import_declaration
    { $$ = move($1); }
    /* In the LRM, package_import_declaration is covered by data_declaration. */

  /* originally: method_qualifier_list_opt task_or_function_declaration */
  /* should qualifier be attached to function/task declaration? */
  | method_property_qualifier_list_not_starting_with_virtual task_declaration
    { SetChild(SymbolCastToNode(*$2)[0] /* kTaskHeader */, 0, $1);
      $$ = move($2); }
  | task_declaration
    { $$ = move($1); }
  | TK_virtual method_qualifier_list_opt task_declaration
    { SetChild(SymbolCastToNode(*$3)[0] /* kTaskHeader */, 0,
          MakeTaggedNode(N::kQualifierList, $1, ForwardChildren($2)));
      $$ =  move($3); }
  /* TODO(fangism): Method qualifiers should be grouped together into one list,
   * rather than being split between virtual and method_qualifier list.
   */
  | method_property_qualifier_list_not_starting_with_virtual function_declaration
    { SetChild(SymbolCastToNode(*$2)[0] /* kFunctionHeader */, 0, $1);
      $$ = move($2); }
  | function_declaration
    { $$ = move($1); }
  | TK_virtual method_qualifier_list_opt function_declaration
    { SetChild(SymbolCastToNode(*$3)[0] /* kFunctionHeader */, 0,
          MakeTaggedNode(N::kQualifierList, $1, ForwardChildren($2)));
      $$ =  move($3); }
  /* pure virtual method prototypes: */
  | TK_pure TK_virtual class_item_qualifier_list_opt method_prototype ';'
   { $$ = MakeTaggedNode(N::kForwardDeclaration,
                        MakeTaggedNode(N::kQualifierList, $1, $2, ForwardChildren($3)),
                        ExtendNode($4, $5)); }
  /* forward declarations (excludes definition body): */
  | TK_extern method_qualifier_list_opt method_prototype ';'
     { $$ = MakeTaggedNode(N::kForwardDeclaration,
                        MakeTaggedNode(N::kQualifierList, $1, ForwardChildren($2)),
                        ExtendNode($3, $4)); }
  | TK_extern method_qualifier_list_opt class_constructor_prototype ';'
     { $$ = MakeTaggedNode(N::kForwardDeclaration,
                        MakeTaggedNode(N::kQualifierList, $1, ForwardChildren($2)),
                        ExtendNode($3, $4)); }
  | class_declaration
    { $$ = move($1); }
  | interface_class_declaration
    { $$ = move($1); }
  | class_constraint
    { $$ = move($1); }
  | type_declaration
    { $$ = move($1); }
  | any_param_declaration
    { $$ = move($1); }
  | covergroup_declaration
    { $$ = move($1); }
  | ';'
    { $$ = MakeTaggedNode(N::kNullDeclaration, $1); }
  | error ';'
    { yyerrok; $$ = Recover(); }
  | macro_call_or_item
    { $$ = move($1); }
  | preprocessor_balanced_class_items
    { $$ = move($1); }
  | preprocessor_action
    { $$ = move($1); }
  ;

interface_data_declaration
  : interface_type list_of_variable_decl_assignments ';'
    { $$ = MakeDataDeclaration(
               qualifier_placeholder,
               MakeInstantiationBase(
                   MakeTaggedNode(N::kInstantiationType, $1),
                   $2),
               $3); }
    /* interface instantiation: virtual type_if inst_if */
  ;

preprocessor_balanced_class_items
  : preprocessor_if_header class_items_opt
    preprocessor_elsif_class_items_opt
    preprocessor_else_class_item_opt
    PP_endif
    { $$ = MakeTaggedNode(N::kPreprocessorBalancedClassItems,
                          ExtendNode($1, $2), ForwardChildren($3), $4, $5);
    }
  ;
preprocessor_elsif_class_items_opt
  : preprocessor_elsif_class_items
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
preprocessor_elsif_class_items
  : preprocessor_elsif_class_items preprocessor_elsif_class_item
    { $$ = ExtendNode($1, $2); }
  | preprocessor_elsif_class_item
    { $$ = MakeNode($1); }  /* Don't bother tagging; node will be flattened. */
  ;
preprocessor_elsif_class_item
  : preprocessor_elsif_header class_items_opt
    { $$ = ExtendNode($1, $2); }
  ;
preprocessor_else_class_item_opt
  : preprocessor_else_class_item
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
preprocessor_else_class_item
  : PP_else class_items_opt
    { $$ = MakeTaggedNode(N::kPreprocessorElseClause, $1, $2); }
  ;

macro_call_or_item
  : MacroGenericItem
    { $$ = move($1); }
    /* lone macro on its own line */
  | MacroCall ';'
    { $$ = ExtendNode($1, $2); }
  ;

class_item_qualifier
  : TK_static
    { $$ = move($1); }
  | TK_protected
    { $$ = move($1); }
  | TK_local
    { $$ = move($1); }
  ;
class_item_qualifier_list_opt
  : class_item_qualifier_list
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
class_item_qualifier_list
  : class_item_qualifier_list class_item_qualifier
    { $$ = ExtendNode($1, $2); }
  | class_item_qualifier
    { $$ = MakeTaggedNode(N::kQualifierList, $1); }
  ;
class_new
  : TK_new '(' argument_list_opt ')'
    { $$ = MakeTaggedNode(N::kClassNew, $1, MakeParenGroup($2, $3, $4)); }
  | TK_new reference
    { $$ = MakeTaggedNode(N::kClassNew, $1, $2); }
    /* The LRM actually permits any expression at $2. */
  | TK_new
    { $$ = MakeTaggedNode(N::kClassNew, $1); }
  /* TODO(fangism):
  | class_id TK_SCOPE_RES TK_new '(' argument_list_opt ')'
   */
  ;

/* action_block is a very strange nonterminal in SystemVerilog:
 * it consists of the statement body of the assert/assume clause,
 * followed by an optional else-clause.
 * To make this more consistent with conditional constructs,
 * this node will be dismantled by the consumer and reshaped,
 * so that an assert-clause will resemble an if-clause, etc.
 */
action_block
  : statement_or_null
    %prec less_than_TK_else
    { $$ = MakeTaggedNode(N::kActionBlock, $1, nullptr); }
  | statement_or_null TK_else statement_or_null
    { $$ = MakeTaggedNode(N::kActionBlock, $1,
                          MakeTaggedNode(N::kElseClause, $2, MakeTaggedNode(N::kElseBody, $3))); }

    /* original grammar rule:
     *   statement TK_else statement_or_null
     * but relaxed to look like the unmatched-if case so the %prec directive
     * can resolve the S/R conflict.
     */
  | TK_else statement_or_null
    { $$ = MakeTaggedNode(N::kActionBlock, nullptr,
                          MakeTaggedNode(N::kElseClause, $1, MakeTaggedNode(N::kElseBody, $2))); }
  ;
concurrent_assertion_item
  : block_identifier_opt concurrent_assertion_statement
    { $$ = MakeTaggedNode(N::kAssertionItem, $1, $2); }
  /* TODO(fangism):
   | checker_instantiation
   */
  ;
concurrent_assertion_statement
  /* all end with ';' or '}' */
  : assert_property_statement
    { $$ = move($1); }
  | assume_property_statement
    { $$ = move($1); }
  | cover_property_statement
    { $$ = move($1); }
  | cover_sequence_statement
    { $$ = move($1); }
  | restrict_property_statement
    { $$ = move($1); }
  ;
assert_property_statement
  : TK_assert TK_property '(' property_spec ')' action_block
    { auto& node = SymbolCastToNode(*$6);
      $$ = MakeTaggedNode(
               N::kAssertPropertyStatement,
               MakeTaggedNode(  /* like an if-clause */
                   N::kAssertPropertyClause,
                   MakeTaggedNode(  /* like an if-header */
                       N::kAssertPropertyHeader,
                       $1, $2, MakeParenGroup($3, $4, $5)),
                   MakeTaggedNode(N::kAssertPropertyBody, node[0])),
               node[1] /* else-clause */);
    }
  ;
assume_property_statement
  : TK_assume TK_property '(' property_spec ')' action_block
    { auto& node = SymbolCastToNode(*$6);
      $$ = MakeTaggedNode(
               N::kAssumePropertyStatement,
               MakeTaggedNode(  /* like an if-clause */
                   N::kAssumePropertyClause,
                   MakeTaggedNode(  /* like an if-header */
                       N::kAssumePropertyHeader,
                       $1, $2, MakeParenGroup($3, $4, $5)),
                   MakeTaggedNode(N::kAssumePropertyBody, node[0])),
               node[1] /* else-clause */);
    }
  ;
cover_property_statement
  : TK_cover TK_property '(' property_spec ')' statement_or_null
    /* shaped like kIfClause */
    { $$ = MakeTaggedNode(N::kCoverPropertyStatement,
                          MakeTaggedNode(N::kCoverPropertyHeader,
                                         $1, $2, MakeParenGroup($3, $4, $5)),
                          MakeTaggedNode(N::kCoverPropertyBody, $6)); }
  ;
expect_property_statement
  : TK_expect '(' property_spec ')' action_block
    { auto& node = SymbolCastToNode(*$5);
      $$ = MakeTaggedNode(
               N::kExpectPropertyStatement,
               MakeTaggedNode(  /* like an if-clause */
                   N::kExpectPropertyClause,
                   MakeTaggedNode(  /* like an if-header */
                       N::kExpectPropertyHeader,
                       $1, MakeParenGroup($2, $3, $4)),
                   MakeTaggedNode(N::kExpectPropertyBody, node[0])),
               node[1] /* else-clause */);
    }
  ;
cover_sequence_statement
  : TK_cover TK_sequence '(' sequence_spec ')' statement_or_null
    /* shaped like kIfClause */
    { $$ = MakeTaggedNode(N::kCoverSequenceStatement,
                          MakeTaggedNode(N::kCoverSequenceHeader,
                                         $1, $2, MakeParenGroup($3, $4, $5)),
                          MakeTaggedNode(N::kCoverSequenceBody, $6)); }

  ;
restrict_property_statement
  : TK_restrict TK_property '(' property_spec ')' ';'
    { $$ = MakeTaggedNode(N::kRestrictPropertyStatement,
                          $1, $2, MakeParenGroup($3, $4, $5), $6); }
  ;

deferred_immediate_assertion_item
  : block_identifier_opt deferred_immediate_assertion_statement
    { $$ = MakeTaggedNode(N::kAssertionItem, $1, $2); }
  ;
immediate_assertion_statement
  : simple_immediate_assertion_statement
    { $$ = move($1); }
  | deferred_immediate_assertion_statement
    { $$ = move($1); }
  ;
simple_immediate_assertion_statement
  : TK_assert '(' expression ')' action_block
  /* shaped similarly to kConditionalStatement */
    { auto& node = SymbolCastToNode(*$5);
      $$ = MakeTaggedNode(
               N::kAssertionStatement,
               MakeTaggedNode(  /* like an if-clause */
                   N::kAssertionClause,
                   MakeTaggedNode(  /* like an if-header */
                       N::kAssertionHeader,
                       $1, nullptr, MakeParenGroup($2, $3, $4)),
                   MakeTaggedNode(N::kAssertionBody, node[0])),
               node[1] /* else-clause */);
    }
  | TK_assume '(' expression ')' action_block
  /* shaped similarly to kConditionalStatement */
    { auto& node = SymbolCastToNode(*$5);
      $$ = MakeTaggedNode(
               N::kAssumeStatement,
               MakeTaggedNode(  /* like an if-clause */
                   N::kAssumeClause,
                   MakeTaggedNode(  /* like an if-header */
                       N::kAssumeHeader,
                       $1, nullptr, MakeParenGroup($2, $3, $4)),
                   MakeTaggedNode(N::kAssumeBody, node[0])),
               node[1] /* else-clause */);
    }
  | TK_cover '(' expression ')' statement_or_null
  /* shaped similarly to kIfClause, doesn't have an else-clause */
    { $$ = MakeTaggedNode(N::kCoverStatement,
                          MakeTaggedNode(N::kCoverHeader,
                                         $1, nullptr,
                                         MakeParenGroup($2, $3, $4)),
                          MakeTaggedNode(N::kCoverBody, $5)); }
  ;
deferred_immediate_assertion_statement
  : TK_assert final_or_zero '(' expression ')' action_block
  /* shaped similarly to kConditionalStatement */
    { auto& node = SymbolCastToNode(*$6);
      $$ = MakeTaggedNode(
               N::kAssertionStatement,
               MakeTaggedNode(  /* like an if-clause */
                   N::kAssertionClause,
                   MakeTaggedNode(  /* like an if-header */
                       N::kAssertionHeader,
                       $1, $2, MakeParenGroup($3, $4, $5)),
                   MakeTaggedNode(N::kAssertionBody, node[0])),
               node[1] /* else-clause */);
    }
  | TK_assume final_or_zero '(' expression ')' action_block
  /* shaped similarly to kConditionalStatement */
    { auto& node = SymbolCastToNode(*$6);
      $$ = MakeTaggedNode(
               N::kAssumeStatement,
               MakeTaggedNode(  /* like an if-clause */
                   N::kAssumeClause,
                   MakeTaggedNode(  /* like an if-header */
                       N::kAssumeHeader,
                       $1, $2, MakeParenGroup($3, $4, $5)),
                   MakeTaggedNode(N::kAssumeBody, node[0])),
               node[1] /* else-clause */);
    }
  | TK_cover final_or_zero '(' expression ')' statement_or_null
  /* shaped similarly to kIfClause, doesn't have an else-clause */
    { $$ = MakeTaggedNode(N::kCoverStatement,
                          MakeTaggedNode(N::kCoverHeader,
                                         $1, $2,
                                         MakeParenGroup($3, $4, $5)),
                          MakeTaggedNode(N::kCoverBody, $6)); }
  ;
final_or_zero
  : TK_final
    { $$ = move($1); }
  | '#' TK_DecNumber
    { $$ = MakeTaggedNode(N::kPoundZero, $1, $2); }
    /* $2 must be 0 */
  ;

constraint_block
  : '{' constraint_block_item_list_opt '}'
    { $$ = MakeBraceGroup($1, $2, $3); }
  ;
constraint_block_item
  : constraint_expression_no_preprocessor
    { $$ = move($1); }
  /* The TK_solve rule has been moved into constraint_expression
   * to support an extension.
   */
  | preprocessor_balanced_constraint_block_item
    /* This also covers preprocessor_balanced_constraint_expressions. */
    { $$ = move($1); }
  ;
constraint_primary_list
  : constraint_primary_list ',' constraint_primary
    { $$ = ExtendNode($1, $2, $3); }
  | constraint_primary
    { $$ = MakeTaggedNode(N::kConstraintPrimaryList, $1); }
  ;
constraint_block_item_list
  : constraint_block_item_list constraint_block_item
    { $$ = ExtendNode($1, $2); }
  | constraint_block_item
    { $$ = MakeTaggedNode(N::kConstraintBlockItemList, $1); }
  ;
constraint_block_item_list_opt
  : constraint_block_item_list
    { $$ = move($1); }
  | /* empty */
    /* create empty list */
    { $$ = MakeTaggedNode(N::kConstraintBlockItemList); }
  ;
preprocessor_balanced_constraint_block_item
  : preprocessor_if_header constraint_block_item_list_opt
    preprocessor_elsif_constraint_block_items_opt
    preprocessor_else_constraint_block_item_opt
    PP_endif
    { $$ = MakeTaggedNode(N::kPreprocessorBalancedConstraintBlockItem,
                          ExtendNode($1, $2), ForwardChildren($3), $4, $5);
    }
  ;
preprocessor_elsif_constraint_block_items_opt
  : preprocessor_elsif_constraint_block_items
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
preprocessor_elsif_constraint_block_items
  : preprocessor_elsif_constraint_block_items preprocessor_elsif_constraint_block_item
    { $$ = ExtendNode($1, $2); }
  | preprocessor_elsif_constraint_block_item
    { $$ = MakeNode($1); }  /* Don't bother tagging; node will be flattened. */
  ;
preprocessor_elsif_constraint_block_item
  : preprocessor_elsif_header constraint_block_item_list_opt
    { $$ = ExtendNode($1, $2); }
  ;
preprocessor_else_constraint_block_item_opt
  : preprocessor_else_constraint_block_item
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
preprocessor_else_constraint_block_item
  : PP_else constraint_block_item_list_opt
    { $$ = MakeTaggedNode(N::kPreprocessorElseClause, $1, $2); }
  ;

constraint_declaration_package_item
  /* Like constraint_declaration, but excludes qualifiers
   * and allows the declared identifier to be scope-qualified.
   */
  : TK_constraint class_id constraint_block
    { $$ = MakeTaggedNode(N::kConstraintDeclaration, nullptr, $1, $2, $3); }

    /* $2 is allowed to be scope-qualified for out-of-line definitions */
  ;
constraint_declaration
  : TK_static_opt TK_constraint GenericIdentifier constraint_block
    { $$ = MakeTaggedNode(N::kConstraintDeclaration, $1, $2, $3, $4); }
  ;
constraint_expression_no_preprocessor
  /* Ends with ';' or '}' */
  : TK_soft expression_or_dist ';'
    { $$ = MakeTaggedNode(N::kConstraintExpression, $1, $2, $3);}
  | expression_or_dist ';'
    { $$ = MakeTaggedNode(N::kConstraintExpression, $1, $2);}
  | expression TK_CONSTRAINT_IMPLIES constraint_set
    { $$ = MakeTaggedNode(N::kConstraintExpression, $1, $2, $3);}
  | TK_if '(' expression ')' constraint_set
    %prec less_than_TK_else
    { $$ = MakeTaggedNode(N::kConstraintExpression, $1, MakeParenGroup($2, $3, $4), $5);}
  | TK_if '(' expression ')' constraint_set TK_else constraint_set
    { $$ = MakeTaggedNode(N::kConstraintExpression, $1, MakeParenGroup($2, $3, $4), $5, $6, $7);}
  | TK_foreach '(' reference ')' constraint_set
    { $$ = MakeTaggedNode(N::kConstraintExpression, $1, MakeParenGroup($2, $3, $4), $5);}
    /* TODO(fangism): $3 must end with the form: '[' loop_variables ']'
     * where loop_variables is a list of loop variable identifiers.
     * See note in variable_dimension nonterminal.
     */
  | uniqueness_constraint ';'
    { $$ = MakeTaggedNode(N::kConstraintExpression, $1, $2);}
  | TK_disable TK_soft constraint_primary ';'
    { $$ = MakeTaggedNode(N::kConstraintExpression, $1, $2, $3, $4);}
  | TK_solve constraint_primary_list TK_before constraint_primary_list ';'
    { $$ = MakeTaggedNode(N::kConstraintExpression, $1, $2, $3, $4, $5);}
    /* solve within as a constraint_set items is an extension to the LRM
     * to allow solve statements inside foreach.
     */
  ;
constraint_expression
  /* Factored out this rule because constraint_block_item covers
     constraint_expression, so excluding preprocessor-balancing from
     constraint_expression will avoid R/R conflicts.
   */
  : constraint_expression_no_preprocessor
    { $$ = move($1); }
  | preprocessor_balanced_constraint_expressions
    { $$ = move($1); }
  ;
constraint_primary
  : reference
    { $$ = move($1); }
    /* covers:
     * reference '[' part_select_range ']'
     *   where part_select_range
     *     : constant_range
     *     | indexed_range
     *     ;
     *   both of which are covered in variable_dimension.
     */
  ;
uniqueness_constraint
  : TK_unique '{' open_range_list '}'
    { $$ = MakeTaggedNode(N::kUniquenessConstraint, $1, MakeBraceGroup($2, $3, $4)); }
  ;
constraint_expression_list
  : constraint_expression_list constraint_expression
    { $$ = ExtendNode($1, $2); }
  | constraint_expression
    { $$ = MakeTaggedNode(N::kConstraintExpressionList, $1); }
  ;
constraint_expression_list_opt
  : constraint_expression_list
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;

preprocessor_balanced_constraint_expressions
  : preprocessor_if_header constraint_expression_list_opt
    preprocessor_elsif_constraint_expressions_opt
    preprocessor_else_constraint_expression_opt
    PP_endif
    { $$ = MakeTaggedNode(N::kPreprocessorBalancedConstraintExpressions,
                          ExtendNode($1, $2), ForwardChildren($3), $4, $5);
    }
  ;
preprocessor_elsif_constraint_expressions_opt
  : preprocessor_elsif_constraint_expressions
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
preprocessor_elsif_constraint_expressions
  : preprocessor_elsif_constraint_expressions preprocessor_elsif_constraint_expression
    { $$ = ExtendNode($1, $2); }
  | preprocessor_elsif_constraint_expression
    { $$ = MakeNode($1); }
  ;
preprocessor_elsif_constraint_expression
  : preprocessor_elsif_header constraint_expression_list_opt
    { $$ = ExtendNode($1, $2); }
  ;
preprocessor_else_constraint_expression_opt
  : preprocessor_else_constraint_expression
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
preprocessor_else_constraint_expression
  : PP_else constraint_expression_list_opt
    { $$ = MakeTaggedNode(N::kPreprocessorElseClause, $1, $2); }
  ;

constraint_prototype
  : TK_static_opt TK_constraint GenericIdentifier ';'
    { $$ = MakeTaggedNode(N::kConstraintPrototype, $1, $2, $3, $4); }
  ;
constraint_set
  : constraint_expression
    { $$ = move($1); }
  | '{' constraint_expression_list '}'
    /* TODO(fangism): $2 should be optional, but empty {} is covered by
       expr_primary_braces in constraint_expression, and S/R conflicts. */
    { $$ = MakeBraceGroup($1, $2, $3); }
  ;
const_opt
  : TK_const
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
var_opt
  : TK_var
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;

data_declaration_base
  /* very similar to struct_union_member and block_item_decl */
  : data_type_or_implicit_basic_followed_by_id_and_dimensions_opt
    trailing_decl_assignment_opt ',' list_of_variable_decl_assignments ';'
    { /* re-shape subtree to pass onto MakeDataDeclaration */
      auto& node = SymbolCastToNode(*$1);
      $$ = MakeNode(
               MakeInstantiationBase(
                    MakeTaggedNode(N::kInstantiationType, node[0]),  /* data type */
            /* declaration assignment list is similar to instantiation list */
                    MakeTaggedNode(N::kVariableDeclarationAssignmentList,
                                   MakeTaggedNode(
                                       N::kVariableDeclarationAssignment,
                                       node[1],  /* id */
                                       node[2],  /* unpacked dimensions */
                                       $2),
                                   $3,  /* ',' */
                                   ForwardChildren($4))),
                $5);
    }
  | data_type_or_implicit_basic_followed_by_id_and_dimensions_opt
    trailing_decl_assignment_opt ';'
    { /* re-shape subtree to pass onto MakeDataDeclaration */
      auto& node = SymbolCastToNode(*$1);
      $$ = MakeNode(
               MakeInstantiationBase(
                    MakeTaggedNode(N::kInstantiationType, node[0]),  /* data type */
            /* declaration assignment list is similar to instantiation list */
                    MakeTaggedNode(N::kVariableDeclarationAssignmentList,
                                   MakeTaggedNode(
                                       N::kVariableDeclarationAssignment,
                                       node[1],  /* id */
                                       node[2],  /* unpacked dimensions */
                                       $2))),
               $3);
    }
  ;
data_declaration_modifiers_opt
  : const_opt var_opt lifetime_opt
    { $$ = MakeTaggedNode(N::kQualifierList, $1, $2, $3); }
  ;
data_declaration
  : data_declaration_modifiers_opt data_declaration_base
    { auto& node = SymbolCastToNode(*$2);
      $$ = MakeDataDeclaration($1, node[0], node[1]);
    }
  /* In the LRM, data_declaration also includes:
   *   type_declaration
   *   package_import_declaration
   *   net_type_declaration
   * but we choose to keep those separate in this grammar.
   */
  ;

data_type_primitive
  : data_type_primitive_scalar decl_dimensions_opt
    /* $2 is packed dimensions */
    { $$ = ExtendNode($1, MakePackedDimensionsNode($2)); }
  ;
data_type_primitive_scalar
  : integer_vector_type signed_unsigned_opt
    { $$ = MakeTaggedNode(N::kDataTypePrimitive, $1, $2); }
  | non_integer_type
    { $$ = MakeTaggedNode(N::kDataTypePrimitive, $1); }
  | struct_data_type
    { $$ = MakeTaggedNode(N::kDataTypePrimitive, $1); }
  | enum_data_type
    { $$ = MakeTaggedNode(N::kDataTypePrimitive, $1); }
  | integer_atom_type signed_unsigned_opt
    { $$ = MakeTaggedNode(N::kDataTypePrimitive, $1, $2); }
  | TK_chandle
    { $$ = MakeTaggedNode(N::kDataTypePrimitive, $1); }
  | TK_string
    { $$ = MakeTaggedNode(N::kDataTypePrimitive, $1); }
  | TK_event
    { $$ = MakeTaggedNode(N::kDataTypePrimitive, $1); }
  ;

  /* trailing optional decl_dimensions moved to rule: data_type */
  /* resolves conflict on: ID . '[' (sized-type or index expression?) */
data_type_base
  : data_type_primitive
    { $$ = move($1); }
  /* hierarchy_identifier starting w/ GenericIdentifier
   * is source of major conflict, so we factor the rest out.
   **/
  | reference_or_call
    { $$ = move($1); }
    /* covers class_id : qualified_id | unqualified_id
     * can already contain type parameters (parameter_value_opt)
     * TODO(fangism): calls are not permitted in type reference, but it was
     *   written permissively to avoid R/R conflicts against expressions.
     */
  /* merged into: reference
  | GenericIdentifier decl_dimensions_opt
  | scope_prefix GenericIdentifier
  | scope_prefix_opt GenericIdentifier
  | class_id
  */
  /* interface_type is also considered a 'data_type' in the language,
   * but it is handled specially in class_item, to avoid a conflict on
   * the 'virtual' keyword.
   */
  | type_reference
    { $$ = move($1); }
  ;

type_reference
  : TK_type '(' expression ')'
    { $$ = MakeTaggedNode(N::kTypeReference, $1, MakeParenGroup($2, $3, $4)); }
  /* TODO(fangism): some data types are not covered
   | TK_type '(' data_type ')'
   */
  ;

 /* pulled decl_dimensions_opt outside of data_type to other rules */
data_type
  : data_type_base /* decl_dimensions_opt */
    { $$ = move($1); }
  ;

interface_type
  : TK_virtual interface_opt GenericIdentifier parameter_value_opt
    { $$ = MakeTaggedNode(N::kInterfaceType, $1, $2, $3, $4); }
    /* $3 is the interface_identifier */
  | TK_virtual interface_opt GenericIdentifier parameter_value_opt
    '.' member_name
    { $$ = MakeTaggedNode(N::kInterfaceType, $1, $2, $3, $4, $5, $6); }

    /* $5 is the modport_identifier (optional) */
  ;
interface_opt
  : TK_interface
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;

delay3_or_drive_opt
  : delay3
    { $$ = move($1); }
  | drive_strength
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;

/* support interface.modport as a type */
scope_or_if_res
  : TK_SCOPE_RES
    { $$ = move($1); }
  | '.'
    { $$ = move($1); }
  ;

/* Re-worked grammar to eliminate conflict on type identifier,
 * which is a consequence of implicit types in declarations.
 * Unable to parse: "ID . ID" without further lookahead past second ID,
 * so rules were rewritten to *group* the declared ID along with
 * its preceding (possibly implicit) type.
 * The *last* identifier is the one being declared, so return it in $$.
 * For outline generation, don't care about type, so it is dropped.
 * Delay and drive specifiers occur in net_declarations.
 */
type_identifier_or_implicit_followed_by_id_and_dimensions_opt
  // : class_id GenericIdentifier { $$ = move($2); }
  // : GenericIdentifier delay3_or_drive_opt GenericIdentifier { $$ = move($3); }
  : GenericIdentifier delay3 decl_dimensions_opt
    GenericIdentifier decl_dimensions_opt
    { $$ = MakeTaggedNode(N::kDataTypeImplicitIdDimensions,
                          MakeTaggedNode(N::kDataType, $1, $2,
                                         MakePackedDimensionsNode($3)),
                          $4, MakeUnpackedDimensionsNode($5)); }
  | GenericIdentifier drive_strength decl_dimensions_opt
    GenericIdentifier decl_dimensions_opt
    { $$ = MakeTaggedNode(N::kDataTypeImplicitIdDimensions,
                          MakeTaggedNode(N::kDataType, $1, $2,
                                         MakePackedDimensionsNode($3)),
                          $4, MakeUnpackedDimensionsNode($5)); }
  | GenericIdentifier decl_dimensions_opt
    GenericIdentifier decl_dimensions_opt
    { $$ = MakeTaggedNode(N::kDataTypeImplicitIdDimensions,
                          MakeTaggedNode(N::kDataType, $1, nullptr,
                                         MakePackedDimensionsNode($2)),
                          $3, MakeUnpackedDimensionsNode($4)); }
  | GenericIdentifier scope_or_if_res GenericIdentifier
    delay3_or_drive_opt decl_dimensions_opt
    GenericIdentifier decl_dimensions_opt
    /* TODO(fangism): separate scope_or_if_res into different node tags cases,
     * one for TK_SCOPE (qualified_id), one for '.' (interface port).
     */
    { $$ = MakeTaggedNode(N::kDataTypeImplicitIdDimensions,
                          MakeTaggedNode(N::kDataType,
                                         MakeTaggedNode(N::kInterfacePortHeader,
                                                        $1, $2, $3),
                                         $4,
                                         MakePackedDimensionsNode($5)),
                          $6, MakeUnpackedDimensionsNode($7)); }
  // | delay3_or_drive_opt GenericIdentifier { $$ = move($2); }
  | /* implicit type */ /* decl_dimensions_opt */
    GenericIdentifier decl_dimensions_opt
    { $$ = MakeTaggedNode(N::kDataTypeImplicitIdDimensions,
                          MakeTaggedNode(N::kDataType, nullptr, nullptr,
                                         MakeTaggedNode(N::kPackedDimensions,
                                                        nullptr)),
                          $1, MakeUnpackedDimensionsNode($2)); }
  | /* implicit type */ delay3 decl_dimensions_opt
    GenericIdentifier decl_dimensions_opt
    { $$ = MakeTaggedNode(N::kDataTypeImplicitIdDimensions,
                          MakeTaggedNode(N::kDataType, nullptr, $1,
                                         MakePackedDimensionsNode($2)),
                          $3, MakeUnpackedDimensionsNode($4)); }
  | /* implicit type */ drive_strength decl_dimensions_opt
    GenericIdentifier decl_dimensions_opt
    { $$ = MakeTaggedNode(N::kDataTypeImplicitIdDimensions,
                          MakeTaggedNode(N::kDataType, nullptr, $1,
                                         MakePackedDimensionsNode($2)),
                          $3, MakeUnpackedDimensionsNode($4)); }
  ;

/* with optional decl_dimensions before and after */
type_identifier_followed_by_id
  : unqualified_id decl_dimensions_opt GenericIdentifier
    { $$ = MakeTaggedNode(N::kTypeIdentifierId,
                          MakeTaggedNode(N::kDataType, $1,
                                         MakePackedDimensionsNode($2)),
                          $3); }
    /* $1 is type */
  | qualified_id decl_dimensions_opt GenericIdentifier
    { $$ = MakeTaggedNode(N::kTypeIdentifierId,
                          MakeTaggedNode(N::kDataType, $1,
                                         MakePackedDimensionsNode($2)),
                          $3); }
  /* The following are 'interface_port_header' from the LRM: */
  | unqualified_id '.' member_name decl_dimensions_opt GenericIdentifier
    { $$ = MakeTaggedNode(N::kTypeIdentifierId,
                          MakeTaggedNode(N::kDataType,
                                         MakeTaggedNode(N::kInterfacePortHeader,
                                                        $1, $2, $3),
                                         MakePackedDimensionsNode($4)),
                          $5); }
    /* $1..$3 is interface modport */
  | TK_interface '.' member_name GenericIdentifier
    { $$ = MakeTaggedNode(N::kTypeIdentifierId,
                          MakeTaggedNode(N::kDataType,
                                         MakeTaggedNode(N::kInterfacePortHeader,
                                                        $1, $2, $3), nullptr),
                          $4); }
  | TK_interface GenericIdentifier
    { $$ = MakeTaggedNode(N::kTypeIdentifierId,
                          MakeTaggedNode(N::kDataType,
                                         MakeTaggedNode(N::kInterfacePortHeader,
                                                        $1), nullptr),
                          $2); }
  ;


/* Return type and declaration name: */
/* no delay3 or drive_strength, for data_declaration */
type_identifier_or_implicit_basic_followed_by_id
  // TODO(jeremycs): standardize this family of rules
  : unqualified_id GenericIdentifier
    { $$ = MakeTaggedNode(N::kDataTypeImplicitBasicId,
                          MakeTaggedNode(N::kDataType, $1), $2); }
    /* $1 is type */
  | qualified_id GenericIdentifier
    { $$ = MakeTaggedNode(N::kDataTypeImplicitBasicId,
                          MakeTaggedNode(N::kDataType, $1), $2); }
    /* $1 is type */
  | /* implicit type */ unqualified_id
    { $$ = MakeTaggedNode(N::kDataTypeImplicitBasicId,
                          MakeTaggedNode(N::kDataType, nullptr), $1); }
  /* This rule really wants to be (implicit type):
   *   GenericIdentifier
   * but to resolve a conflict due to insufficient lookahead, it is "upgraded"
   * to a more inclusive nonterminal.
   * TODO(fangism): verify that this is GenericIdentifier without parameters.
   */
  /* The following are 'interface_port_header' from the LRM: */
  | unqualified_id '.' member_name GenericIdentifier
    { $$ = MakeTaggedNode(N::kDataTypeImplicitBasicId,
                          MakeTaggedNode(N::kDataType,
                                         MakeTaggedNode(N::kInterfacePortHeader,
                                                        $1, $2, $3)),
                          $4); }
    /* $1..$3 is interface.modport */
  | TK_interface '.' member_name GenericIdentifier
    { $$ = MakeTaggedNode(N::kDataTypeImplicitBasicId,
                          MakeTaggedNode(N::kDataType,
                                         MakeTaggedNode(N::kInterfacePortHeader,
                                                        $1, $2, $3)),
                          $4); }
  | TK_interface GenericIdentifier
    { $$ = MakeTaggedNode(N::kDataTypeImplicitBasicId,
                          MakeTaggedNode(N::kDataType,
                                         MakeTaggedNode(N::kInterfacePortHeader,
                                                        $1)),
                          $2); }
  ;

/* data_type or class_id followed by declared name, optional array size
 * The declared name can be scope-qualified for out-of-line definitions.
 * Here, the last 'class_id' covers scope-qualified names.
 */
type_identifier_or_implicit_basic_followed_by_id_and_dimensions_opt
  : qualified_id decl_dimensions_opt
    class_id decl_dimensions_opt
    { $$ = MakeTypeIdDimensionsTuple(
                          MakeTaggedNode(N::kDataType, $1,
                                         MakePackedDimensionsNode($2)),
                          $3, MakeUnpackedDimensionsNode($4)); }
    /* $1 is type */
  | unqualified_id decl_dimensions_opt
    class_id decl_dimensions_opt
    { $$ = MakeTypeIdDimensionsTuple(
                          MakeTaggedNode(N::kDataType, $1,
                                         MakePackedDimensionsNode($2)),
                          $3, MakeUnpackedDimensionsNode($4)); }
  | unqualified_id '.' member_name decl_dimensions_opt
    class_id decl_dimensions_opt
    /* This looks like reference_or_call */
    { $$ = MakeTypeIdDimensionsTuple(
                          MakeTaggedNode(N::kDataType,
                                         MakeTaggedNode(N::kInterfacePortHeader,
                                                        $1, $2, $3),
                                         MakePackedDimensionsNode($4)),
                          $5, MakeUnpackedDimensionsNode($6)); }
    /* $1..$3 is interface.modport */
  | /* implicit type */ unqualified_id decl_dimensions_opt
    { $$ = MakeTypeIdDimensionsTuple(
                          MakeTaggedNode(N::kDataType, nullptr, nullptr),
                          $1, MakeUnpackedDimensionsNode($2)); }
  | /* implicit type */ qualified_id decl_dimensions_opt
    { $$ = MakeTypeIdDimensionsTuple(
                          MakeTaggedNode(N::kDataType, nullptr, nullptr),
                          $1, MakeUnpackedDimensionsNode($2)); }
  /* This rule really wants to be (implicit type):
   *   GenericIdentifier decl_dimensions_opt
   * However, since (GenericIdentifier decl_dimensions_opt) is
   * ambiguously an implicit-typed declaration and a return type, there is
   * insufficient symbol lookahead to resolve the difference.
   * Thus, GenericIdentifier is "upgraded" to a construct that captures
   * GenericIdentifier without conflict, here, unqualified_id.
   * This permits invalid constructs, so syntax should be enforced in a
   * separate pass.
   */
  ;

data_type_or_implicit
  : decl_dimensions delay3_or_drive_opt
    { $$ = MakeTaggedNode(N::kDataTypeImplicitIdDimensions,
                          MakeTaggedNode(N::kDataType, nullptr,
                                         MakePackedDimensionsNode($1)),
                          $2, nullptr, nullptr); }
  | signing decl_dimensions_opt delay3_or_drive_opt
    { $$ = MakeTaggedNode(N::kDataTypeImplicitIdDimensions,
                          MakeTaggedNode(N::kDataType, $1,
                                         MakePackedDimensionsNode($2)),
                          $3, nullptr, nullptr); }
  | GenericIdentifier decl_dimensions_opt delay3_or_drive_opt
    { $$ = MakeTaggedNode(N::kDataTypeImplicitIdDimensions,
                          MakeTaggedNode(N::kDataType, $1,
                                         MakePackedDimensionsNode($2)),
                          $3, nullptr, nullptr); }
  | GenericIdentifier TK_SCOPE_RES GenericIdentifier decl_dimensions_opt delay3_or_drive_opt
    { $$ = MakeTaggedNode(N::kDataTypeImplicitIdDimensions,
                          MakeTaggedNode(
                              N::kDataType,
                              MakeTaggedNode(N::kQualifiedId, $1, $2, $3),
                              MakePackedDimensionsNode($4)),
                          $5, nullptr, nullptr); }
  /* want to use just 'class_id' to cover all qualified and unqualified types,
   * including parameterized types, but encounter grammar conflicts
   */
  ;

/* For declaring net_type or function_declaration return type, followed by declared name */
data_type_or_implicit_followed_by_id_and_dimensions_opt
  : data_type_primitive GenericIdentifier decl_dimensions_opt
    { $$ = MakeTaggedNode(N::kDataTypeImplicitIdDimensions,
                          MakeTaggedNode(N::kDataType, $1, nullptr),
                          nullptr /* delay3_or_drive_opt */,
                          $2, MakeUnpackedDimensionsNode($3)); }
    /* $1 is type, including optional packed dimensions */
  /* allows optional delay3 or drive_strength: */
  | type_identifier_or_implicit_followed_by_id_and_dimensions_opt
    { $$ = move($1); }
  | signing decl_dimensions_opt delay3_or_drive_opt
    GenericIdentifier decl_dimensions_opt
    { $$ = MakeTaggedNode(N::kDataTypeImplicitIdDimensions,
                          MakeTaggedNode(N::kDataType, $1,
                                         MakePackedDimensionsNode($2)),
                          $3, $4,
                          MakeUnpackedDimensionsNode($5)); }
  | decl_dimensions delay3_or_drive_opt GenericIdentifier decl_dimensions_opt
    { $$ = MakeTaggedNode(N::kDataTypeImplicitIdDimensions,
                          MakeTaggedNode(N::kDataType, nullptr,
                                         MakePackedDimensionsNode($1)),
                          $2, $3,
                          MakeUnpackedDimensionsNode($4)); }
  | TK_void GenericIdentifier decl_dimensions_opt
    { $$ = MakeTaggedNode(N::kDataTypeImplicitIdDimensions,
                          MakeTaggedNode(N::kDataType, $1, nullptr),
                          nullptr /* delay3_or_drive_opt */, $2,
                          MakeUnpackedDimensionsNode($3)); }
  ;

data_type_or_implicit_basic_followed_by_id
  // TODO(jeremycs): standardize this family of rules
  : data_type_primitive GenericIdentifier
    { $$ = MakeTaggedNode(N::kDataTypeImplicitBasicId,
                          MakeTaggedNode(N::kDataType, $1), $2); }
  /* forbids optional delay3 or drive_strength: */
  | type_identifier_or_implicit_basic_followed_by_id
    { $$ = move($1); }
  | signing decl_dimensions_opt GenericIdentifier
    { $$ = MakeTaggedNode(N::kDataTypeImplicitBasicId,
                          MakeTaggedNode(N::kDataType, $1,
                                         MakePackedDimensionsNode($2)),
                          $3); }
  | decl_dimensions GenericIdentifier
    { $$ = MakeTaggedNode(N::kDataTypeImplicitBasicId,
                          MakeTaggedNode(N::kDataType, nullptr,
                                         MakePackedDimensionsNode($1)),
                          $2); }
  | TK_void GenericIdentifier
    { $$ = MakeTaggedNode(N::kDataTypeImplicitBasicId,
                          MakeTaggedNode(N::kDataType, $1), $2); }
  ;

/**
 * For general port declarations: (tf_port_item, port_declaration_noattr)
 * This rule was introduced as a combination of:
 *    TYPE DIM ID DIM
 *    TYPE DIM ID
 *    TYPE ID
 *    DIM ID DIM
 *    DIM ID
 *    ID
 * where TYPE can be an ID.
 *
 * ID can now be a scope-qualified name (here, class_name),
 * to support out-of-line definitions.
 **/
data_type_or_implicit_basic_followed_by_id_and_dimensions_opt
  : data_type_primitive class_id decl_dimensions_opt
    { $$ = MakeTypeIdDimensionsTuple(
                          MakeTaggedNode(N::kDataType, $1,
                                         nullptr /* packed dimensions */),
                          $2, MakeUnpackedDimensionsNode($3)); }
  /* forbids optional delay3 or drive_strength: */
  | type_identifier_or_implicit_basic_followed_by_id_and_dimensions_opt
    { $$ = move($1); }
  | signing decl_dimensions_opt class_id decl_dimensions_opt
    { $$ = MakeTypeIdDimensionsTuple(
                          MakeTaggedNode(N::kDataType, $1,
                                         MakePackedDimensionsNode($2)),
                          $3, MakeUnpackedDimensionsNode($4)); }
  | decl_dimensions class_id decl_dimensions_opt
    { $$ = MakeTypeIdDimensionsTuple(
                          MakeTaggedNode(N::kDataType, nullptr,
                                         MakePackedDimensionsNode($1)),
                          $2, MakeUnpackedDimensionsNode($3)); }
  | TK_void class_id decl_dimensions_opt
    { $$ = MakeTypeIdDimensionsTuple(
                          MakeTaggedNode(N::kDataType, $1, nullptr),
                          $2, MakeUnpackedDimensionsNode($3)); }
  ;

description
  : module_or_interface_declaration
    { $$ = move($1); }
  | udp_primitive
    { $$ = move($1); }
  | config_declaration
    { $$ = move($1); }
  | nature_declaration
    { $$ = move($1); }
  | package_declaration
    { $$ = move($1); }
  | discipline_declaration
    { $$ = move($1); }
  | package_item_no_pp
    { $$ = move($1); }
  | TKK_attribute '(' GenericIdentifier ','
    TK_StringLiteral ',' TK_StringLiteral ')'
    { $$ = MakeTaggedNode(N::kAttribute, $1,
                          MakeParenGroup($2, MakeNode($3, $4, $5, $6, $7),
                                         $8)); }
  | bind_directive
    { $$ = move($1); }
  | preprocessor_balanced_description_items
    { $$ = move($1); }
  | preprocessor_action
    { $$ = move($1); }
  ;
description_list_opt
  : description_list
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
description_list
  : description
    { $$ = MakeTaggedNode(N::kDescriptionList, $1); }
  | description_list description
    { $$ = ExtendNode($1, $2); }
  ;
preprocessor_balanced_description_items
  : preprocessor_if_header description_list_opt
    preprocessor_elsif_description_items_opt
    preprocessor_else_description_item_opt
    PP_endif
    { $$ = MakeTaggedNode(N::kPreprocessorBalancedDescriptionItems,
                          ExtendNode($1, $2), ForwardChildren($3), $4, $5);
    }
  ;
preprocessor_elsif_description_items_opt
  : preprocessor_elsif_description_items
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
preprocessor_elsif_description_items
  : preprocessor_elsif_description_items preprocessor_elsif_description_item
    { $$ = ExtendNode($1, $2); }
  | preprocessor_elsif_description_item
    { $$ = MakeNode($1); }  /* Don't bother tagging; node will be flattened. */
  ;
preprocessor_elsif_description_item
  : preprocessor_elsif_header description_list_opt
    { $$ = ExtendNode($1, $2); }
  ;
preprocessor_else_description_item_opt
  : preprocessor_else_description_item
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
preprocessor_else_description_item
  : PP_else description_list_opt
    { $$ = MakeTaggedNode(N::kPreprocessorElseClause, $1, $2); }
  ;
endnew_opt
  : ':' TK_new
    { $$ = MakeTaggedNode(N::kEndNew, $1, $2); }
  | /* empty */
    { $$ = nullptr; }
  ;
dynamic_array_new
  : TK_new '[' expression ']'
    { $$ = MakeTaggedNode(N::kDynamicArrayNew, $1,
                          MakeBracketGroup($2, $3, $4), nullptr); }
  | TK_new '[' expression ']' '(' expression ')'
    { $$ = MakeTaggedNode(N::kDynamicArrayNew, $1,
                          MakeBracketGroup($2, $3, $4),
                          MakeParenGroup($5, $6, $7)); }

  ;
for_step_opt
  : for_step
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
for_step
  : for_step ',' assignment_statement
    { $$ = ExtendNode($1, $2, $3); }
  | assignment_statement
    { $$ = MakeTaggedNode(N::kForStepList, $1); }
  ;
/* LRM: this is named for_step_assignment */
assignment_statement
  : assignment_statement_no_expr
    { $$ = move($1); }
  | inc_or_dec_expression
    { $$ = move($1); }
  /* TODO(b/150645241): function_subroutine_call */
  ;
assignment_statement_no_expr
  : lpvalue '=' expression
    { $$ = MakeTaggedNode(N::kNetVariableAssignment, $1, $2, $3); }
  | assign_modify_statement
    { $$ = move($1); }
  ;

function_prototype
  /* TODO(fangism): can this be structured like FunctionHeader? */
  /* users of this rule may append a trailing ';' */
  : TK_function lifetime_opt
    /* data_type_or_implicit_or_void GenericIdentifier */
    data_type_or_implicit_basic_followed_by_id_and_dimensions_opt
    tf_port_list_paren_opt
    { $$ = MakeTaggedNode(N::kFunctionPrototype,
                          qualifier_placeholder, $1, $2, $3, $4); }
    /* Without port list, is suitable for export declarations. */
  ;

function_return_type_and_id
  : data_type_or_implicit_basic_followed_by_id_and_dimensions_opt
  /* $1 should not have unpacked dimensions */
    { $$ = RepackReturnTypeId(move($1)); }
  | interface_type class_id
    { $$ = RepackReturnTypeId(MakeTypeIdDimensionsTuple(
               MakeTaggedNode(N::kDataType, $1, nullptr), $2, nullptr)); }
  ;

function_declaration
  /* This covers both in-line declarations and out-of-line class method
   * declarations.
   */
  : TK_function lifetime_opt
    function_return_type_and_id '(' tf_port_list_opt ')' ';'
    /* block_item_decls_opt statement_or_null_list_opt */
    block_item_or_statement_or_null_list_opt
    TK_endfunction endfunction_label_opt
    { $$ = MakeFunctionDeclaration(qualifier_placeholder, $1, $2,
                                   ForwardChildren($3),  // expand type id pair
                                   MakeParenGroup($4, $5, $6),
                                   $7, nullptr, $8, $9, $10); }
  | TK_function lifetime_opt
    function_return_type_and_id ';'
    function_item_list
    statement_or_null_list_opt
    TK_endfunction endfunction_label_opt
    { $$ = MakeFunctionDeclaration(qualifier_placeholder, $1, $2,
                                   ForwardChildren($3),  // expand type id pair
                                   nullptr, $4, $5, $6, $7, $8); }
  | TK_function lifetime_opt
    function_return_type_and_id ';'
    /* empty function_item_list */
    statement_or_null_list_opt
    TK_endfunction endfunction_label_opt
    { $$ = MakeFunctionDeclaration(qualifier_placeholder, $1, $2,
                                   ForwardChildren($3),  // expand type id pair
                                   nullptr, $4, nullptr, $5, $6, $7); }
  ;

endfunction_label_opt
  : label_opt
    { $$ = $1 ? MakeTaggedNode(N::kFunctionEndlabel, ForwardChildren($1))
              : nullptr; }
  | ':' TK_new
    /* for constructors */
    { $$ = MakeTaggedNode(N::kFunctionEndlabel, $1, $2); }
  ;
implicit_class_handle
  : TK_this
    { $$ = move($1); }
  | TK_super
    { $$ = move($1); }
  ;

/* TODO(jeremycs): Fill this out */
inc_or_dec_expression
  : TK_INCR lpvalue /* %prec UNARY_PREC */
    { $$ = MakeTaggedNode(N::kIncrementDecrementExpression, $1, $2); }
  | lpvalue TK_INCR /* %prec UNARY_PREC */
    { $$ = MakeTaggedNode(N::kIncrementDecrementExpression, $1, $2); }
  | TK_DECR lpvalue /* %prec UNARY_PREC */
    { $$ = MakeTaggedNode(N::kIncrementDecrementExpression, $1, $2); }
  | lpvalue TK_DECR /* %prec UNARY_PREC */
    { $$ = MakeTaggedNode(N::kIncrementDecrementExpression, $1, $2); }
  ;
integer_atom_type
  : TK_byte
    { $$ = move($1); }
  | TK_shortint
    { $$ = move($1); }
  | TK_int
    { $$ = move($1); }
  | TK_longint
    { $$ = move($1); }
  | TK_integer
    { $$ = move($1); }
  | TK_time
    { $$ = move($1); }
  ;
integer_vector_type
  : TK_reg
    { $$ = move($1); }
  | TK_bit
    { $$ = move($1); }
  | TK_logic
    { $$ = move($1); }
  ;
join_keyword
  : TK_join
    { $$ = move($1); }
  | TK_join_none
    { $$ = move($1); }
  | TK_join_any
    { $$ = move($1); }
  ;
jump_statement
  : TK_break ';'
    { $$ = MakeTaggedNode(N::kJumpStatement, $1, $2); }
  | TK_continue ';'
    { $$ = MakeTaggedNode(N::kJumpStatement, $1, $2); }
  | TK_return ';'
    { $$ = MakeTaggedNode(N::kJumpStatement, $1, nullptr, $2); }
  | TK_return expression ';'
    { $$ = MakeTaggedNode(N::kJumpStatement, $1, $2, $3); }
  ;
loop_statement
  : TK_for '(' for_initialization_opt ';' expression_opt ';' for_step_opt ')'
    statement_or_null
      { $$ = MakeTaggedNode(N::kForLoopStatement,
          MakeTaggedNode(N::kLoopHeader, $1,
              MakeParenGroup($2,
                  MakeTaggedNode(N::kForSpec,
                      $3, $4,
                      MakeTaggedNode(N::kForCondition, $5),
                      $6, $7),
              $8)),
          $9); }
  | TK_forever statement_or_null
    { $$ = MakeTaggedNode(N::kForeverLoopStatement, $1, $2); }
  | repeat_control statement_or_null
    { $$ = MakeTaggedNode(N::kRepeatLoopStatement, $1, $2); }
  | TK_while '(' expression ')' statement_or_null
    { $$ = MakeTaggedNode(N::kWhileLoopStatement, $1, $2, $3, $4, $5); }
  | TK_do statement_or_null TK_while '(' expression ')' ';'
    { $$ = MakeTaggedNode(N::kDoWhileLoopStatement, $1, $2, $3, $4, $5, $6, $7); }
  | TK_foreach '(' reference ')' statement_or_null
    { $$ = MakeTaggedNode(N::kForeachLoopStatement, $1, $2, $3, $4, $5); }
    /* TODO(fangism): $3 must end with the form: '[' loop_variables ']' .
     * where loop_variables is a list of loop variable identifiers.
     * See note in variable_dimension nonterminal.
     */
  ;
for_initialization_opt
  : for_initialization
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
for_initialization
  : for_initialization ',' for_init_decl_or_assign
    { $$ = ExtendNode($1, $2, $3); }
  | for_init_decl_or_assign
    { $$ = MakeTaggedNode(N::kForInitializationList, $1); }
  ;
for_init_decl_or_assign
  : lpvalue '=' expression
    { $$ = MakeTaggedNode(N::kForInitialization, nullptr, nullptr, $1, $2, $3); }
  | data_type GenericIdentifier '=' expression
    { $$ = MakeTaggedNode(N::kForInitialization, nullptr, $1, $2, $3, $4); }
  | TK_var data_type GenericIdentifier '=' expression
    { $$ = MakeTaggedNode(N::kForInitialization, $1, $2, $3, $4, $5); }
  ;

/* TODO(fangism): collect list of fields/variables to report. */
list_of_variable_decl_assignments
  : variable_decl_assignment
    { $$ = MakeTaggedNode(N::kVariableDeclarationAssignmentList, $1); }
  | list_of_variable_decl_assignments ',' variable_decl_assignment
    { $$ = ExtendNode($1, $2, $3); }
  ;
variable_decl_assignment
  /* similar to gate_instance_or_register_variable */
  : GenericIdentifier decl_dimensions_opt trailing_decl_assignment_opt
    { $$ = MakeTaggedNode(N::kVariableDeclarationAssignment, $1,
                          MakeUnpackedDimensionsNode($2), $3); }
    /* TODO(fangism): $2 must start with unsized dimensions '[' ']'
     *   for dynamic_array_new.
     */
    /* TODO(fangism): Arrays should not be assigned to a singular rvalue. */
  ;
trailing_decl_assignment_opt
  : trailing_decl_assignment
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
trailing_decl_assignment
  /* similar to trailing_assign */
  : '=' dynamic_array_new
    { $$ = MakeTaggedNode(N::kTrailingAssign, $1, $2); }
  | '=' expression
    { $$ = MakeTaggedNode(N::kTrailingAssign, $1, $2); }
  | '=' class_new
    { $$ = MakeTaggedNode(N::kTrailingAssign, $1, $2); }
  /* TODO(fangism): allow [ class_id "::" ] class scope before 'new' */
  ;
method_qualifier_list_opt
  : method_qualifier_list
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
method_qualifier_list
  : method_qualifier_list method_qualifier
    { $$ = ExtendNode($1, $2); }
  | method_qualifier
    { $$ = MakeTaggedNode(N::kQualifierList, $1); }
  ;
method_property_qualifier_list_not_starting_with_virtual
  : method_property_qualifier_list_not_starting_with_virtual
    method_property_qualifier
    { $$ = ExtendNode($1, $2); }
  | property_qualifier  /* excludes TK_virtual */
    { $$ = MakeTaggedNode(N::kQualifierList, $1); }
  ;
method_qualifier
  : TK_virtual
    { $$ = move($1); }
  | TK_pure TK_virtual
    { $$ = move($1); }
  | class_item_qualifier
    { $$ = move($1); }
  ;
method_property_qualifier
  /* grammatic simplification: unify method_qualifier with property qualifier */
  : TK_virtual
    { $$ = move($1); }
  | class_item_qualifier
    { $$ = move($1); }
  | random_qualifier
    { $$ = move($1); }
  ;

modport_declaration
  : TK_modport modport_item_list ';'
    { $$ = MakeTaggedNode(N::kModportDeclaration, $1, $2, $3); }
  ;
modport_item_list
  : modport_item
    { $$ = MakeTaggedNode(N::kModportItemList, $1); }
  | modport_item_list ',' modport_item
    { $$ = ExtendNode($1, $2, $3); }
  ;
modport_item
  : GenericIdentifier '(' modport_ports_list ')'
    { $$ = MakeTaggedNode(N::kModportItem, $1, MakeParenGroup($2, $3, $4)); }
  ;
modport_ports_list
  /* This is a list of modport_ports_declaration, whose rule would normally
     be written as (from LRM):

       modport_ports_list
         : modport_ports_list ',' modport_ports_declaration
         | modport_ports_declaration
         ;
       modport_ports_declaration
         : modport_clocking_declaration
         | modport_simple_ports_declaration
         | modport_tf_ports_declaration
         ;
       // each of these could be prefixed with 'attribute_list_opt'

     This set of rules is implemented in a state machine fashion to
     eliminate the S/R conflict on ',' which acts as two types of separators:
     separating declarations of the same type, and declarations of different
     types.  By writing explicit rules about the context before the separator,
     we effectively give this a two-token lookahead.

     The tree structure returned is a (two-level) list-of-lists, where
     the inner list can cover multiple identifier declarations.
    */
  : modport_simple_ports_declaration_last
    { $$ = move($1); }
  | modport_tf_ports_declaration_last
    { $$ = move($1); }
  | modport_clocking_declaration_last
    { $$ = move($1); }
  ;

dpi_spec_string
  : TK_StringLiteral
    { $$ = move($1); }
  /* TODO(fangism): Verify this is "DPI-C" or "DPI". */
  ;
dpi_import_property_opt
  : dpi_import_property
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
dpi_import_property
  : TK_context
    { $$ = move($1); }
    /* suitable for task_prototype and function_prototype */
  | TK_pure
    { $$ = move($1); }
    /* suitable for only function_prototype */
  ;

dpi_import_export
  : dpi_import_item
    { $$ = move($1); }
  | dpi_export_item
    { $$ = move($1); }
  ;
dpi_export_item
  /* The following rules are expanded from:
   * TK_export dpi_spec_string { GenericIdentifier '=' }_opt modport_tf_port ';'
   */
  : TK_export dpi_spec_string GenericIdentifier '=' modport_tf_port ';'
    { $$ = MakeTaggedNode(N::kDPIExportItem, $1, $2, $3, $4, $5, $6); }
  | TK_export dpi_spec_string modport_tf_port ';'
    { $$ = MakeTaggedNode(N::kDPIExportItem, $1, $2, nullptr, nullptr, $3, $4); }
  ;
import_export
  : TK_export
    { $$ = move($1); }
  | TK_import
    { $$ = move($1); }
  ;
dpi_import_item
  /* The following rules are expanded from:
   * TK_import dpi_spec_string dpi_import_property_opt
   *   { GenericIdentifier '=' }_opt modport_tf_port ';'
   */
  : TK_import dpi_spec_string dpi_import_property_opt
    GenericIdentifier '=' modport_tf_port ';'
    { $$ = MakeDPIImport($1, $2, $3, $4, $5, $6, $7); }
  | TK_import dpi_spec_string dpi_import_property_opt modport_tf_port ';'
    { $$ = MakeDPIImport($1, $2, $3, nullptr, nullptr, $4, $5); }
  ;

modport_ports_declaration_trailing_comma
  : modport_simple_ports_declaration_trailing_comma
    { $$ = move($1); }
  | modport_tf_ports_declaration_trailing_comma
    { $$ = move($1); }
  | modport_clocking_declaration_trailing_comma
    { $$ = move($1); }
  ;
modport_simple_ports_declaration_trailing_comma
  : modport_simple_ports_declaration_last ','
    /* At this point, we don't know whether the comma is followed by
       a keyword or another continued declaration.  Return a 2-tuple.  */
    { $$ = MakeNode($1, $2); }
  ;
modport_tf_ports_declaration_trailing_comma
  : modport_tf_ports_declaration_last ','
    { $$ = MakeNode($1, $2); }  /* Return a 2-tuple. */
  ;
modport_clocking_declaration_trailing_comma
  : modport_clocking_declaration_last ','
    { $$ = MakeNode($1, $2); }  /* Return a 2-tuple. */
  ;
modport_tf_ports_declaration_begin
  : import_export
    { $$ = MakeTaggedNode(N::kModportPortList,
                          MakeTaggedNode(N::kModportTFPortsDeclaration, $1)); }
  | modport_ports_declaration_trailing_comma import_export
    { $$ = ExtendFirstSublist(
          $1, MakeTaggedNode(N::kModportTFPortsDeclaration, $2));
    }
  ;
modport_tf_ports_declaration_last
  : modport_tf_ports_declaration_begin modport_tf_port
    { $$ = ExtendLastSublist($1, $2); }
  | modport_tf_ports_declaration_trailing_comma modport_tf_port
    { $$ = ExtendLastSublistWithSeparator($1, $2); }
  ;
modport_clocking_declaration_begin
  : TK_clocking
    { $$ = MakeTaggedNode(
        N::kModportPortList,
        MakeTaggedNode(N::kModportClockingPortsDeclaration, $1)); }
  | modport_ports_declaration_trailing_comma TK_clocking
    { $$ = ExtendFirstSublist(
          $1, MakeTaggedNode(N::kModportClockingPortsDeclaration, $2));
    }
  ;
modport_clocking_declaration_last
  : modport_clocking_declaration_begin GenericIdentifier
    { $$ = ExtendLastSublist($1, $2); }
  /* clocking declarations only take a single identifier (not a list),
     thus the following rule is disabled:
  | modport_clocking_declaration_trailing_comma GenericIdentifier
   */
  ;
modport_simple_ports_declaration_begin
  : port_direction
    { $$ = MakeTaggedNode(
          N::kModportPortList,
          MakeTaggedNode(N::kModportSimplePortsDeclaration, $1)); }
  | modport_ports_declaration_trailing_comma port_direction
    { $$ = ExtendFirstSublist(
          $1, MakeTaggedNode(N::kModportSimplePortsDeclaration, $2));
    }
  ;
modport_simple_ports_declaration_last
  : modport_simple_ports_declaration_begin modport_simple_port
    { $$ = ExtendLastSublist($1, $2); }
  | modport_simple_ports_declaration_trailing_comma modport_simple_port
    { $$ = ExtendLastSublistWithSeparator($1, $2); }
  ;

modport_simple_port
  : '.' member_name '(' expression ')'
    { $$ = MakeTaggedNode(N::kModportSimplePort, $1, $2,
                          MakeParenGroup($3, $4, $5)); }
    /* TODO(fangism): use distinct enums for these two cases */
  | GenericIdentifier
    { $$ = MakeTaggedNode(N::kModportSimplePort, $1); }
  ;
modport_tf_port
  : task_prototype
    { $$ = move($1); }
  | function_prototype
    { $$ = move($1); }
  | GenericIdentifier
    { $$ = move($1); }
  ;
non_integer_type
  : TK_real
    { $$ = move($1); }
  | TK_realtime
    { $$ = move($1); }
  | TK_shortreal
    { $$ = move($1); }
  ;

macro_digits
  : MacroCall
    { $$ = move($1); }
  | MacroIdentifier
    { $$ = move($1); }
  ;
based_number
  : dec_based_number
    { $$ = move($1); }
  | bin_based_number
    { $$ = move($1); }
  | oct_based_number
    { $$ = move($1); }
  | hex_based_number
    { $$ = move($1); }
  ;
dec_based_number
  : TK_DecBase TK_DecDigits
    { $$ = MakeTaggedNode(N::kBaseDigits, $1, $2); }
  | TK_DecBase TK_XZDigits
    { $$ = MakeTaggedNode(N::kBaseDigits, $1, $2); }
  | TK_DecBase macro_digits
    { $$ = MakeTaggedNode(N::kBaseDigits, $1, $2); }
  ;
bin_based_number
  : TK_BinBase TK_BinDigits
    { $$ = MakeTaggedNode(N::kBaseDigits, $1, $2); }
  | TK_BinBase macro_digits
    { $$ = MakeTaggedNode(N::kBaseDigits, $1, $2); }
  ;
oct_based_number
  : TK_OctBase TK_OctDigits
    { $$ = MakeTaggedNode(N::kBaseDigits, $1, $2); }
  | TK_OctBase macro_digits
    { $$ = MakeTaggedNode(N::kBaseDigits, $1, $2); }
  ;
hex_based_number
  : TK_HexBase TK_HexDigits
    { $$ = MakeTaggedNode(N::kBaseDigits, $1, $2); }
  | TK_HexBase macro_digits
    { $$ = MakeTaggedNode(N::kBaseDigits, $1, $2); }
  ;
number
  : based_number
    { $$ = MakeTaggedNode(N::kNumber, $1); }
  | TK_DecNumber
    { $$ = MakeTaggedNode(N::kNumber, $1); }
  | constant_dec_number based_number
    { $$ = MakeTaggedNode(N::kNumber, $1, $2); }
  | TK_UnBasedNumber
    { $$ = MakeTaggedNode(N::kNumber, $1); }
  | constant_dec_number TK_UnBasedNumber
    { $$ = MakeTaggedNode(N::kNumber, $1, $2); }
  ;
/* allow `macro where one might expect a constant number */
constant_dec_number
  : TK_DecNumber
    { $$ = move($1); }
  | MacroNumericWidth
    { $$ = move($1); }
  ;
open_range_list
  : open_range_list ',' value_range
    { $$ = ExtendNode($1, $2, $3); }
  | value_range
    { $$ = MakeTaggedNode(N::kOpenRangeList, $1); }
  ;
package_declaration
  : TK_package lifetime_opt GenericIdentifier ';'
    package_item_list_opt
    TK_endpackage label_opt
    { $$ = MakeTaggedNode(N::kPackageDeclaration, $1, $2, $3, $4, $5, $6, $7); }
  ;
module_package_import_list_opt
  : package_import_list
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
package_import_list
  : package_import_declaration
    { $$ = MakeTaggedNode(N::kPackageImportList, $1); }
  | package_import_list package_import_declaration
    { $$ = ExtendNode($1, $2); }
  ;
package_import_declaration
  : TK_import package_import_item_list ';'
    { $$ = MakeTaggedNode(N::kPackageImportDeclaration, $1, $2, $3); }
  ;
package_export_declaration
  : TK_export '*' TK_SCOPE_RES '*' ';'
    { $$ = MakeTaggedNode(N::kPackageExportDeclaration, $1,
                          MakeTaggedNode(N::kPackageImportItem,
                                         MakeTaggedNode(N::kScopePrefix, $2, $3),
                                         $4),
                          $5);
    }
  | TK_export package_import_item_list ';'
    { $$ = MakeTaggedNode(N::kPackageExportDeclaration, $1, $2, $3); }
  ;
package_import_item
  : scope_prefix GenericIdentifier
    { $$ = MakeTaggedNode(N::kPackageImportItem, $1, $2); }
  | scope_prefix '*'
    { $$ = MakeTaggedNode(N::kPackageImportItem, $1, $2); }
  /**
  : GenericIdentifier TK_SCOPE_RES GenericIdentifier
  | GenericIdentifier TK_SCOPE_RES '*'
  **/
  ;
package_import_item_list
  : package_import_item_list ',' package_import_item
    { $$ = ExtendNode($1, $2, $3); }
  | package_import_item
    { $$ = MakeTaggedNode(N::kPackageImportItemList, $1); }
  ;
package_item
  : package_item_no_pp
    { $$ = move($1); }
  | preprocessor_balanced_package_items
    { $$ = move($1); }
  | preprocessor_action
    { $$ = move($1); }
  ;
package_item_no_pp
  : package_or_generate_item_declaration
    { $$ = move($1); }
  | timeunits_declaration
    { $$ = move($1); }
  | type_declaration
    { $$ = move($1); }
  | data_declaration
    { $$ = move($1); }
  | interface_data_declaration
    { $$ = move($1); }
  | clocking_declaration
    { $$ = move($1); }
  | let_declaration
    { $$ = move($1); }
  | constraint_declaration_package_item
    { $$ = move($1); }
  | package_import_declaration
    { $$ = move($1); }
  | package_export_declaration
    { $$ = move($1); }
  | timescale_directive
    { $$ = move($1); }
  | misc_directive
    { $$ = move($1); }
  | module_item_directive
    { $$ = move($1); }
  | macro_call_or_item
    { $$ = move($1); }
  | error ';'  /* error in data or type declaration */
    { yyerrok; $$ = Recover(); }
  /**
   * Allow subset of module_items at the top-level to be able to
   * parse files that are `included within module bodies.
   * Tracked as b/36417019.
   **/
  | any_param_declaration
    { $$ = move($1); }
  | /* attribute_list_opt */ TK_initial statement_item
    { $$ = MakeTaggedNode(N::kInitialStatement, $1, $2); }
  ;
preprocessor_balanced_package_items
  : preprocessor_if_header package_item_list_opt
    preprocessor_elsif_package_items_opt
    preprocessor_else_package_item_opt
    PP_endif
    { $$ = MakeTaggedNode(N::kPreprocessorBalancedPackageItems,
                          ExtendNode($1, $2), ForwardChildren($3), $4, $5);
    }
  ;
preprocessor_elsif_package_items_opt
  : preprocessor_elsif_package_items
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
preprocessor_elsif_package_items
  : preprocessor_elsif_package_items preprocessor_elsif_package_item
    { $$ = ExtendNode($1, $2); }
  | preprocessor_elsif_package_item
    { $$ = MakeNode($1); }  /* Don't bother tagging; node will be flattened. */
  ;
preprocessor_elsif_package_item
  : preprocessor_elsif_header package_item_list_opt
    { $$ = ExtendNode($1, $2); }
  ;
preprocessor_else_package_item_opt
  : preprocessor_else_package_item
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
preprocessor_else_package_item
  : PP_else package_item_list_opt
    { $$ = MakeTaggedNode(N::kPreprocessorElseClause, $1, $2); }
  ;
package_item_list
  : package_item_list package_item
    { $$ = ExtendNode($1, $2); }
  | package_item
    { $$ = MakeTaggedNode(N::kPackageItemList, $1); }
  ;
package_item_list_opt
  : package_item_list
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
misc_directive
  /* These directives may appear in top/package scope. */
  : DR_resetall
    { $$ = move($1); }
  | DR_celldefine
    { $$ = move($1); }
  | DR_endcelldefine
    { $$ = move($1); }
  | DR_unconnected_drive pull01
    { $$ = MakeTaggedNode(N::kTopLevelDirective, $1, $2); }
  | DR_nounconnected_drive
    { $$ = move($1); }
  | DR_default_nettype net_type_or_none
    { $$ = MakeTaggedNode(N::kTopLevelDirective, $1, $2); }
  | DR_suppress_faults
    { $$ = move($1); }
  | DR_nosuppress_faults
    { $$ = move($1); }
  | DR_enable_portfaults
    { $$ = move($1); }
  | DR_disable_portfaults
    { $$ = move($1); }
  | DR_delay_mode_distributed
    { $$ = move($1); }
  | DR_delay_mode_path
    { $$ = move($1); }
  | DR_delay_mode_unit
    { $$ = move($1); }
  | DR_delay_mode_zero
    { $$ = move($1); }
  | DR_default_decay_time decay_value_simple
    { $$ = MakeTaggedNode(N::kTopLevelDirective, $1, $2); }
    /* $2 can be real or integer time */
  | DR_default_trireg_strength TK_DecNumber
    { $$ = MakeTaggedNode(N::kTopLevelDirective, $1, $2); }
    /* $2 is integer in [0,250] */
  | DR_pragma
    { $$ = move($1); }
  | DR_uselib
    { $$ = move($1); }
  | DR_begin_keywords TK_StringLiteral
    { $$ = MakeTaggedNode(N::kTopLevelDirective, $1, $2); }
    /* $2 should name a standard, e.g. "1800-2012" or "1364-2005" */
  | DR_end_keywords
    { $$ = move($1); }
  ;
net_type_or_none
  : net_type
    { $$ = move($1); }
  | GenericIdentifier
    { $$ = move($1); }
    /* $1 should be 'none', which is not a keyword. */
  ;
module_item_directive
  /* These directives may appear within a module. */
  : DR_protect
    { $$ = move($1); }
  | DR_endprotect
    { $$ = move($1); }
  ;
port_direction
  : dir
    { $$ = move($1); }
  | TK_ref
    { $$ = move($1); }
  ;
tf_port_direction
  : port_direction
    { $$ = move($1); }
  | TK_const TK_ref
    { $$ = MakeTaggedNode(N::kConstRef, $1, $2); }
  ;
tf_port_direction_opt
  : tf_port_direction
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
property_qualifier
  : class_item_qualifier
    { $$ = move($1); }
  | random_qualifier
    { $$ = move($1); }
  ;
property_spec
  : event_control_opt property_spec_disable_iff_opt property_expr
    { $$ = MakeTaggedNode(N::kPropertySpec, $1, $2, $3); }
  ;
sequence_spec
  : event_control_opt property_spec_disable_iff_opt sequence_expr
    { $$ = MakeTaggedNode(N::kSequenceSpec, $1, $2, $3); }
  ;
property_spec_disable_iff
  : TK_disable TK_iff '(' expression_or_dist ')'
    { $$ = MakeTaggedNode(N::kPropertySpecDisableIff,
                          $1, $2, MakeParenGroup($3, $4, $5)); }
  ;
property_spec_disable_iff_opt
  : property_spec_disable_iff
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
random_qualifier_opt
  : random_qualifier
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
random_qualifier
  : TK_rand
    { $$ = move($1); }
  | TK_randc
    { $$ = move($1); }
  ;

signing
  : TK_signed
    { $$ = move($1); }
  | TK_unsigned
    { $$ = move($1); }
  ;
statement
  : /* attribute_list_opt */ statement_item
    { $$ = move($1); }
  | unqualified_id ':' /* attribute_list_opt */ statement_item
    { $$ = MakeTaggedNode(N::kLabeledStatement, $1, $2, $3); }
    /* $1 should be a GenericIdentifier, but unqualified_id avoids conflict. */
  ;
statement_or_null
  : statement
    { $$ = move($1); }
  | /* attribute_list_opt */ ';'
    { $$ = MakeTaggedNode(N::kNullStatement, $1); }
  ;
block_item_or_statement_or_null
  : block_item_decl
      { $$ = move($1); }
  | statement_or_null
      { $$ = move($1); }
  ;
block_item_or_statement_or_null_list
  : block_item_or_statement_or_null_list block_item_or_statement_or_null
    { $$ = ExtendNode($1, $2); }
  | block_item_or_statement_or_null
    { $$ = MakeTaggedNode(N::kBlockItemStatementList, $1); }
  ;
block_item_or_statement_or_null_list_opt
  : block_item_or_statement_or_null_list
    { $$ = move($1); }
  | /* empty */
    { $$ = MakeTaggedNode(N::kBlockItemStatementList); }
  ;
stream_expression
  : expression
    { $$ = move($1); }
  /* TODO(fangism):
  | expression TK_with select_variable_dimension
   */
  ;
stream_expression_list
  : stream_expression_list ',' stream_expression
    { $$ = ExtendNode($1, $2, $3); }
  | stream_expression
    { $$ = MakeTaggedNode(N::kStreamExpressionList, $1); }
  ;
stream_operator
  : TK_LS
    { $$ = move($1); }
  | TK_RS
    { $$ = move($1); }
  ;
streaming_concatenation
  : '{' stream_operator slice_size_opt '{' stream_expression_list '}' '}'
    { $$ = MakeTaggedNode(N::kStreamingConcatenation, $1, $2, $3,
                          MakeBraceGroup($4, $5, $6), $7); }
  /* accommodate macro call as operand of stream_operator */
  | '{' stream_operator slice_size MacroCall '}'
    { $$ = MakeTaggedNode(N::kStreamingConcatenation, $1, $2, $3, $4, $5); }
  | '{' stream_operator slice_size MacroIdentifier '}'
    { $$ = MakeTaggedNode(N::kStreamingConcatenation, $1, $2, $3, $4, $5); }
  | '{' stream_operator MacroCall '}'
    { $$ = MakeTaggedNode(N::kStreamingConcatenation, $1, $2, nullptr, $3, $4); }
  | '{' stream_operator MacroIdentifier '}'
    { $$ = MakeTaggedNode(N::kStreamingConcatenation, $1, $2, nullptr, $3, $4); }
  ;
slice_size_opt
  : slice_size
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
slice_size
  /* similar to parameter_expr */
  /* original slice_size:
  : expression  // must be constant
  | data_type
  * However, to avoid conflict with the '{' that follows, we really want
  * any-expression-that-doesn't-start-with-{.
  * A close compromise is expr_primary without expr_primary_braces.
  */
  : expr_primary_no_groups
    { $$ = move($1); }
  | expr_primary_parens
    { $$ = move($1); }
  | reference_or_call
    { $$ = move($1); }
  | data_type_primitive
    { $$ = move($1); }
  /* non-primitive data-types already covered by reference_or_call */
  ;
task_prototype
  : TK_task lifetime_opt GenericIdentifier tf_port_list_paren_opt
  /* users of this rule may append a trailing ';' */
    { $$ = MakeTaggedNode(N::kTaskPrototype, qualifier_placeholder,
                          $1, $2, $3, $4); }

  ;
task_declaration
  : TK_task lifetime_opt task_declaration_id tf_port_list_paren_opt ';'
    /** replaced with unified list to eliminate conflict
    task_item_list_opt statement_or_null_list_opt
    **/
    tf_item_or_statement_or_null_list_opt
    TK_endtask label_opt
    { $$ = MakeTaggedNode(N::kTaskDeclaration,
               MakeTaggedNode(N::kTaskHeader, qualifier_placeholder,
                   $1, $2, $3, $4, $5),
               $6, $7, $8); }
    /* The declared name, $3, can be scope-qualified,
     * which is covered by class_id.
     */
/** merged above
  | TK_task lifetime_opt task_declaration_id
    '(' tf_port_list_opt ')' ';'
    block_item_decls_opt
    statement_or_null_list_opt
    TK_endtask
    label_opt
**/
  ;
task_declaration_id
  : GenericIdentifier scope_or_if_res GenericIdentifier
    { $$ = MakeTaggedNode(N::kQualifiedId,
           MakeTaggedNode(N::kUnqualifiedId, $1), $2,
           MakeTaggedNode(N::kUnqualifiedId, $3)); }
  | GenericIdentifier
    { $$ = MakeTaggedNode(N::kUnqualifiedId, $1); }
  ;
tf_port_declaration
  /* tf_port_direction signed_unsigned_opt
   * class_id_opt decl_dimensions_opt
   * list_of_tf_variable_identifiers ';'
   */
  /* Expanded to avoid GenericIdentifier conflict on implicit type: */
  : tf_port_direction signed_unsigned_opt
    qualified_id decl_dimensions_opt
    list_of_tf_variable_identifiers ';'
    { $$ = MakeTaggedNode(N::kTFPortDeclaration, $1,
                          MakeTaggedNode(N::kDataType,
                                         MakeTaggedNode(N::kDataTypePrimitive,
                                                        $2, $3),
                                         MakePackedDimensionsNode($4)),
                          $5, $6); }
  | tf_port_direction signed_unsigned_opt
    unqualified_id decl_dimensions_opt
    list_of_tf_variable_identifiers ';'
    { $$ = MakeTaggedNode(N::kTFPortDeclaration, $1,
                          MakeTaggedNode(N::kDataType,
                                         MakeTaggedNode(N::kDataTypePrimitive, $2, $3),
                                         MakePackedDimensionsNode($4)),
                          $5, $6); }
  | tf_port_direction signed_unsigned_opt decl_dimensions
    list_of_tf_variable_identifiers ';'
    { $$ = MakeTaggedNode(N::kTFPortDeclaration, $1,
                          MakeTaggedNode(N::kDataType,
                                         MakeTaggedNode(N::kDataTypePrimitive, $2, nullptr),
                                         MakePackedDimensionsNode($3)),
                          $4, $5); }
  | tf_port_direction signed_unsigned_opt
    list_of_tf_variable_identifiers ';'
    { $$ = MakeTaggedNode(N::kTFPortDeclaration, $1,
                          MakeTaggedNode(N::kDataType,
                                         MakeTaggedNode(N::kDataTypePrimitive, $2),
                                         nullptr),
                          $3, $4); }
  | tf_port_direction data_type_primitive
    list_of_tf_variable_identifiers ';'
    { $$ = MakeTaggedNode(N::kTFPortDeclaration, $1,
                          MakeTaggedNode(N::kDataType, $2, nullptr),
                          $3, $4); }
  ;
list_of_tf_variable_identifiers
  : list_of_tf_variable_identifiers ',' tf_variable_identifier
    { $$ = ExtendNode($1, $2, $3); }
  | tf_variable_identifier_first
    { $$ = MakeTaggedNode(N::kTFVariableIdentifierList, $1); }
  ;
tf_variable_identifier_first
  /* This rule is a workaround to resolve a S/R conflict from implicit types. */
  : unqualified_id decl_dimensions_opt trailing_assign_opt
    { $$ = MakeTaggedNode(N::kTFVariableIdentifier, $1, $2, $3); }
    /* TODO(fangism): Verify that $1 doesn't actually have any parameters. */
  ;
tf_variable_identifier
  : GenericIdentifier decl_dimensions_opt trailing_assign_opt
    { $$ = MakeTaggedNode(N::kTFVariableIdentifier, $1, $2, $3); }
  ;
tf_port_item
  : tf_port_direction_opt data_type_or_implicit_basic_followed_by_id_and_dimensions_opt
    tf_port_item_expr_opt
    { $$ = MakeTaskFunctionPortItem($1, $2, $3); }
  | tf_port_direction_opt interface_type GenericIdentifier decl_dimensions_opt
    tf_port_item_expr_opt
    { $$ = MakeTaskFunctionPortItem($1,
                          MakeTypeIdDimensionsTuple(
                              MakeTaggedNode(N::kDataType, $2, nullptr),
                              $3, MakeUnpackedDimensionsNode($4)),
                          $5); }
  ;
tf_port_item_expr_opt
  : '=' expression
    { $$ = MakeTaggedNode(N::kTrailingAssign, $1, $2); }
  | /* empty */
    { $$ = nullptr; }
  ;

tf_port_list
  : tf_port_list_item_last
    { $$ = move($1); }
  | tf_port_list_preprocessor_last
    { $$ = move($1); }
  ;
tf_port_list_trailing_comma
  : tf_port_list ','
    { $$ = ExtendNode($1, $2); }
  ;
tf_port_list_item_last
  : tf_port_list_trailing_comma tf_port_item
    { $$ = ExtendNode($1, $2); }
  | tf_port_list_preprocessor_last tf_port_item
    { $$ = ExtendNode($1, $2); }
  | tf_port_item
    { $$ = MakeTaggedNode(N::kPortList, $1); }
  ;
tf_port_list_preprocessor_last
  : tf_port_list preprocessor_directive
    { $$ = ExtendNode($1, $2); }
  | tf_port_list_trailing_comma preprocessor_directive
    { $$ = ExtendNode($1, $2); }
  | preprocessor_directive
    { $$ = MakeTaggedNode(N::kPortList, $1); }
  ;

timescale_directive
  : DR_timescale time_literal '/' time_literal
    { $$ = MakeTaggedNode(N::kTimescaleDirective, $1, $2, $3, $4); }
  | DR_timescale MacroGenericItem
    { $$ = MakeTaggedNode(N::kTimescaleDirective, $1, $2); }
  ;
time_literal
  : TK_TimeLiteral
    { $$ = MakeTaggedNode(N::kTimeLiteral, $1); }
  | TK_DecNumber TK_timescale_unit
    { $$ = MakeTaggedNode(N::kTimeLiteral, $1, $2); }
  ;
timeunits_declaration
  : TK_timeunit TK_TimeLiteral ';'
    { $$ = MakeTaggedNode(N::kTimeunitsDeclaration, $1, $2, $3); }
  | TK_timeunit TK_TimeLiteral '/' TK_TimeLiteral ';'
    { $$ = MakeTaggedNode(N::kTimeunitsDeclaration, $1, $2, $3, $4, $5); }
  | TK_timeprecision TK_TimeLiteral ';'
    { $$ = MakeTaggedNode(N::kTimeunitsDeclaration, $1, $2, $3); }
  /* Originated from iverilog parser, but mo mention of them in the LRM: */
  | TK_timeunit_check TK_TimeLiteral ';'
    { $$ = MakeTaggedNode(N::kTimeunitsDeclaration, $1, $2, $3); }
  | TK_timeunit_check TK_TimeLiteral '/' TK_TimeLiteral ';'
    { $$ = MakeTaggedNode(N::kTimeunitsDeclaration, $1, $2, $3, $4, $5); }
  | TK_timeprecision_check TK_TimeLiteral ';'
    { $$ = MakeTaggedNode(N::kTimeunitsDeclaration, $1, $2, $3); }
  ;
value_range
  : expression
    { $$ = move($1); }
  | '[' expression ':' expression ']'
    { $$ = MakeTaggedNode(N::kValueRange, $1, $2, $3, $4, $5); }
  /* half-open ranges are only legal in certain contexts */
  ;

select_variable_dimension
  /* used by references and element selection */
  : '[' expression ':' expression ']'
    { $$ = MakeTaggedNode(N::kSelectVariableDimension, $1, $2, $3, $4, $5); }
  | '[' expression_or_null_list_opt ']'
    { $$ = MakeTaggedNode(N::kSelectVariableDimension, $1, $2, $3); }
  /* indexed_range: */
  | '[' expression TK_PO_POS expression ']'
    { $$ = MakeTaggedNode(N::kSelectVariableDimension, $1, $2, $3, $4, $5); }
  | '[' expression TK_PO_NEG expression ']'
    { $$ = MakeTaggedNode(N::kSelectVariableDimension, $1, $2, $3, $4, $5); }
  ;

decl_variable_dimension
  /* used in declaration contexts */
  : '[' expression ':' expression ']'
    { $$ = MakeTaggedNode(N::kDimensionRange, $1, $2, $3, $4, $5); }
  | '[' expression_or_null_list_opt ']'
    { $$ = MakeTaggedNode(N::kDimensionScalar, $1, $2, $3); }
    /* This covers the common form of: '[' expression ']' .
     * When this is used as '[' loop_variables ']' in the foreach context,
     * each item should be GenericIdentifier, referring to a loop variable,
     * or blank.
     * This also covers '[' ']' empty brackets, which is an unsized_dimension
     * for dynamic array allocations.
     * "+:" and "-:" are slice operators, and $4 should be constant expressions.
     */
  | '[' expression TK_PO_POS expression ']'
    { $$ = MakeTaggedNode(N::kDimensionSlice, $1, $2, $3, $4, $5); }
  | '[' expression TK_PO_NEG expression ']'
    { $$ = MakeTaggedNode(N::kDimensionSlice, $1, $2, $3, $4, $5); }
  /* queue dimension, covered by above: */
  /* simplification:
   * added the following to look like index_extension (only for hierarchy_identifier)
   */
  /* associative dimension: */
  | '[' data_type_primitive ']'
    { $$ = MakeTaggedNode(N::kDimensionAssociativeType, $1, $2, $3 ); }
    /* non-primitive data_type are covered by expression */
  | lb_star_rb
    { $$ = move($1); }
  ;

lb_star_rb
  /* The following are accepted as '[*]' for associative array declarations: */
  : '[' '*' ']'
    { $$ = MakeTaggedNode(N::kDimensionAssociativeIntegral, $1, $2, $3); }
  | TK_LBSTARRB
    { $$ = MakeTaggedNode(N::kDimensionAssociativeIntegral, $1); }
  | TK_LBSTAR ']'
    { $$ = MakeTaggedNode(N::kDimensionAssociativeIntegral, $1, $2); }
  ;

/** ignore attributes for now, treat them as comments in lexer
attribute_list_opt
  : attribute_instance_list
  | // empty
  ;
attribute_instance_list
  : TK_PSTAR TK_STARP
  | TK_PSTAR attribute_list TK_STARP
  | attribute_instance_list TK_PSTAR TK_STARP
  | attribute_instance_list TK_PSTAR attribute_list TK_STARP
  ;
attribute_list
  : attribute_list ',' attribute
  | attribute
  ;
attribute
  : GenericIdentifier '=' expression
  | GenericIdentifier
  ;
**/
any_param_declaration
  // TODO(fangism): restructure trailing assign lists
  /** grouped id with type to allow ranged decl without conflict
   * | TK_parameter param_type parameter_assign_list ';'
   * | TK_localparam param_type localparam_assign_list ';'
   */
  : TK_parameter param_type_followed_by_id_and_dimensions_opt
    trailing_assign ',' parameter_assign_list ';'
    { $$ = MakeTaggedNode(N::kParamDeclaration, $1, $2, $3, $4, $5, $6); }
  | TK_parameter param_type_followed_by_id_and_dimensions_opt
    trailing_assign ';'
    { $$ = MakeTaggedNode(N::kParamDeclaration, $1, $2, $3, $4); }
  | TK_localparam param_type_followed_by_id_and_dimensions_opt
    trailing_assign ',' localparam_assign_list ';'
    { $$ = MakeTaggedNode(N::kParamDeclaration, $1, $2, $3, $4, $5, $6); }
  | TK_localparam param_type_followed_by_id_and_dimensions_opt
    trailing_assign ';'
    { $$ = MakeTaggedNode(N::kParamDeclaration, $1, $2, $3, $4); }
  /* The next rules are type parameter declarations: */
  | TK_parameter TK_type type_assignment_list ';'
    { $$ = MakeTaggedNode(N::kParamDeclaration, $1, $2, $3, $4); }
  | TK_localparam TK_type type_assignment_list ';'
    { $$ = MakeTaggedNode(N::kParamDeclaration, $1, $2, $3, $4); }
  ;

instantiation_type
  : data_type
    { $$ = MakeTaggedNode(N::kInstantiationType, $1); }
  | interface_type
    { $$ = MakeTaggedNode(N::kInstantiationType, $1); }
  ;

instantiation_base
  : instantiation_type gate_instance_or_register_variable_list
    { $$ = MakeInstantiationBase($1, $2); }
  ;

data_declaration_or_module_instantiation
  /* data_declaration: */
  /* expanded from:
   *   const_opt var_opt lifetime_opt
   *   [ data_type | interface_type ] gate_instance_or_register_variable_list ';'
   * to avoid S/R conflict.
   */
  : instantiation_base ';'
    { $$ = MakeDataDeclaration(qualifier_placeholder, $1, $2); }
  | lifetime const_opt instantiation_base ';'
    { $$ = MakeDataDeclaration(
                          MakeTaggedNode(N::kQualifierList, $1, $2), $3, $4); }
    /* const_opt was added here to support "static const" (vs. "const static")
     * which is not explicitly permitted by the LRM grammar, but is interpreted
     * by some vendors as intended to be permitted.
     */
  | TK_var lifetime_opt instantiation_base ';'
    { $$ = MakeDataDeclaration(
                          MakeTaggedNode(N::kQualifierList, $1, $2), $3, $4); }
  | TK_const var_opt lifetime_opt instantiation_base ';'
      { $$ = MakeDataDeclaration(
                            MakeTaggedNode(N::kQualifierList, $1, $2, $3),
                            $4, $5); }
  /* Using data_declaration_modifiers_opt causes S/R conflict.  */
  ;

block_item_decl
  : data_declaration_or_module_instantiation
    { $$ = move($1); }
  /* temporarily removed
  | TK_reg data_type register_variable_list ';'
  */
  | package_import_declaration
    { $$ = move($1); }
  | any_param_declaration
    { $$ = move($1); }
  | type_declaration
    { $$ = move($1); }
  | let_declaration
    { $$ = move($1); }
  ;
/** no longer needed
block_item_decls
  : block_item_decl
  | block_item_decls block_item_decl
  ;
block_item_decls_opt
  : block_item_decls
  | // empty
  ;
**/
type_declaration
  : TK_typedef data_type GenericIdentifier decl_dimensions_opt ';'
    { $$ = MakeTaggedNode(N::kTypeDeclaration, $1, $2, nullptr, $3, $4, $5); }
  | TK_typedef TK_class GenericIdentifier ';'
    { $$ = MakeTaggedNode(N::kTypeDeclaration, $1, $2, nullptr, $3, $4, nullptr); }
  | TK_typedef TK_interface TK_class GenericIdentifier ';'
    { $$ = MakeTaggedNode(N::kTypeDeclaration, $1, $2, $3, $4, $5); }
  | TK_typedef interface_type GenericIdentifier ';'
    { $$ = MakeTaggedNode(N::kTypeDeclaration, $1, $2, nullptr, $3, $4, nullptr); }
  /* TODO: Figure out how to make the braced members list optional
     to make these more robust */
  | TK_typedef TK_enum GenericIdentifier ';'
    { $$ = MakeTaggedNode(N::kTypeDeclaration, $1,
                          MakeTaggedNode(N::kDataTypePrimitive,
                                         MakeTaggedNode(N::kEnumType,
                                                        $2, nullptr, nullptr)),
                          nullptr, $3, $4, nullptr); }
  | TK_typedef TK_struct GenericIdentifier ';'
    { $$ = MakeTaggedNode(N::kTypeDeclaration, $1,
                          MakeTaggedNode(N::kDataTypePrimitive,
                                         MakeTaggedNode(N::kStructType,
                                                        $2, nullptr, nullptr)),
                          nullptr, $3, $4, nullptr); }
  | TK_typedef TK_union GenericIdentifier ';'
    { $$ = MakeTaggedNode(N::kTypeDeclaration, $1,
                          MakeTaggedNode(N::kDataTypePrimitive,
                                         MakeTaggedNode(N::kUnionType,
                                                        $2, nullptr, nullptr)),
                          nullptr, $3, $4, nullptr); }
  | TK_typedef GenericIdentifier ';'
    { $$ = MakeTaggedNode(N::kTypeDeclaration, $1, nullptr, nullptr, $2, $3, nullptr); }
  ;
/* enums only become named via typedef */
enum_data_type
  : TK_enum '{' enum_name_list '}'
    { $$ = MakeTaggedNode(N::kEnumType, $1, nullptr, MakeBraceGroup($2, $3, $4)); }
  | TK_enum data_type '{' enum_name_list '}'
    { $$ = MakeTaggedNode(N::kEnumType, $1, $2, MakeBraceGroup($3, $4, $5)); }
  ;
enum_name_list
  : enum_name_list_preprocessor_last
    { $$ = move($1); }
  | enum_name_list_item_last
    { $$ = move($1); }
  ;
enum_name_list_trailing_comma
  : enum_name_list ','
    { $$ = ExtendNode($1, $2); }
  ;
enum_name_list_preprocessor_last
  : enum_name_list preprocessor_directive
    { $$ = ExtendNode($1, $2); }
  | enum_name_list_trailing_comma preprocessor_directive
    { $$ = ExtendNode($1, $2); }
  | preprocessor_directive
    { $$ = MakeTaggedNode(N::kEnumNameList, $1); }
  ;
enum_name_list_item_last
  : enum_name_list_trailing_comma enum_name
    { $$ = ExtendNode($1, $2); }
  | enum_name_list_preprocessor_last enum_name
    { $$ = ExtendNode($1, $2); }
  | enum_name
    { $$ = MakeTaggedNode(N::kEnumNameList, $1); }
  ;

pos_neg_number
  : number
    { $$ = move($1); }
  | '-' number
    { $$ = MakeTaggedNode(N::kNumber, $1, ForwardChildren($2)); }
  ;
enum_name
  : GenericIdentifier
    { $$ = MakeTaggedNode(N::kEnumName, $1); }
  | GenericIdentifier '[' pos_neg_number ']'
    { $$ = MakeTaggedNode(N::kEnumName, $1, MakeBracketGroup($2, $3, $4)); }
  | GenericIdentifier '[' pos_neg_number ':' pos_neg_number ']'
    { $$ = MakeTaggedNode(N::kEnumName, $1, MakeBracketGroup($2, MakeNode($3, $4, $5), $6)); }
  | GenericIdentifier '=' expression
    { $$ = MakeTaggedNode(N::kEnumName, $1, MakeTaggedNode(N::kTrailingAssign, $2, $3)); }
  | GenericIdentifier '[' pos_neg_number ']' '=' expression
    { $$ = MakeTaggedNode(N::kEnumName, $1, MakeBracketGroup($2, $3, $4),
                          MakeTaggedNode(N::kTrailingAssign, $5, $6)); }
  | GenericIdentifier '[' pos_neg_number ':' pos_neg_number ']' '=' expression
    { $$ = MakeTaggedNode(N::kEnumName, $1, MakeBracketGroup($2, MakeNode($3, $4, $5), $6),
                          MakeTaggedNode(N::kTrailingAssign, $7, $8)); }
  ;
/* structs only become named via typedef */
struct_data_type
  : TK_struct packed_signing_opt '{' struct_union_member_list '}'
    { $$ = MakeTaggedNode(N::kStructType, $1, $2, MakeBraceGroup($3, $4, $5)); }
  | TK_union TK_tagged_opt packed_signing_opt '{' struct_union_member_list '}'
    { $$ = MakeTaggedNode(N::kUnionType, $1, $2, $3,
                          MakeBraceGroup($4, $5, $6)); }
  ;
packed_signing_opt
  : TK_packed signed_unsigned_opt
    { $$ = MakeTaggedNode(N::kPackedSigning, $1, $2); }
  | /* empty */
    { $$ = nullptr; }
  ;
struct_union_member_list
  : struct_union_member_list struct_union_member
    { $$ = ExtendNode($1, $2); }
  | struct_union_member
    { $$ = MakeTaggedNode(N::kStructUnionMemberList, $1); }
  ;
struct_union_member
  /* very similar to data_declaration */
  : /* attribute_list_opt */
    random_qualifier_opt
    data_type_or_implicit_followed_by_id_and_dimensions_opt trailing_assign_opt ';'
    { $$ = MakeTaggedNode(N::kStructUnionMember, $1, $2, $3, $4); }
  | random_qualifier_opt
    data_type_or_implicit_followed_by_id_and_dimensions_opt trailing_assign_opt ','
    list_of_variable_decl_assignments ';'
    { $$ = MakeTaggedNode(N::kStructUnionMember, $1, $2, $3, $4, $5, $6); }
  | preprocessor_directive
    { $$ = move($1); }
  ;

case_item
  : expression_list_proper ':' statement_or_null
    { $$ = MakeTaggedNode(N::kCaseItem, $1, $2, $3); }
  | TK_default ':' statement_or_null
    { $$ = MakeTaggedNode(N::kDefaultItem, $1, $2, $3); }
  | TK_default     statement_or_null
    { $$ = MakeTaggedNode(N::kDefaultItem, $1, nullptr, $2); }
  | preprocessor_directive
    { $$ = MakeTaggedNode(N::kCaseItem, $1); }
/**
  | error ':' statement_or_null
**/
  ;
case_inside_item
  : open_range_list ':' statement_or_null
    { $$ = MakeTaggedNode(N::kCaseInsideItem, $1, $2, $3); }
  | TK_default ':' statement_or_null
    { $$ = MakeTaggedNode(N::kCaseInsideItem, $1, $2, $3); }
  | TK_default statement_or_null
    { $$ = MakeTaggedNode(N::kCaseInsideItem, $1, $2); }
  | preprocessor_directive
    { $$ = MakeTaggedNode(N::kCaseInsideItem, $1); }
  ;
case_pattern_item
  : pattern TK_TAND expression ':' statement_or_null
    { $$ = MakeTaggedNode(N::kCasePatternItem, $1, $2, $3, $4, $5); }
  | pattern ':' statement_or_null
    { $$ = MakeTaggedNode(N::kCasePatternItem, $1, $2, $3); }
  | TK_default ':' statement_or_null
    { $$ = MakeTaggedNode(N::kCasePatternItem, $1, $2, $3); }
  | TK_default statement_or_null
    { $$ = MakeTaggedNode(N::kCasePatternItem, $1, $2); }
  | preprocessor_directive
    { $$ = MakeTaggedNode(N::kCasePatternItem, $1); }
  ;
case_items
  : case_items case_item
    { $$ = ExtendNode($1, $2); }
  | case_item
    { $$ = MakeTaggedNode(N::kCaseItemList, $1); }
  ;
case_inside_items
  : case_inside_items case_inside_item
    { $$ = ExtendNode($1, $2); }
  | case_inside_item
    { $$ = MakeTaggedNode(N::kCaseInsideItemList, $1); }
  ;
case_pattern_items
  : case_pattern_items case_pattern_item
    { $$ = ExtendNode($1, $2); }
  | case_pattern_item
    { $$ = MakeTaggedNode(N::kCasePatternItemList, $1); }

  ;
charge_strength
  : '(' TK_small ')'
    { $$ = MakeParenGroup($1, $2, $3); }
  | '(' TK_medium ')'
    { $$ = MakeParenGroup($1, $2, $3); }
  | '(' TK_large ')'
    { $$ = MakeParenGroup($1, $2, $3); }
  ;
charge_strength_opt
  : charge_strength
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;

defparam_assign
  : reference '=' expression
    { $$ = MakeTaggedNode(N::kDefParamAssignment, $1, $2, $3); }
  ;
defparam_assign_list
  : defparam_assign
    { $$ = MakeTaggedNode(N::kDefParamAssignmentList, nullptr, $1); }
  /* TODO(fangism): The following doesn't seem valid, so remove it. */
  | decl_dimensions defparam_assign
    { $$ = MakeTaggedNode(N::kDefParamAssignmentList,
                          MakeUnpackedDimensionsNode($1),
                          $2); }
  | defparam_assign_list ',' defparam_assign
    { $$ = ExtendNode($1, $2, $3); }
  ;
delay1
  : '#' delay_value_simple
    { $$ = MakeTaggedNode(N::kDelay, $1, $2); }
  | '#' '(' delay_value ')'
    { $$ = MakeTaggedNode(N::kDelay, $1, MakeParenGroup($2, $3, $4)); }
  ;
delay3
  : '#' delay_value_simple
    { $$ = MakeTaggedNode(N::kDelay, $1, $2); }
  | '#' '(' delay_value ')'
    { $$ = MakeTaggedNode(N::kDelay, $1, MakeParenGroup($2, $3, $4)); }
  | '#' '(' delay_value ',' delay_value ')'
    { $$ = MakeTaggedNode(N::kDelay, $1, MakeParenGroup($2, MakeNode($3, $4, $5), $6)); }
  | '#' '(' delay_value ',' delay_value ',' delay_value ')'
    { $$ = MakeTaggedNode(N::kDelay, $1, MakeParenGroup($2, MakeNode($3, $4, $5, $6, $7), $8)); }
  ;
delay3_opt
  : delay3
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
delay_value_list
  : delay_value
    { $$ = MakeTaggedNode(N::kDelayValueList, $1); }
  | delay_value_list ',' delay_value
    { $$ = ExtendNode($1, $2, $3); }
  ;
delay_value
  : expression
    { $$ = move($1); }
  | expression ':' expression ':' expression
    // TODO(jeremycs): change this structure to reflect mintypmax
    { $$ = MakeTaggedNode(N::kMinTypMaxList, $1, $2, $3, $4, $5); }
  ;
delay_value_simple
  : TK_DecNumber
    { $$ = MakeTaggedNode(N::kDelayValue, $1); }
  | TK_RealTime
    { $$ = MakeTaggedNode(N::kDelayValue, $1); }
  | delay_identifier
    { $$ = move($1); }
    /* $1 is a package-scope identifier in LRM, but some tools
     * also permit a general hierarchical reference.
     */
  | TK_TimeLiteral
    { $$ = MakeTaggedNode(N::kDelayValue, $1); }
  | TK_1step
    { $$ = MakeTaggedNode(N::kDelayValue, $1); }
  ;
delay_identifier
  : delay_identifier '.' GenericIdentifier
    { $$ = ExtendNode($1, $2, $3); }
  | delay_scope
    { $$ = move($1); }
  ;
delay_scope
  : delay_scope TK_SCOPE_RES GenericIdentifier
    { $$ = ExtendNode($1, $2, $3); }
  | GenericIdentifier
    { $$ = MakeTaggedNode(N::kDelayValue, $1);  }
  ;
decay_value_simple
  : TK_DecNumber
    { $$ = move($1); }
  | TK_RealTime
    { $$ = move($1); }
  | TK_TimeLiteral
    { $$ = move($1); }
  | TK_infinite
    { $$ = move($1); }
  ;

optional_semicolon
  : ';'
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
discipline_declaration
  : TK_discipline GenericIdentifier optional_semicolon
    discipline_items_opt TK_enddiscipline
    { $$ = MakeTaggedNode(N::kDisciplineDeclaration, $1, $2, $3, $4, $5); }
  ;
discipline_items_opt
  : discipline_items
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
discipline_items
  : discipline_items discipline_item
    { $$ = ExtendNode($1, $2); }
  | discipline_item
    { $$ = MakeTaggedNode(N::kDisciplineItemList, $1); }
  ;
discipline_item
  /* discipline_domain_bindings: */
  : TK_domain TK_discrete ';'
    { $$ = MakeTaggedNode(N::kDisciplineDomainBinding, $1, $2, $3); }
  | TK_domain TK_continuous ';'
    { $$ = MakeTaggedNode(N::kDisciplineDomainBinding, $1, $2, $3); }
  /* nature_bindings: potential_or_flow nature_identifier ';' */
  | TK_potential GenericIdentifier ';'
    { $$ = MakeTaggedNode(N::kDisciplinePotential, $1, $2, $3); }
  | TK_flow GenericIdentifier ';'
    { $$ = MakeTaggedNode(N::kDisciplineFlow, $1, $2, $3); }
  /* TODO(fangism): nature_attribute_override */
  ;
nature_declaration
  : TK_nature GenericIdentifier optional_semicolon
    nature_items
    TK_endnature
  ;
nature_items
  : nature_items nature_item
  | nature_item
  ;
nature_item
  : TK_units '=' TK_StringLiteral ';'
  | TK_abstol '=' expression ';'
  | TK_access '=' GenericIdentifier ';'
  | TK_idt_nature '=' GenericIdentifier ';'
  | TK_ddt_nature '=' GenericIdentifier ';'
  ;

config_declaration
  : TK_config GenericIdentifier ';'
    design_statement
    list_of_config_rule_statements_opt
    TK_endconfig label_opt
    { $$ = MakeTaggedNode(N::kConfigDeclaration, $1, $2, $3, $4, $5, $6, $7); }
  ;
design_statement
  : TK_design lib_cell_identifiers_opt ';'
    { $$ = MakeTaggedNode(N::kDesignStatement, $1, $2, $3); }
  ;
lib_cell_identifiers_opt
  : lib_cell_identifiers
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
lib_cell_identifiers
  : lib_cell_identifiers lib_cell_id
    { $$ = ExtendNode($1, $2); }
  | lib_cell_id
    { $$ = MakeTaggedNode(N::kDesignStatementItems, $1); }
  ;
list_of_config_rule_statements_opt
  : list_of_config_rule_statements
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
list_of_config_rule_statements
  : list_of_config_rule_statements config_rule_statement
    { $$ = ExtendNode($1, $2); }
  | config_rule_statement
    { $$ = MakeTaggedNode(N::kConfigRuleStatementList, $1); }
  ;
config_rule_statement
  : TK_default liblist_clause ';'
    { $$ = MakeTaggedNode(N::kConfigRuleStatement, $1, $2, $3); }
  | inst_clause liblist_clause ';'
    { $$ = MakeTaggedNode(N::kConfigRuleStatement, $1, $2, $3); }
  | inst_clause use_clause ';'
    { $$ = MakeTaggedNode(N::kConfigRuleStatement, $1, $2, $3); }
  | cell_clause liblist_clause ';'
    { $$ = MakeTaggedNode(N::kConfigRuleStatement, $1, $2, $3); }
  | cell_clause use_clause ';'
    { $$ = MakeTaggedNode(N::kConfigRuleStatement, $1, $2, $3); }
  | preprocessor_balanced_config_rule_statements
    { $$ = move($1); }
  ;
inst_clause
  : TK_instance reference
    /* $2:reference is instance name */
    { $$ = MakeTaggedNode(N::kInstClause, $1, $2); }
  ;
cell_clause
  : TK_cell lib_cell_id
    { $$ = MakeTaggedNode(N::kCellClause, $1, $2); }
  ;
liblist_clause
  : TK_liblist list_of_libraries_opt
    { $$ = MakeTaggedNode(N::kLiblistClause, $1, $2); }
  ;
use_clause
  : TK_use lib_cell_id opt_config
    { $$ = MakeTaggedNode(N::kUseClause, $1, $2, nullptr, ForwardChildren($3)); }
/* TODO(b/124600414): This has a S/R conflict because ('.' ID) is in both parts.
  Why is there no separator between these in the official grammar??
  | TK_use lib_cell_id named_parameter_assignment_list opt_config
    { $$ = MakeTaggedNode(N::kUseClause, $1, $2, $3, ForwardChildren($3)); }
 */
  | TK_use named_parameter_assignment_list opt_config
    { $$ = MakeTaggedNode(N::kUseClause, $1, nullptr, $2, ForwardChildren($3)); }
  ;

preprocessor_balanced_config_rule_statements
  : preprocessor_if_header list_of_config_rule_statements_opt
    preprocessor_elsif_config_rule_statements_opt
    preprocessor_else_config_rule_statement_opt
    PP_endif
    { $$ = MakeTaggedNode(N::kPreprocessorBalancedConfigRuleStatements,
                          ExtendNode($1, $2), ForwardChildren($3), $4, $5);
    }
  ;
preprocessor_elsif_config_rule_statements_opt
  : preprocessor_elsif_config_rule_statements
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
preprocessor_elsif_config_rule_statements
  : preprocessor_elsif_config_rule_statements
    preprocessor_elsif_config_rule_statement
    { $$ = ExtendNode($1, $2); }
  | preprocessor_elsif_config_rule_statement
    { $$ = MakeNode($1); }  /* Don't bother tagging; node will be flattened. */
  ;
preprocessor_elsif_config_rule_statement
  : preprocessor_elsif_header list_of_config_rule_statements_opt
    { $$ = ExtendNode($1, $2); }
  ;
preprocessor_else_config_rule_statement_opt
  : preprocessor_else_config_rule_statement
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
preprocessor_else_config_rule_statement
  : PP_else list_of_config_rule_statements_opt
    { $$ = MakeTaggedNode(N::kPreprocessorElseClause, $1, $2); }
  ;

named_parameter_assignment_list
  : named_parameter_assignment_list ',' named_parameter_assignment
    { $$ = ExtendNode($1, $2, $3); }
  | named_parameter_assignment
    { $$ = MakeTaggedNode(N::kActualParameterByNameList, $1); }
  ;
named_parameter_assignment
  /* subset of: parameter_value_byname */
  : '.' member_name '(' parameter_expr ')'
    { $$ = MakeTaggedNode(N::kParamByName, $1, $2, MakeParenGroup($3, $4, $5)); }
  | '.' member_name '(' ')'
    { $$ = MakeTaggedNode(N::kParamByName, $1, $2, MakeParenGroup($3, nullptr, $4)); }
  ;
opt_config
  : ':' TK_config
    { $$ = MakeNode($1, $2); }
  | /* empty */
    { $$ = nullptr; }
  ;
lib_cell_id
  : GenericIdentifier
    /* cell_id */
    { $$ = MakeTaggedNode(N::kCellIdentifier, nullptr, nullptr, $1); }
  | GenericIdentifier '.' GenericIdentifier
    /* library_id . cell_id */
    { $$ = MakeTaggedNode(N::kCellIdentifier, $1, $2, $3); }
  ;
list_of_libraries_opt
  : list_of_libraries
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
list_of_libraries
  : list_of_libraries GenericIdentifier
    { $$ = ExtendNode($1, $2); }
  | GenericIdentifier
    { $$ = MakeTaggedNode(N::kIdentifierList, $1); }
    /* could also give this own node type like kLibraryList */
  ;

drive_strength
  : '(' dr_strength0 ',' dr_strength1 ')'
  | '(' dr_strength1 ',' dr_strength0 ')'
  | '(' dr_strength0 ',' TK_highz1 ')'
  | '(' dr_strength1 ',' TK_highz0 ')'
  | '(' TK_highz1 ',' dr_strength0 ')'
  | '(' TK_highz0 ',' dr_strength1 ')'
  ;
drive_strength_opt
  : drive_strength
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
dr_strength0
  : TK_supply0
    { $$ = move($1); }
  | TK_strong0
    { $$ = move($1); }
  | TK_pull0
    { $$ = move($1); }
  | TK_weak0
    { $$ = move($1); }
  ;
dr_strength1
  : TK_supply1
    { $$ = move($1); }
  | TK_strong1
    { $$ = move($1); }
  | TK_pull1
    { $$ = move($1); }
  | TK_weak1
    { $$ = move($1); }
  ;
pull01
  : TK_pull0
    { $$ = move($1); }
  | TK_pull1
    { $$ = move($1); }
  ;
event_control
  : '@' hierarchy_event_identifier
    { $$ = MakeTaggedNode(N::kEventControl, $1, $2);}
  | '@' '(' event_expression_list ')'
    { $$ = MakeTaggedNode(N::kEventControl, $1, MakeParenGroup($2, $3, $4));}
  | '@' '(' '*' ')'
    { $$ = MakeTaggedNode(N::kEventControl, $1, MakeParenGroup($2, $3, $4));}
  | '@' '*'
    { $$ = MakeTaggedNode(N::kEventControl, $1, $2);}
    /* same as @ (*), which the lexer returns as just '*' */
  ;
event_control_opt
  : event_control
    { $$ = move($1); }
  | /*empty*/
    { $$ = nullptr; }
  ;
event_expression_list
  : event_expression
    { $$ = MakeTaggedNode(N::kEventExpressionList, $1); }
  | event_expression_list TK_or event_expression
    { $$ = ExtendNode($1, $2, $3); }
  | event_expression_list ',' event_expression
    { $$ = ExtendNode($1, $2, $3); }
  ;
event_expression
  : edge_operator expression
    { $$ = MakeTaggedNode(N::kEventExpression, $1, $2); }
  | expression
    { $$ = MakeTaggedNode(N::kEventExpression, $1); }
  | edge_operator expression TK_iff expression
    { $$ = MakeTaggedNode(N::kEventExpression, $1, $2, $3, $4); }
  | expression TK_iff expression
    { $$ = MakeTaggedNode(N::kEventExpression, $1, $2, $3); }
  ;
branch_probe_expression
  : GenericIdentifier '(' GenericIdentifier ',' GenericIdentifier ')'
    { $$ = MakeTaggedNode(N::kBranchProbeExpression, $1,
                          MakeParenGroup($2, MakeNode($3, $4, $5), $6)); }
  | GenericIdentifier '(' GenericIdentifier ')'
    { $$ = MakeTaggedNode(N::kBranchProbeExpression, $1,
                          MakeParenGroup($2, $3, $4)); }
  ;

pattern_opt
  : pattern
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
pattern
  : '.' member_name
    { $$ = MakeTaggedNode(N::kPattern, $1, $2); }
  | TK_DOTSTAR
    { $$ = MakeTaggedNode(N::kPattern, $1); }
  | expr_primary_no_groups
    { $$ = MakeTaggedNode(N::kPattern, $1); }
  /* $1 must be a constant expression */
  | TK_tagged GenericIdentifier pattern_opt
    // TODO(jeremycs): consider flattening here
    { $$ = MakeTaggedNode(N::kPattern, $1, $2, $3); }
  | TK_LP pattern_list '}'
    { $$ = MakeTaggedNode(N::kPattern, $1, $2, $3); }
  | TK_LP member_pattern_list '}'
    { $$ = MakeTaggedNode(N::kPattern, $1, $2, $3); }
  ;
pattern_list
  : pattern_list ',' pattern
    { $$ = ExtendNode($1, $2, $3); }
  | pattern
    { $$ = MakeTaggedNode(N::kPatternList, $1); }
  ;
member_pattern
  : GenericIdentifier ':' pattern
    { $$ = MakeTaggedNode(N::kMemberPattern, $1, $2, $3); }
  ;
member_pattern_list
  : member_pattern_list ',' member_pattern
    { $$ = ExtendNode($1, $2, $3); }
  | member_pattern
    { $$ = MakeTaggedNode(N::kMemberPatternList, $1); }
  ;

expression
  : equiv_impl_expr
    { $$ = MakeTaggedNode(N::kExpression, $1);}
  ;

equiv_impl_expr
  : cond_expr
    { $$ = move($1); }
  | cond_expr TK_LOGICAL_IMPLIES equiv_impl_expr
    { $$ = MakeBinaryExpression($1, $2, $3); }
  | cond_expr TK_LOGEQUIV equiv_impl_expr
    { $$ = MakeBinaryExpression($1, $2, $3); }
  ;

/* lowest precedence expression */
cond_expr
  : logor_expr
    { $$ = move($1); }
  | logor_expr '?' expression ':' cond_expr
    { $$ = MakeTaggedNode(N::kTernaryExpression, $1, $2, $3, $4, $5); }
  // | cond_expr '?' cond_expr ':' cond_expr
  // | cond_expr '?' logor_expr ':' logor_expr
  /*
  | expression '?' expression ':' expression
  */
  ;

inc_or_dec_or_primary_expr
  : postfix_expression
    { $$ = move($1); }
  | inc_or_dec_expression
    { $$ = move($1); }
  ;

unary_expr
  : unary_prefix_expr
    { $$ = move($1); }
  ;

unary_prefix_expr
  : inc_or_dec_or_primary_expr
    { $$ = move($1); }
  /* the following section's expr_primary-s were prefixed with attribute_list_opt */
  | unary_op unary_prefix_expr
   { $$ = MakeTaggedNode(N::kUnaryPrefixExpression, $1, $2); }
  ;

unary_op
  : '+'
    { $$ = move($1); }
  | '-'
    { $$ = move($1); }
  | '~'
    { $$ = move($1); }
  | '&'
    { $$ = move($1); }
  | '!'
    { $$ = move($1); }
  | '|'
    { $$ = move($1); }
  | '^'
    { $$ = move($1); }
  | TK_NAND
    { $$ = move($1); }
  | TK_NOR
    { $$ = move($1); }
  | TK_NXOR
    { $$ = move($1); }
  ;

pow_expr
  : unary_expr
    { $$ = move($1); }
  | pow_expr TK_POW unary_expr
    { $$ = MakeBinaryExpression($1, $2, $3); }
  ;

mul_expr
  : pow_expr
    { $$ = move($1); }
  | mul_expr '*' pow_expr
    { $$ = MakeBinaryExpression($1, $2, $3); }
  | mul_expr '/' pow_expr
    { $$ = MakeBinaryExpression($1, $2, $3); }
  | mul_expr '%' pow_expr
    { $$ = MakeBinaryExpression($1, $2, $3); }
  ;

add_expr
  : mul_expr
    { $$ = move($1); }
  | add_expr '+' mul_expr
    { $$ = MakeBinaryExpression($1, $2, $3); }
  | add_expr '-' mul_expr
    { $$ = MakeBinaryExpression($1, $2, $3); }
  ;

shift_expr
  : add_expr
    { $$ = move($1); }
  | shift_expr TK_LS add_expr
    { $$ = MakeBinaryExpression($1, $2, $3); }
  | shift_expr TK_RS add_expr
    { $$ = MakeBinaryExpression($1, $2, $3); }
  | shift_expr TK_RSS add_expr
    { $$ = MakeBinaryExpression($1, $2, $3); }
  ;

comp_expr
  : shift_expr
    { $$ = move($1); }
  | comp_expr '<' shift_expr
    { $$ = MakeBinaryExpression($1, $2, $3); }
  | comp_expr '>' shift_expr
    { $$ = MakeBinaryExpression($1, $2, $3); }
  | comp_expr TK_LE shift_expr
    { $$ = MakeBinaryExpression($1, $2, $3); }
  | comp_expr TK_GE shift_expr
    { $$ = MakeBinaryExpression($1, $2, $3); }
  | comp_expr TK_inside '{' open_range_list '}'
    { //auto group = MakeBraceGroup($3, $4, $5);
      $$ = MakeBinaryExpression($1, $2, MakeBraceGroup($3, $4, $5)); }
    /* inside_expression is for set membership */
  | comp_expr TK_inside reference
    { $$ = MakeBinaryExpression($1, $2, $3); }
    /* This rule is an extension of the LRM rules supported by some vendors. */
  /* TODO(fangism): dist expression has same precedence as this group */
  ;

logeq_expr
  : comp_expr
    { $$ = move($1); }
  | logeq_expr TK_EQ comp_expr
    { $$ = MakeBinaryExpression($1, $2, $3); }
  | logeq_expr TK_NE comp_expr
    { $$ = MakeBinaryExpression($1, $2, $3); }
  | logeq_expr TK_WILDCARD_EQ comp_expr
    { $$ = MakeBinaryExpression($1, $2, $3); }
  | logeq_expr TK_WILDCARD_NE comp_expr
    { $$ = MakeBinaryExpression($1, $2, $3); }

  ;

caseeq_expr
  : logeq_expr
    { $$ = move($1); }
  | caseeq_expr TK_CEQ logeq_expr
    { $$ = MakeBinaryExpression($1, $2, $3); }
  | caseeq_expr TK_CNE logeq_expr
    { $$ = MakeBinaryExpression($1, $2, $3); }
  ;

bitand_expr
  : caseeq_expr
    { $$ = move($1); }
  | bitand_expr '&' caseeq_expr
    { $$ = MakeBinaryExpression($1, $2, $3); }
  | bitand_expr TK_NAND caseeq_expr
    { $$ = MakeBinaryExpression($1, $2, $3); }
  ;

xor_expr
  : bitand_expr
    { $$ = move($1); }
  | xor_expr '^' bitand_expr
    { $$ = MakeBinaryExpression($1, $2, $3); }
  | xor_expr TK_NXOR bitand_expr
    { $$ = MakeBinaryExpression($1, $2, $3); }
  ;

bitor_expr
  : xor_expr
    { $$ = move($1); }
  | bitor_expr '|' xor_expr
    { $$ = MakeBinaryExpression($1, $2, $3); }
  | bitor_expr TK_NOR xor_expr
    { $$ = MakeBinaryExpression($1, $2, $3); }
  ;

with_exprs_suffix
  : with_exprs_suffix with_covergroup_expression_in_parens
    { $$ = MakeTaggedNode(N::kWithGroup, $1, $2); }
  | bitor_expr
    { $$ = move($1); }
  ;

matches_expr
  : with_exprs_suffix
    { $$ = move($1); }
  /* matches_integer_covergroup_expr is a form of select_expression */
  | matches_expr TK_matches with_exprs_suffix
    { $$ = MakeBinaryExpression($1, $2, $3); }

    /* $3 should be expression? */
  ;

logand_expr
  : matches_expr
    { $$ = move($1); }
  | logand_expr TK_LAND bitor_expr
    { $$ = MakeBinaryExpression($1, $2, $3); }
  ;

/* lowest precedence binary expression */
logor_expr
  : logand_expr
    { $$ = move($1); }
  | logor_expr TK_LOR logand_expr
    { $$ = MakeBinaryExpression($1, $2, $3); }
  ;

expr_mintypmax
  /* The typical min-typ-max for is expression ':' expression ':' expression.
   * However, we extend this to support other special forms that appear
   * inside parentheses without conflict.
   * The precedence chosen for these list delimiters is arbitrary.
   * See trans_list (in comments).
   */
  : expr_mintypmax_trans_set
    { $$ = move($1); }
  ;
expr_mintypmax_trans_set
  /* TODO(jeremycs): reconsider decision to flatten here*/
  /* TK_EG is the separator in trans_set */
  : expr_mintypmax_trans_set TK_EG expr_mintypmax_generalized
    { $$ = ExtendNode($1, $2, ForwardChildren($3)); }
  | expr_mintypmax_generalized
    { $$ = IsExpression($1) ? move($1)
                            : MakeTaggedNode(N::kMinTypMaxList, ForwardChildren($1)); }
  ;
expr_mintypmax_generalized
  /* covers original expr_mintypmax : expression ':' expression ':' expression
   * When this is expected, verify that this has 3 items in the 'list'.
   */
  : expr_mintypmax_generalized ':' property_expr_or_assignment_list
    { $$ = ExtendNode($1, $2, ForwardChildren($3)); }
  | property_expr_or_assignment_list  /* ','-separated */
    { $$ = IsExpression($1) ? move($1)
                            : MakeTaggedNode(N::kMinTypMaxList, ForwardChildren($1)); }
  /* for trans_list, each of these can be an open_range_list,
   * for all other contexts, these should be single value_range.
   */
  ;
property_expr_or_assignment_list
  /* covers open_range_list */
  : property_expr_or_assignment_list ',' property_expr_or_assignment
    { $$ = ExtendNode($1, $2, $3); }
  | property_expr_or_assignment
    { $$ = IsExpression($1) ? move($1)
                            : MakeTaggedNode(N::kMinTypMaxList, $1); }
  ;
property_expr_or_assignment
  : property_expr
    { $$ = move($1); }
    /* covers sequence_repetition_expr
     * : expression_or_dist boolean_abbrev_opt
     * covers trans_range_list
     * : open_range_list boolean_abbrev_opt
     * though only the last item should be suffixed, and its boolean_abbrev_opt
     * should be hoisted to become a suffix of the whole open_range_list.
     */
    /* covers simple expression, including inc_or_dec_expression (assignment) */
    /* covers sequence_match_item: subroutine_call */
  | '[' expression ':' expression ']'
    { $$ = MakeTaggedNode(N::kValueRange, $1, $2, $3, $4, $5); }
    /* covers value_range */
  | assignment_statement_no_expr
    { $$ = move($1); }
    /* covers sequence_match_item: assignment_statement */
  ;

argument_list_opt
  : any_argument_list
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
any_argument_list
  : any_argument_list_trailing_comma
    { $$ = move($1); }
  | any_argument_list_preprocessor_last
    { $$ = move($1); }
  | any_argument_list_item_last
    { $$ = move($1); }
  ;
any_argument_list_trailing_comma
  : any_argument_list ','
    { $$ = ExtendNode($1, $2); }
  | ','
    { $$ = MakeTaggedNode(N::kArgumentList, $1); }
  ;
any_argument_list_item_last
  /* TODO(fangism): positional arguments must appear before named arguments,
   * except inside odd uses of preprocessing directives.
   */
  : any_argument
    { $$ = MakeTaggedNode(N::kArgumentList, $1); }
  /* comma separating arguments is required,
   * except after preprocessor directive
   */
  | any_argument_list_trailing_comma any_argument
    { $$ = ExtendNode($1, $2); }
  | any_argument_list_preprocessor_last any_argument
    { $$ = ExtendNode($1, $2); }
  ;
any_argument_list_preprocessor_last
  : preprocessor_directive
    { $$ = MakeTaggedNode(N::kArgumentList, $1); }
  /* comma separating preprocessor_directive is optional */
  | any_argument_list preprocessor_directive
    { $$ = ExtendNode($1, $2); }
  ;
any_argument
  /* similar to parameter_expr */
  : expression /* positional argument */
    { $$ = move($1); }
  | data_type_primitive
    { $$ = move($1); }
  | event_control
    { $$ = move($1); }
    /* Special functions like $past() take an event_control argument. */
  | parameter_value_byname /* named argument */
    { $$ = move($1); }
  ;

expression_or_null_list_opt
  : expression_or_null_list_opt ',' expression_opt
    { $$ = ExtendNode($1, $2, $3); }
  | expression_opt
    { $$ = MakeTaggedNode(N::kExpressionList, $1); }
  ;

expression_opt
  : expression
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;

expression_list_proper
  : expression_list_proper ',' expression
    { $$ = ExtendNode($1, $2, $3); }
  | expression
    { $$ = MakeTaggedNode(N::kExpressionList, $1); }
  ;

scope_prefix
  : GenericIdentifier TK_SCOPE_RES
    { $$ = MakeTaggedNode(N::kScopePrefix, $1, $2); }
  | TK_Sunit TK_SCOPE_RES
    { $$ = MakeTaggedNode(N::kScopePrefix, $1, $2); }
  ;

postfix_expression
  : reference_or_call
    { $$ = move($1); }
  | expr_primary
    { $$ = move($1); }
  ;

reference_or_call
  : local_root
    { $$ = MakeTaggedNode(N::kReferenceCallBase,
                          MakeTaggedNode(N::kReference, $1)); }
    /* base of reference, including GenericIdentifier */
  | local_root '(' argument_list_opt ')'
    { $$ = MakeTaggedNode(N::kReferenceCallBase,
                          MakeTaggedNode(N::kFunctionCall, $1, MakeParenGroup($2, $3, $4))); }
    /* subroutine call */
  | reference_or_call hierarchy_or_call_extension
    { $$ = ExtendNode($1, $2); }
    /* . member */
    /* or method call */
  | reference_or_call select_variable_dimension
    { $$ = ExtendNode($1, $2); }
    /* [ range ] */
  ;

reference
  /* Replaces former "scoped_hierarchy_identifier" */
  : local_root
    { $$ = MakeTaggedNode(N::kReference, $1); }
    /* base of reference, including GenericIdentifier */
  | reference hierarchy_extension
    { $$ = ExtendNode($1, $2); }
    /* . member */
  | reference select_variable_dimension
    { $$ = ExtendNode($1, $2); }
    /* [ range ] */
  ;

/* highest precedence expressions */
expr_primary
  /* split up as such to make it easier to work around conflicts */
  : expr_primary_no_groups
    { $$ = move($1); }

  // | '(' expr_mintypmax ')'
  //   { $$ = MakeParenGroup($1, $2, $3); }
  | expr_primary_parens
   { $$ = move($1); }
  | expr_primary_braces
    { $$ = move($1); }
  | assignment_pattern
    { $$ = move($1); }
  ;
expr_primary_parens
  : '(' expr_mintypmax ')'
    {  $$ = MakeParenGroup($1, $2, $3); }
  ;
expr_primary_braces
  : '{' '}'
    { $$ = MakeTaggedNode(N::kExpression, MakeBraceGroup($1, nullptr, $2)); }
  | '{' value_range '{' expression_list_proper '}' '}'
    { $$ = MakeTaggedNode(N::kExpression, $1, $2, MakeBraceGroup($3, $4, $5), $6); }
    /* repeat concatenation: $2 should be an expression, not a range */
  | range_list_in_braces
    { $$ = MakeTaggedNode(N::kExpression, $1); }
  | streaming_concatenation
    { $$ = MakeTaggedNode(N::kExpression, $1); }

  ;
range_list_in_braces
  : '{' open_range_list '}'
     { $$ = MakeBraceGroup($1, $2, $3);  }
    /* for SystemVerilog covergroups, open_range_list serves as covergroup_range_list. */
    /* list of ranges also covers list of expressions, so to eliminate R/R conflict
     * we've removed expression_list_proper:
  | '{' expression_list_proper '}'
     */
  ;

/* TODO(jeremycs): unclear how much of this structure is actually needed */
type_or_id_root
  : class_id
    { $$ = move($1); }
  | implicit_class_handle
    { $$ = move($1); }
  ;
local_root
  : TK_local_SCOPE type_or_id_root
    { $$ = MakeTaggedNode(N::kLocalRoot, $1, $2); }
    /* 'local' is a legal class_qualifier in the scope of an
     * inline constraint block.
     */
  | TK_Sroot '.' type_or_id_root
    { $$ = MakeTaggedNode(N::kLocalRoot, $1, $2, $3); }
  | type_or_id_root
    { $$ = MakeTaggedNode(N::kLocalRoot, $1); }
  /* The following is now covered by allowing '.' class_id in
   * hierarchy_extension.
   *   implicit_class_handle '.' class_id
   */
  | MacroCall
    { $$ = move($1); }

  ;

string_literal
  : TK_StringLiteral
    { $$ = move($1); }
  | TK_EvalStringLiteral
    { $$ = move($1); }
  ;

expr_primary_no_groups
  : number
    { $$ = move($1); }
  | TK_RealTime
    { $$ = move($1); }
  // allow time literals in delay1 expressions,
  //   e.g. #(10ps) or #(N *5ps)
  | TK_TimeLiteral
    { $$ = move($1); }
  | string_literal
    { $$ = move($1); }
  | cast
    { $$ = move($1); }
  | randomize_call
    { $$ = move($1); }
  | select_condition  /* for covergroups */
    { $$ = move($1); }
  | '$'
    { $$ = move($1); }
    /* '$' is used to index into queues */
  | TK_null
    { $$ = move($1); }
  | system_tf_call
    { $$ = move($1); }
  | type_reference
    { $$ = move($1); }
  | MacroGenericItem
    { $$ = move($1); }
    /* This is useful for when there is an expression or reference
     * on its own line, at the end of an argument list.
     */
  ;
cast
  : casting_type '\'' '(' expression ')'
    { $$ = MakeTaggedNode(N::kCast, $1, $2, MakeParenGroup($3, $4, $5)); }
  ;
casting_type
  : TK_DecNumber
    { $$ = move($1); }
    /* TODO(fangism): This should be a constant_primary. */
  | expr_primary_parens
    { $$ = move($1); }
  | system_tf_call
    { $$ = move($1); }
    /* for constant functions like $bits and $clog2 */
  | data_type_base
    { $$ = move($1); }
    /* $1 should be limited to 'simple_type', according to the LRM. */
  | signing
    { $$ = move($1); }
  | TK_const
    { $$ = move($1); }
  /* covered by data_type_base:
  | TK_string
  */
  ;
randomize_call
  : TK_randomize /* attribute_list_opt */
    identifier_list_in_parens_opt
    with_constraint_block_opt
    { $$ = MakeTaggedNode(N::kRandomizeFunctionCall, $1, $2, $3); }
  /* Another form of randomize_call is as a member function, like foo.randomize().
   * This is covered by reference_or_call.
   */
  ;
identifier_list_in_parens_opt
  : '(' identifier_list ')'
    { $$ = MakeParenGroup($1, $2, $3); }
    /* variable identifier list */
  | '(' TK_null ')'
    { $$ = MakeParenGroup($1, $2, $3); }
  | '(' ')'
    { $$ = MakeParenGroup($1, nullptr, $2); }
  | /* empty */
  ;
identifier_list
  /* LRM:18.7 says list elements should be simple variable identifiers,
   * but it seems that some tools allow indexed references.
   */
  : identifier_list ',' reference
    { $$ = ExtendNode($1, $2, $3); }
  | reference
    { $$ = MakeTaggedNode(N::kIdentifierList, $1); }
  ;
with_constraint_block_opt
  : with_constraint_block
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
with_constraint_block
  : TK_with identifier_list_in_parens_opt constraint_block
    { $$ = MakeTaggedNode(N::kWithConstraints, $1, $2, $3); }
  ;

unary_builtin_function
  : TK_acos
    { $$ = move($1); }
  | TK_acosh
    { $$ = move($1); }
  | TK_asin
    { $$ = move($1); }
  | TK_asinh
    { $$ = move($1); }
  | TK_atan
    { $$ = move($1); }
  | TK_atanh
    { $$ = move($1); }
  | TK_ceil
    { $$ = move($1); }
  | TK_cos
    { $$ = move($1); }
  | TK_cosh
    { $$ = move($1); }
  | TK_exp
    { $$ = move($1); }
  | TK_floor
    { $$ = move($1); }
  | TK_ln
    { $$ = move($1); }
  | TK_log
    { $$ = move($1); }
  | TK_sin
    { $$ = move($1); }
  | TK_sinh
    { $$ = move($1); }
  | TK_sqrt
    { $$ = move($1); }
  | TK_tan
    { $$ = move($1); }
  | TK_tanh
    { $$ = move($1); }
  | TK_abs
    { $$ = move($1); }
  ;
binary_builtin_function
  : TK_atan2
    { $$ = move($1); }
  | TK_hypot
    { $$ = move($1); }
  | TK_pow
    { $$ = move($1); }
  /* In SystemVerilog these are built-in array-locator methods.
  | TK_max
  | TK_min
  */
  ;

/**
function_item_list_opt
  : function_item_list
  | // empty
  ;
**/
function_item_list
  : function_item
    { $$ = MakeTaggedNode(N::kFunctionItemList, $1); }
  | function_item_list function_item
    { $$ = ExtendNode($1, $2); }
  ;
function_item
  : tf_port_declaration
    { $$ = move($1); }
  | block_item_decl
    { $$ = move($1); }
  ;

/* primitive_gate_instance covers the following gate_instantiation constructs
 * from the LRM:
 *   cmos_switch_instance
 *   enable_gate_instance
 *   mos_switch_instance
 *   n_input_gate_instance
 *   n_output_gate_instance
 *   pass_switch_instance
 *   pass_enable_switch_instance
 *   pull_gate_instance
 * The number of arguments for each of these instances is not checked here.
 */
primitive_gate_instance
  : GenericIdentifier decl_dimensions_opt '(' any_port_list_opt ')'
    /* $1, $2 := name_of_instance
                 : instance_identifier { unpacked_dimension }
      */
    { $$ = MakeTaggedNode(N::kPrimitiveGateInstance, $1,
                          MakeUnpackedDimensionsNode($2),
                          MakeParenGroup($3, $4, $5)); }
  /* anonymous instance */
  | '(' any_port_list_opt ')'
    { $$ = MakeTaggedNode(N::kPrimitiveGateInstance, nullptr, nullptr, MakeParenGroup($1, $2, $3)); }
  | GenericIdentifier decl_dimensions
    { $$ = MakeTaggedNode(N::kPrimitiveGateInstance, $1,
                          MakeUnpackedDimensionsNode($2),
                          nullptr); }
  ;
primitive_gate_instance_list
  : primitive_gate_instance_list ',' primitive_gate_instance
    { $$ = ExtendNode($1, $2, $3); }
  | primitive_gate_instance
    { $$ = MakeTaggedNode(N::kPrimitiveGateInstanceList, $1); }
  ;

// more permissive to eliminate S/R conflict
// TODO(fangism): Reject the unpacked dimensions in these constructs.
gate_instance_or_register_variable
  /* similar to variable_decl_assignment */
  : GenericIdentifier decl_dimensions_opt trailing_decl_assignment_opt
    { $$ = MakeTaggedNode(N::kRegisterVariable, $1,
                          MakeUnpackedDimensionsNode($2), $3); }
  | GenericIdentifier decl_dimensions_opt '(' any_port_list_opt ')'
    { $$ = MakeTaggedNode(N::kGateInstance, $1,
                          MakeUnpackedDimensionsNode($2),
                          MakeParenGroup($3, $4, $5)); }
  | MacroCall
    /* covers MacroCallId '(' ... ')' as an instance */
    /* TODO(fangism): restructure this like a kGateInstance */
    { $$ = move($1); }
  /* TODO(fangism): arrays should not be declared with port connections */
  /* TODO(b/36706412): support anonymous instances */
  // | '(' any_port_list_opt ')'
  ;
gate_instance_or_register_variable_list
  : gate_instance_or_register_variable_list ',' gate_instance_or_register_variable
    { $$ = ExtendNode($1, $2, $3); }
  | gate_instance_or_register_variable
    { $$ = MakeTaggedNode(N::kGateInstanceRegisterVariableList, $1); }
  ;

gatetype
  : TK_and
    { $$ = move($1); }
  | TK_nand
    { $$ = move($1); }
  | TK_or
    { $$ = move($1); }
  | TK_nor
    { $$ = move($1); }
  | TK_xor
    { $$ = move($1); }
  | TK_xnor
    { $$ = move($1); }
  | TK_buf
    { $$ = move($1); }
  | TK_bufif0
    { $$ = move($1); }
  | TK_bufif1
    { $$ = move($1); }
  | TK_not
    { $$ = move($1); }
  | TK_notif0
    { $$ = move($1); }
  | TK_notif1
    { $$ = move($1); }
  ;
switchtype
  : TK_nmos
  | TK_rnmos
  | TK_pmos
  | TK_rpmos
  | TK_cmos
  | TK_rcmos
  | TK_tran
  | TK_rtran
  | TK_tranif0
  | TK_tranif1
  | TK_rtranif0
  | TK_rtranif1
  ;


hierarchy_extension
  : '.' class_id
    { $$ = MakeTaggedNode(N::kHierarchyExtension, $1, $2); }
  /* TODO(fangism): Most of the time, $2 should be a GenericIdentifier;
   * a qualified class_id should only be permitted when preceded by
   * implicit_class_handle.
   * (This happens to resemble C++-syntax: obj.foo::bar .)
   */
  | '.' MacroCall
    { $$ = MakeTaggedNode(N::kMacroCallExtension, $1, $2);}
  ;
hierarchy_or_call_extension
  : '.' class_id
    { $$ = MakeTaggedNode(N::kHierarchyExtension, $1, $2); }
  | '.' class_id '(' argument_list_opt ')'
    { $$ = MakeTaggedNode(N::kMethodCallExtension, $1, $2, MakeParenGroup($3, $4, $5)); }
  | '.' MacroCall
    { $$ = MakeTaggedNode(N::kMacroCallExtension, $1, $2); }
  /* Special functions like 'new' and 'randomize' should only ever appear
   * at end of any hierarchical reference.
   */
  | '.' TK_new
    { $$ = MakeTaggedNode(N::kNewCall, $1, $2); }
  | '.' TK_new '(' argument_list_opt ')'
    { $$ = MakeTaggedNode(N::kNewCall, $1, $2, MakeParenGroup($3, $4, $5)); }
  | '.' TK_randomize '(' argument_list_opt ')' with_constraint_block_opt
    { $$ = MakeTaggedNode(N::kRandomizeMethodCallExtension, $1,
                          $2,
                          MakeParenGroup($3, $4, $5), $6); }

    /* Extra layers are created here to make the call to randomize appear as
     * any other arbitrary function call.
     */
  | '.' TK_randomize with_constraint_block_opt
    { $$ = MakeTaggedNode(N::kRandomizeMethodCallExtension, $1,
                          $2,
                          nullptr, $3); }
    /* member function form of randomize_call */
  | '.' builtin_array_method '(' argument_list_opt ')'
    array_method_with_predicate_opt
    { $$ = MakeTaggedNode(N::kBuiltinArrayMethodCallExtension, $1, $2, MakeParenGroup($3, $4, $5), $6); }
  | '.' builtin_array_method
    array_method_with_predicate_opt
    { $$ = MakeTaggedNode(N::kBuiltinArrayMethodCallExtension, $1, $2, $3); }
  ;
/** this was merged into variable_dimension: (eliminate conflict on '[' )
index_extension
  : '[' expression ']'
  | '[' expression ':' expression ']'
  | '[' expression TK_PO_POS expression ']'
  | '[' expression TK_PO_NEG expression ']'
  ;
**/
builtin_array_method
  : array_locator_method
    { $$ = move($1); }
  | array_ordering_method
    { $$ = move($1); }
  | array_reduction_method
    { $$ = move($1); }
  ;
array_locator_method
  /* built-in method calls */
  : TK_find
    { $$ = move($1); }
  | TK_find_index
    { $$ = move($1); }
  | TK_find_first
    { $$ = move($1); }
  | TK_find_first_index
    { $$ = move($1); }
  | TK_find_last
    { $$ = move($1); }
  | TK_find_last_index
    { $$ = move($1); }
  | TK_unique
    { $$ = move($1); }
  | TK_unique_index
    { $$ = move($1); }
  | TK_min
    { $$ = move($1); }
  | TK_max
    { $$ = move($1); }
  ;
array_ordering_method
  : TK_sort
    { $$ = move($1); }
  | TK_rsort
    { $$ = move($1); }
  | TK_reverse
    { $$ = move($1); }
  | TK_shuffle
    { $$ = move($1); }
  ;
array_reduction_method
  : TK_sum
    { $$ = move($1); }
  | TK_product
    { $$ = move($1); }
  | TK_and
    { $$ = move($1); }
  | TK_or
    { $$ = move($1); }
  | TK_xor
    { $$ = move($1); }
  ;
array_method_with_predicate_opt
  : TK_with '(' expression ')'
    { $$ = MakeTaggedNode(N::kArrayWithPredicate, $1, MakeParenGroup($2, $3, $4)); }
  | /* empty */
    { $$ = nullptr; }
  ;

hierarchy_event_identifier
  : hierarchy_event_identifier '.' hierarchy_segment
    { $$ = ExtendNode($1, $2, $3); }
  | hierarchy_segment
    { $$ = MakeTaggedNode(N::kHierarchySegmentList, $1); }
  ;
hierarchy_segment
  : GenericIdentifier select_dimensions_opt
    { $$ = MakeTaggedNode(N::kHierarchySegment, $1, $2); }
  ;

list_of_identifiers
  : GenericIdentifier
    { $$ = MakeTaggedNode(N::kIdentifierList, $1); }
  | list_of_identifiers ',' GenericIdentifier
    { $$ = ExtendNode($1, $2, $3); }
  ;
list_of_identifiers_unpacked_dimensions
  : list_of_identifiers_unpacked_dimensions ',' identifier_optional_unpacked_dimensions
    { $$ = ExtendNode($1, $2, $3); }
  | identifier_optional_unpacked_dimensions
    { $$ = MakeTaggedNode(N::kIdentifierUnpackedDimensionsList, $1); }
  ;
identifier_optional_unpacked_dimensions
  : GenericIdentifier decl_dimensions_opt
    { $$ = MakeTaggedNode(N::kIdentifierUnpackedDimensions, $1,
                          MakeUnpackedDimensionsNode($2)); }
  ;
list_of_module_item_identifiers
  /* Workaround for grammatic conflict on implicitly typed port declarations:
   * Allow first item in list to reduce to an unqualified_id.
   * Subsequent items should be GenericIdentifiers.
   */
  : list_of_module_item_identifiers ',' identifier_optional_unpacked_dimensions
    { $$ = ExtendNode($1, $2, $3); }
  | unqualified_id decl_dimensions_opt
    { $$ = MakeTaggedNode(N::kIdentifierList,
                          MakeTaggedNode(N::kIdentifierUnpackedDimensions, $1,
                                         MakeTaggedNode(N::kUnpackedDimensions,
                                                        $2))); }
  /* TODO(fangism): Verify $1 is a bare GenericIdentifer, no parameters. */
  ;
list_of_port_identifiers
  /* TODO(fangism): This probably needs decl_dimensions_opt after identifiers. */
  : GenericIdentifier
    { $$ = MakeTaggedNode(N::kPortIdentifierList,
                          MakeTaggedNode(N::kPortIdentifier, $1)); }
  | GenericIdentifier '=' expression
    /* port identifier with '=' default value */
    { $$ = MakeTaggedNode(N::kPortIdentifierList,
                          MakeTaggedNode(N::kPortIdentifier, $1, $2, $3)); }
  | list_of_port_identifiers ',' GenericIdentifier
    { $$ = ExtendNode($1, $2, MakeTaggedNode(N::kPortIdentifier, $3)); }
  | list_of_port_identifiers ',' GenericIdentifier '=' expression
    /* port identifier with '=' default value */
    { $$ = ExtendNode($1, $2, MakeTaggedNode(N::kPortIdentifier, $3, $4, $5)); }
  ;

identifier_opt
  : GenericIdentifier
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;

/* Inside preprocesor conditionals only, allow trailing comma */
preprocessor_list_of_ports_or_port_declarations_opt
  : list_of_ports_or_port_declarations_opt
    { $$ = move($1); }
  | list_of_ports_or_port_declarations_trailing_comma
    { $$ = move($1); }
  ;
list_of_ports_or_port_declarations_opt
  : list_of_ports_or_port_declarations
    { $$ = move($1); }
  | /* empty */
    { $$ = MakeTaggedNode(N::kPortDeclarationList); }
  ;
list_of_ports_or_port_declarations
  /* This serves as list_of_ports or list_of_port_declarations.
   * The LRM grammar does not permit mixing the two styles of lists (ANSI and
   * non-ANSI), but combining them was necessary to accommodate preprocessing
   * directives without ambiguity.
   */
  : list_of_ports_or_port_declarations_preprocessor_last
    { $$ = move($1); }
  | list_of_ports_or_port_declarations_item_last
    { $$ = move($1); }
  ;
list_of_ports_or_port_declarations_preprocessor_last
  : list_of_ports_or_port_declarations
    preprocessor_balanced_port_declarations
    { $$ = ExtendNode($1, $2); }
  | list_of_ports_or_port_declarations_trailing_comma
    preprocessor_balanced_port_declarations
    { $$ = ExtendNode($1, $2); }
  | preprocessor_balanced_port_declarations
    { $$ = MakeTaggedNode(N::kPortDeclarationList, $1); }
  ;
list_of_ports_or_port_declarations_item_last
  : list_of_ports_or_port_declarations_preprocessor_last port_or_port_declaration
    { $$ = ExtendNode($1, $2); }
  | list_of_ports_or_port_declarations_trailing_comma port_or_port_declaration
    { $$ = ExtendNode($1, $2); }
  | port_or_port_declaration
    { $$ = MakeTaggedNode(N::kPortDeclarationList, $1); }
  ;
list_of_ports_or_port_declarations_trailing_comma
  : list_of_ports_or_port_declarations ','
    { $$ = ExtendNode($1, $2); }
  ;
port_or_port_declaration
  : port
    { $$ = move($1); }
  | port_declaration
    { $$ = move($1); }
  /** The following has been incorporated into the 'port' rule:
  | GenericIdentifier trailing_assign_opt
  **/
  ;

preprocessor_balanced_port_declarations
  : preprocessor_if_header preprocessor_list_of_ports_or_port_declarations_opt
    preprocessor_elsif_port_declarations_opt
    preprocessor_else_port_declarations_opt
    PP_endif
    { $$ = MakeTaggedNode(N::kPreprocessorBalancedPortDeclarations,
                          ExtendNode($1, $2), ForwardChildren($3), $4, $5);
    }
  | MacroGenericItem
    { $$ = move($1); }
  | preprocessor_action
    { $$ = move($1); }
  ;
preprocessor_elsif_port_declarations_opt
  : preprocessor_elsif_port_declarations
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
preprocessor_elsif_port_declarations
  : preprocessor_elsif_port_declarations preprocessor_elsif_port_declaration
    { $$ = ExtendNode($1, $2); }
  | preprocessor_elsif_port_declaration
    { $$ = MakeNode($1); }  /* Don't bother tagging; node will be flattened. */
  ;
preprocessor_elsif_port_declaration
  : preprocessor_elsif_header preprocessor_list_of_ports_or_port_declarations_opt
    { $$ = ExtendNode($1, $2); }
  ;
preprocessor_else_port_declarations_opt
  : preprocessor_else_port_declarations
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
preprocessor_else_port_declarations
  : PP_else preprocessor_list_of_ports_or_port_declarations_opt
    { $$ = MakeTaggedNode(N::kPreprocessorElseClause, $1, $2); }
  ;

/** list_of_port_or_port_declarations combines the following rules:
list_of_ports
  : port_opt
  | list_of_ports ',' port_opt
  ;

list_of_port_declarations
  : list_of_port_declarations ',' port_declaration
  | list_of_port_declarations ',' GenericIdentifier trailing_assign_opt
  | port_declaration
  ;
**/

dir
  : TK_input
    { $$ = move($1); }
  | TK_output
    { $$ = move($1); }
  | TK_inout
    { $$ = move($1); }
  ;

port_declaration
  : /* attribute_list_opt */ port_declaration_noattr
  { $$ = move($1); }
  ;
port_declaration_noattr
  /* should consist of:
   *   inout_declaration
   *   input_declaration
   *   output_declaration
   *   ref_declaration
   *   interface_port_declaration
   */
  // TODO(jeremycs): make this look more like type_identifier_followed_by_ ... rules
  /* originally: data_type_or_implicit, but restricted to resolve conflict */
  // : dir var_or_net_type_opt data_type_or_implicit GenericIdentifier decl_dimensions_opt
  // | dir var_or_net_type_opt data_type_or_implicit GenericIdentifier '=' expression
  //
  // NodekPortDeclaration will have children in the following format:
  // dir, var_or_net_type_opt, data_type (includes packed dimensions), id,
  // unpacked dimensions, trailing_assign_opt
  //
  : port_direction var_or_net_type_opt
    data_type_or_implicit_basic_followed_by_id_and_dimensions_opt
    trailing_assign_opt
    { $$ = MakeTaggedNode(N::kPortDeclaration, $1, $2, ForwardChildren($3), $4); }
    // TODO(fangism): inout's cannot have variable port types,
    // so this needs to be enforced in CST validation.
  | net_type data_type_or_implicit_basic_followed_by_id_and_dimensions_opt
    trailing_assign_opt
    { $$ = MakeTaggedNode(N::kPortDeclaration, nullptr, $1,
                          ForwardChildren($2), $3); }
  | port_direction TK_wreal GenericIdentifier trailing_assign_opt
    { $$ = MakeTaggedNode(N::kPortDeclaration, $1, nullptr, $2, $3, nullptr, $4); }
  | data_type_primitive GenericIdentifier decl_dimensions_opt trailing_assign_opt
    { $$ = MakeTaggedNode(N::kPortDeclaration, nullptr, nullptr,
                          // just expand without ForwardChildren:
                          // MakeTypeIdDimensionsTuple(
                              MakeTaggedNode(N::kDataType, $1,
                                             nullptr /* packed dimensions */),
                              MakeTaggedNode(N::kUnqualifiedId, $2),
                              MakeUnpackedDimensionsNode($3)
                          // )
                          ,  //
                          $4); }
  /* user-defined types: including interface_port_declaration */
  | type_identifier_followed_by_id decl_dimensions_opt trailing_assign_opt
    { $$ = MakeTaggedNode(N::kPortDeclaration, nullptr, nullptr, ForwardChildren($1),
                          MakeUnpackedDimensionsNode($2), $3); }
  ;
var_or_net_type_opt
  : net_type
    { $$ = move($1); }
  | TK_var
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;

signed_unsigned_opt
  : signing
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
lpvalue
  : reference_or_call
    { $$ = MakeTaggedNode(N::kLPValue, $1); }
    /* Unless functions can return by reference, calls should not be permitted
     * in lvalues.  Written this way to avoid R/R conflict against expressions.
     * TODO(fangism): Reject any () calls in $1.
     */
  | range_list_in_braces
    { $$ = MakeTaggedNode(N::kLPValue, $1); }
  /* TODO(fangism): For lpvalue, verify that $1 is of the form
   * '{' expression_list_proper '}' and that each item in the list is an lvalue.
   */
  | streaming_concatenation
    { $$ = MakeTaggedNode(N::kLPValue, $1); }
  ;
cont_assign
  : lpvalue '=' expression
    { $$ = MakeTaggedNode(N::kNetVariableAssignment, $1, $2, $3); }

  // FIXME: allow just lpvalue to permit just reference for MacroCall
  ;
cont_assign_list
  : cont_assign_list ',' cont_assign
    { $$ = ExtendNode($1, $2, $3); }
  | cont_assign
    { $$ = MakeTaggedNode(N::kAssignmentList, $1); }
  ;

symbol_or_label
  : GenericIdentifier
    { $$ = move($1); }
  | MacroIdItem
    { $$ = move($1); }
  ;

module_or_interface_declaration
  /* combined module and interface declarations because they are so similar */
  : /* attribute_list_opt */
    module_start lifetime_opt symbol_or_label
    module_package_import_list_opt
    module_parameter_port_list_opt
    module_port_list_opt
    module_attribute_foreign_opt ';'
    /* local_timeunit_prec_decl_opt */ /* merged into module_item */
    module_item_list_opt
    module_end
    label_opt
    { const auto node_enum = DeclarationKeywordToNodeEnum(*$1);
      $$ = MakeTaggedNode(node_enum,
                          MakeModuleHeader($1, $2, $3, $4, $5, $6, $7, $8),
                          $9, $10, $11); }
  /* TODO(fangism): check that module_start and module_end match */
  /* TODO(fangism): extern {module,interface,program} declarations, ANSI and non-ANSI */
  ;

module_start
  : TK_module
    { $$ = move($1); }
  | TK_macromodule
    { $$ = move($1); }
  | TK_program
    { $$ = move($1); }
  | TK_interface
    { $$ = move($1); }
  ;

module_end
  : TK_endmodule
    { $$ = move($1); }
  | TK_endprogram
    { $$ = move($1); }
  | TK_endinterface
    { $$ = move($1); }
  ;
label_opt
  : ':' symbol_or_label
    { $$ = MakeTaggedNode(N::kLabel, $1, $2); }
  | /* empty */
    { $$ = nullptr; }
  ;
module_attribute_foreign
  : TK_PSTAR GenericIdentifier TK_integer GenericIdentifier '=' TK_StringLiteral ';' TK_STARP
    { $$ = MakeTaggedNode(N::kModuleAttributeForeign, $1, $2, $3, $4, $5, $6, $7, $8); }
  ;
module_attribute_foreign_opt
  : module_attribute_foreign
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
module_port_list_opt
  : '(' list_of_ports_or_port_declarations_opt ')'
    { $$ = MakeParenGroup($1, $2, $3); }
  /** replaces these rules:
  | '(' list_of_ports ')'
  | '(' list_of_port_declarations ')'
  **/
  | /* empty */
    { $$ = nullptr; }
  ;
module_parameter_port_list_opt
  : '#' '(' module_parameter_port_list ')'
  { $$ = MakeTaggedNode(N::kFormalParameterListDeclaration, $1, MakeParenGroup($2, $3, $4)); }
  | '#' '(' ')'
  { $$ = MakeTaggedNode(N::kFormalParameterListDeclaration, $1, MakeParenGroup($2, nullptr, $3)); }
  | /* empty */
  { $$ = nullptr; }
  ;

parameter_opt
  : TK_parameter
    { $$ = move($1); }
  | TK_localparam
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
module_parameter_port
  /* : parameter_opt param_type_opt parameter_assign */
  : parameter_opt param_type_followed_by_id_and_dimensions_opt
    trailing_assign_opt
    { $$ = MakeTaggedNode(N::kParamDeclaration, $1, $2, $3); }
    /* TODO(fangism): Verify that $2 should not have trailing dimensions (in this context). */
  | parameter_opt TK_type type_assignment
    { $$ = MakeTaggedNode(N::kParamDeclaration, $1, $2, $3); }
  /* type parameter:
   * The EBNF permits a type_assignment_list, however, since both
   * type_assignment_list and module_parameter_port_list are comma-separated,
   * 1-token lookahead is insufficient to decide S/R on ',', thus we limit
   * each type parameter declaration to one assignment.
   * TODO(fangism): Refactor grammar to better allow comma-separated
   * parameter multi-declarations.
   */
  /* The keyword 'parameter' is optional in formal parameter lists (such as
   * ANSI-style header declarations), but it is good for readability.
   */
  ;

type_assignment_list
  : type_assignment_list ',' type_assignment
    { $$ = ExtendNode($1, $2, $3); }
  | type_assignment
    { $$ = MakeTaggedNode(N::kTypeAssignmentList, $1); }
  ;
type_assignment
  : GenericIdentifier '=' parameter_expr
    { $$ = MakeTaggedNode(N::kTypeAssignment, $1, $2, $3); }
    /* $3 covers all types, including interface types and user-defined types */
  | GenericIdentifier
    { $$ = MakeTaggedNode(N::kTypeAssignment, $1, nullptr, nullptr); }

  ;

module_parameter_port_list
  /* expanded to allow preprocessing directives like `ifdef */
  : module_parameter_port_list_item_last
    { $$ = move($1); }
  | module_parameter_port_list_preprocessor_last
    { $$ = move($1); }
  ;
module_parameter_port_list_trailing_comma
  : module_parameter_port_list ','
    { $$ = ExtendNode($1, $2); }
  ;
module_parameter_port_list_preprocessor_last
  : module_parameter_port_list preprocessor_directive
    { $$ = ExtendNode($1, $2); }
  | module_parameter_port_list_trailing_comma preprocessor_directive
    { $$ = ExtendNode($1, $2); }
  | preprocessor_directive
    { $$ = MakeTaggedNode(N::kFormalParameterList, $1); }
  ;
module_parameter_port_list_item_last
  /* default value is mandatory */
  : module_parameter_port_list_trailing_comma module_parameter_port
    { $$ = ExtendNode($1, $2); }
  | module_parameter_port_list_preprocessor_last module_parameter_port
    { $$ = ExtendNode($1, $2); }
  | module_parameter_port
    { $$ = MakeTaggedNode(N::kFormalParameterList, $1); }
  ;

net_declaration
  : net_type net_variable_or_decl_assigns ';'
    { $$ = MakeTaggedNode(N::kNetDeclaration, $1, nullptr, $2, $3); }
  | net_type data_type_or_implicit net_variable_or_decl_assigns ';'
    { $$ = MakeTaggedNode(N::kNetDeclaration, $1, $2, $3, $4); }
    /* TODO(fangism): support drive_strength and charge_strength */
  // : net_type data_type_or_implicit delay3_opt net_variable_list ';'
  // : net_type data_type_or_implicit delay3 net_variable_list ';'
  // : net_type data_type_or_implicit_followed_by_id
  // | net_type data_type_or_implicit delay3_opt net_decl_assigns ';'
  // | net_type data_type_or_implicit delay3 net_decl_assigns ';'
  // | net_type data_type_or_implicit_followed_by_id
  //   trailing_assign_opt ';'
  // | net_type data_type_or_implicit_followed_by_id
  //   trailing_assign_opt ',' net_decl_assigns ';'
  // | net_type data_type_or_implicit drive_strength net_decl_assigns ';'
  | TK_trireg charge_strength_opt decl_dimensions_opt delay3_opt list_of_identifiers ';'
    { $$ = MakeTaggedNode(N::kNetDeclaration, $1, $2,
                          MakePackedDimensionsNode($3),
                          $4, $5, $6); }
  /* TODO(fangism): net_type_identifer [ delay_control ] list_of_net_decl_assignments */
  /* TODO(fangism): TK_interconnect ... */
  ;

module_port_declaration
  // TODO(fangism): add more structure here, e.g. kDataType.
  /* In the LRM, this is ansi_port_declaration.
   * Any of these could be prefixed with attribute_list_opt.
   */
  : TK_wreal delay3_opt net_variable_or_decl_assigns ';'
    { $$ = MakeTaggedNode(N::kModulePortDeclaration, $1, $2, $3, $4); }
  | port_direction TK_wreal list_of_identifiers_unpacked_dimensions ';'
    { $$ = MakeTaggedNode(N::kModulePortDeclaration, $1, $2, $3, $4); }
  // | TK_wreal delay3 net_variable_list ';'
  // | TK_wreal net_variable_list ';'
  // | TK_wreal net_decl_assigns ';'
  | port_direction signed_unsigned_opt qualified_id decl_dimensions_opt
    list_of_identifiers_unpacked_dimensions ';'
    { $$ = MakeTaggedNode(N::kModulePortDeclaration, $1, $2, $3,
                          MakePackedDimensionsNode($4),
                          $5, $6); }
  | port_direction signed_unsigned_opt unqualified_id decl_dimensions_opt
    list_of_identifiers_unpacked_dimensions ';'
    { $$ = MakeTaggedNode(N::kModulePortDeclaration, $1, $2, $3,
                          MakePackedDimensionsNode($4),
                          $5, $6); }
  | port_direction signed_unsigned_opt decl_dimensions delay3_opt
    list_of_identifiers_unpacked_dimensions ';'
    { $$ = MakeTaggedNode(N::kModulePortDeclaration, $1, $2,
                          MakePackedDimensionsNode($3), $4,
                          $5, $6);}
    /* implicit type */
  | port_direction signed_unsigned_opt delay3
    list_of_identifiers_unpacked_dimensions ';'
    { $$ = MakeTaggedNode(N::kModulePortDeclaration, $1, $2, $3, $4, $5); }
    /* implicit type */
  | port_direction signed_unsigned_opt list_of_module_item_identifiers ';'
    { $$ = MakeTaggedNode(N::kModulePortDeclaration, $1, $2, $3, $4);}
    /* implicit type */
  | port_direction port_net_type signed_unsigned_opt decl_dimensions_opt
    list_of_identifiers_unpacked_dimensions ';'
    { $$ = MakeTaggedNode(N::kModulePortDeclaration, $1, $2, $3,
                          MakePackedDimensionsNode($4),
                          $5, $6); }
  | dir var_type signed_unsigned_opt decl_dimensions_opt
    list_of_port_identifiers ';'
    { $$ = MakeTaggedNode(N::kModulePortDeclaration, $1, $2, $3,
                          MakePackedDimensionsNode($4),
                          $5, $6); }
  ;

parameter_override
  : TK_defparam defparam_assign_list ';'
    { $$ = MakeTaggedNode(N::kParameterOverride, $1, $2, $3); }
  ;

gate_instantiation
  // TODO(jeremycs): possibly introduce structure here
  : /* attribute_list_opt */ gatetype primitive_gate_instance_list ';'
    { $$ = MakeTaggedNode(N::kGateInstantiation, $1, $2, $3); }
  | /* attribute_list_opt */ gatetype delay3 primitive_gate_instance_list ';'
    { $$ = MakeTaggedNode(N::kGateInstantiation, $1, $2, $3, $4); }
  | /* attribute_list_opt */ gatetype drive_strength primitive_gate_instance_list ';'
    { $$ = MakeTaggedNode(N::kGateInstantiation, $1, $2, $3, $4); }
  | /* attribute_list_opt */ gatetype drive_strength delay3 primitive_gate_instance_list ';'
    { $$ = MakeTaggedNode(N::kGateInstantiation, $1, $2, $3, $4, $5); }
  | /* attribute_list_opt */ switchtype primitive_gate_instance_list ';'
    { $$ = MakeTaggedNode(N::kGateInstantiation, $1, $2, $3); }
  | /* attribute_list_opt */ switchtype delay3 primitive_gate_instance_list ';'
    { $$ = MakeTaggedNode(N::kGateInstantiation, $1, $2, $3, $4); }
  | TK_pullup primitive_gate_instance_list ';'
    { $$ = MakeTaggedNode(N::kGateInstantiation, $1, $2, $3); }
  | TK_pulldown primitive_gate_instance_list ';'
    { $$ = MakeTaggedNode(N::kGateInstantiation, $1, $2, $3); }
  | TK_pullup '(' dr_strength1 ')' primitive_gate_instance_list ';'
    { $$ = MakeTaggedNode(N::kGateInstantiation, $1,
                          MakeParenGroup($2, $3, $4), $5, $6); }
  | TK_pullup '(' dr_strength1 ',' dr_strength0 ')' primitive_gate_instance_list ';'
    { $$ = MakeTaggedNode(N::kGateInstantiation, $1,
                          MakeParenGroup($2, MakeNode($3, $4, $5), $6), $7, $8); }
  | TK_pullup '(' dr_strength0 ',' dr_strength1 ')' primitive_gate_instance_list ';'
    { $$ = MakeTaggedNode(N::kGateInstantiation, $1,
                          MakeParenGroup($2, MakeNode($3, $4, $5), $6), $7, $8); }
  | TK_pulldown '(' dr_strength0 ')' primitive_gate_instance_list ';'
    { $$ = MakeTaggedNode(N::kGateInstantiation, $1,
                          MakeParenGroup($2, $3, $4), $5, $6); }
  | TK_pulldown '(' dr_strength1 ',' dr_strength0 ')' primitive_gate_instance_list ';'
    { $$ = MakeTaggedNode(N::kGateInstantiation, $1,
                          MakeParenGroup($2, MakeNode($3, $4, $5), $6), $7, $8); }
  | TK_pulldown '(' dr_strength0 ',' dr_strength1 ')' primitive_gate_instance_list ';'
    { $$ = MakeTaggedNode(N::kGateInstantiation, $1,
                          MakeParenGroup($2, MakeNode($3, $4, $5), $6), $7, $8); }
  ;

specify_block
  : TK_specify specify_item_list_opt TK_endspecify
    { $$ = MakeTaggedNode(N::kSpecifyBlock, $1, $2, $3); }
  ;

specparam_declaration
  : /* attribute_list_opt */ TK_specparam specparam_decl ';'
    { $$ = MakeTaggedNode(N::kSpecParamDeclaration, $1, $2, $3); }
  ;

generate_region
  : TK_generate generate_item_list_opt TK_endgenerate
    { $$ = MakeTaggedNode(N::kGenerateRegion, $1, $2, $3); }
  ;

continuous_assign
  : TK_assign drive_strength_opt delay3_opt cont_assign_list ';'
    { $$ = MakeTaggedNode(N::kContinuousAssignmentStatement, $1, $2, $3, $4, $5); }
  /* Allowed the following because they have been observed in practice: */
  | TK_assign drive_strength_opt delay3_opt macro_call_or_item
    { $$ = MakeTaggedNode(N::kContinuousAssignmentStatement, $1, $2, $3, $4, nullptr); }
  /* TODO(fangism): shape kContinuousAssignmentStatement consistently */
  ;

net_alias_assign_lvalue_list
  : net_alias_assign_lvalue_list '=' lpvalue
    { $$ = ExtendNode($1, $2, $3); }
  | lpvalue '=' lpvalue
    { $$ = MakeTaggedNode(N::kNetAliasLvalueList, $1, $2, $3); }
  ;

net_alias
  : TK_alias net_alias_assign_lvalue_list ';'
    { $$ = MakeTaggedNode(N::kNetAlias, $1, $2, $3); }
  ;

loop_generate_construct
  : TK_for '(' genvar_opt GenericIdentifier '=' expression ';'
    expression_opt ';' for_step_opt ')'
    generate_item
    { $$ = MakeTaggedNode(
        N::kLoopGenerateConstruct,
        MakeTaggedNode(
            N::kLoopHeader, $1,
            MakeParenGroup($2,
                MakeTaggedNode(N::kForSpec,
                    MakeTaggedNode(N::kForInitialization, $3, nullptr, $4, $5, $6),
                    $7,
                    MakeTaggedNode(N::kForCondition, $8),
                    $9, $10),
                $11)),
        $12); }
    /* for_step permits any assignment operation */
  ;

conditional_generate_construct
  : generate_if generate_item TK_else generate_item
    { $$ = MakeTaggedNode(N::kConditionalGenerateConstruct,
             MakeTaggedNode(N::kGenerateIfClause,
               MakeTaggedNode(N::kGenerateIfHeader, $1),
               MakeTaggedNode(N::kGenerateIfBody, $2)),
           MakeTaggedNode(N::kGenerateElseClause,
               $3,
               MakeTaggedNode(N::kGenerateElseBody, $4))); }
  | generate_if generate_item %prec less_than_TK_else
    { $$ = MakeTaggedNode(N::kConditionalGenerateConstruct,
             MakeTaggedNode(N::kGenerateIfClause,
               MakeTaggedNode(N::kGenerateIfHeader, $1),
               MakeTaggedNode(N::kGenerateIfBody, $2)),
           nullptr); }
  | TK_case '(' expression ')' generate_case_items TK_endcase
    { $$ = MakeTaggedNode(N::kCaseGenerateConstruct, $1,
                          MakeParenGroup($2, $3, $4), $5, $6); }
  ;

always_construct
  : /* attribute_list_opt */ always_any statement
    { $$ = MakeTaggedNode(N::kAlwaysStatement, $1, $2); }
  ;

initial_construct
  : /* attribute_list_opt */ TK_initial statement
    { $$ = MakeTaggedNode(N::kInitialStatement, $1, $2); }
  ;

final_construct
  : /* attribute_list_opt */ TK_final statement
    { $$ = MakeTaggedNode(N::kFinalStatement, $1, $2); }
  ;

analog_construct
  : /* attribute_list_opt */ TK_analog analog_statement
    { $$ = MakeTaggedNode(N::kAnalogStatement, $1, ForwardChildren($2)); }
  ;

module_common_item
  : module_or_generate_item_declaration
    { $$ = move($1); }
  | always_construct
    { $$ = move($1); }
  | initial_construct
    { $$ = move($1); }
  | final_construct
    { $$ = move($1); }
  | analog_construct
    /* Verilog-AMS*/
    { $$ = move($1); }
  | assertion_item
    { $$ = move($1); }
  | bind_directive
    { $$ = move($1); }
  | continuous_assign
    { $$ = move($1); }
  | net_alias
    { $$ = move($1); }
  | loop_generate_construct
    { $$ = move($1); }
  | conditional_generate_construct
    { $$ = move($1); }
  | system_tf_call ';'
    { $$ = ExtendNode($1, $2); }
    /* covers elaboration_system_task */
  ;

genvar_declaration
  : TK_genvar list_of_identifiers ';'
    { $$ = MakeTaggedNode(N::kGenvarDeclaration, $1, $2, $3); }
  ;

module_or_generate_item_declaration
  // TODO(jeremycs): fill this out
  : package_or_generate_item_declaration
    { $$ = move($1); }
  | clocking_declaration
    { $$ = move($1); }
  | TK_default TK_clocking GenericIdentifier ';'
    { $$ = MakeTaggedNode(N::kDefaultClockingStatement, $1, $2, $3, $4); }
  | TK_default TK_disable TK_iff expression_or_dist ';'
    { $$ = MakeTaggedNode(N::kDefaultDisableStatement, $1, $2, $3, $4, $5); }
  | genvar_declaration
    { $$ = move($1); }
  ;

package_or_generate_item_declaration
  : class_declaration
    { $$ = move($1); }
  | interface_class_declaration
    { $$ = move($1); }
  | net_declaration
    { $$ = move($1); }
  | task_declaration
    { $$ = move($1); }
  | function_declaration
    { $$ = move($1); }
  | covergroup_declaration
    { $$ = move($1); }
  | assertion_item_declaration
    { $$ = move($1); }
  | modport_declaration
    { $$ = move($1); }
  | specparam_declaration
    { $$ = move($1); }
  /* TODO(fangism): checker_declaration */
  | dpi_import_export
    { $$ = move($1); }
  | ';'
    { $$ = MakeTaggedNode(N::kNullItem, $1); }
  ;

module_or_generate_item
  : parameter_override
    { $$ = move($1); }
  | gate_instantiation
    { $$ = move($1); }
  /* TODO(fangism): udp_instantiation */
  | /* attribute_list_opt */ block_item_decl
    { $$ = move($1); }
    /* includes module_instantiation, and most other instantiations */
  | module_common_item
    { $$ = move($1); }
  ;

module_item
  : module_port_declaration
    { $$ = move($1); }
  | non_port_module_item
    { $$ = move($1); }
  | module_block
    { $$ = move($1); }
  | macro_call_or_item
    { $$ = move($1); }
  | preprocessor_balanced_module_items
    { $$ = move($1); }
  | preprocessor_action
    { $$ = move($1); }
  | module_item_directive
    { $$ = move($1); }
  | error ';'
    { yyerrok; $$ = Recover(); }
  ;

module_block
  /* This construct is not LRM-valid, but is popularly supported among other
   * tools, so we decided to legalize the syntax while flagging it as a
   * lint error.
   */
  : begin module_item_list_opt end
    { $$ = MakeTaggedNode(N::kModuleBlock, $1, $2, $3); }
  ;
preprocessor_balanced_module_items
  : preprocessor_if_header module_item_list_opt
    preprocessor_elsif_module_items_opt
    preprocessor_else_module_item_opt
    PP_endif
    { $$ = MakeTaggedNode(N::kPreprocessorBalancedModuleItems,
                          ExtendNode($1, $2), ForwardChildren($3), $4, $5);
    }
  ;
preprocessor_elsif_module_items_opt
  : preprocessor_elsif_module_items
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
preprocessor_elsif_module_items
  : preprocessor_elsif_module_items preprocessor_elsif_module_item
    { $$ = ExtendNode($1, $2); }
  | preprocessor_elsif_module_item
    { $$ = MakeNode($1); }  /* Don't bother tagging; node will be flattened. */
  ;
preprocessor_elsif_module_item
  : preprocessor_elsif_header module_item_list_opt
    { $$ = ExtendNode($1, $2); }
  ;
preprocessor_else_module_item_opt
  : preprocessor_else_module_item
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
preprocessor_else_module_item
  : PP_else module_item_list_opt
    { $$ = MakeTaggedNode(N::kPreprocessorElseClause, $1, $2); }
  ;

non_port_module_item
  : generate_region
    { $$ = move($1); }
  | module_or_generate_item
    { $$ = move($1); }
  | specify_block
    { $$ = move($1); }
  | timeunits_declaration
    { $$ = move($1); }
  | module_or_interface_declaration
    { $$ = move($1); }
  | TKK_attribute '(' GenericIdentifier ',' TK_StringLiteral ',' TK_StringLiteral ')' ';'
    { $$ = MakeTaggedNode(N::kAttribute, $1,
                          MakeParenGroup($2, MakeNode($3, $4, $5, $6, $7),
                                         $8), $9); }
  ;

always_any
  : TK_always
    { $$ = move($1); }
  | TK_always_ff
    { $$ = move($1); }
  | TK_always_comb
    { $$ = move($1); }
  | TK_always_latch
    { $$ = move($1); }
  ;
generate_if
  : TK_if '(' expression ')'
    { $$ = MakeTaggedNode(N::kGenerateIf, $1, MakeParenGroup($2, $3, $4)); }
  ;
generate_case_items
  : generate_case_items generate_case_item
    { $$ = ExtendNode($1, $2); }
  | generate_case_item
    { $$ = MakeTaggedNode(N::kGenerateCaseItemList, $1); }
  ;
generate_case_item
  : expression_list_proper ':' generate_item
    { $$ = MakeTaggedNode(N::kGenerateCaseItem, $1, $2, $3); }
  | TK_default ':' generate_item
    { $$ = MakeTaggedNode(N::kGenerateDefaultItem, $1, $2, $3); }
  ;

generate_item
  : module_or_generate_item
    { $$ = move($1); }
  /*
  | interface_or_generate_item
  | checker_or_generate_item
   */
  | generate_block
    { $$ = move($1); }
  | macro_call_or_item
    { $$ = move($1); }
  | preprocessor_balanced_generate_items
    { $$ = move($1); }
  | preprocessor_action
    { $$ = move($1); }
  | error ';'
    { yyerrok; $$ = Recover(); }
  ;
preprocessor_balanced_generate_items
  : preprocessor_if_header generate_item_list_opt
    preprocessor_elsif_generate_items_opt
    preprocessor_else_generate_item_opt
    PP_endif
    { $$ = MakeTaggedNode(N::kPreprocessorBalancedGenerateItems,
                          ExtendNode($1, $2), ForwardChildren($3), $4, $5);
    }
  ;
preprocessor_elsif_generate_items_opt
  : preprocessor_elsif_generate_items
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
preprocessor_elsif_generate_items
  : preprocessor_elsif_generate_items preprocessor_elsif_generate_item
    { $$ = ExtendNode($1, $2); }
  | preprocessor_elsif_generate_item
    { $$ = MakeNode($1); }  /* Don't bother tagging; node will be flattened. */
  ;
preprocessor_elsif_generate_item
  : preprocessor_elsif_header generate_item_list_opt
    { $$ = ExtendNode($1, $2); }
  ;
preprocessor_else_generate_item_opt
  : preprocessor_else_generate_item
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
preprocessor_else_generate_item
  : PP_else generate_item_list_opt
    { $$ = MakeTaggedNode(N::kPreprocessorElseClause, $1, $2); }
  ;

begin
  : TK_begin label_opt
    { $$ = MakeTaggedNode(N::kBegin, $1, $2); }
  ;

end
  : TK_end label_opt
    { $$ = MakeTaggedNode(N::kEnd, $1, $2); }
  ;

/* The LRM swaps the roles of generate_item and generate_block, but block as
 * an subset of item (not the reverse) is more conventional in other languages.
 */
generate_block
  : begin generate_item_list_opt end
    { $$ = MakeTaggedNode(N::kGenerateBlock, $1, $2, $3); }
    /* begin : label is more common and is the preferred style. */
  | unqualified_id ':' TK_begin generate_item_list_opt end
    { $$ = MakeTaggedNode(N::kGenerateBlock,
                          MakeTaggedNode(N::kBegin,
                                         MakeTaggedNode(N::kLabel, $1, $2),
                                         $3),
                          $4, $5); }
    /* $1 should be a GenericIdentifier, without parameter_value */
    /* Legal syntax, but the style-linter should reject this. */
  ;

generate_item_list
  : generate_item_list generate_item
    { $$ = ExtendNode($1, $2); }
  | generate_item
    { $$ = MakeTaggedNode(N::kGenerateItemList, $1); }
  ;

generate_item_list_opt
  : generate_item_list
    { $$ = move($1); }
  | /* empty */
    { $$ = MakeTaggedNode(N::kGenerateItemList); }
  ;

module_item_list
  : module_item_list module_item
    { $$ = ExtendNode($1, $2); }
  | module_item
    { $$ = MakeTaggedNode(N::kModuleItemList, $1); }
  ;
module_item_list_opt
  : module_item_list
    { $$ = move($1); }
  | /* empty */
    { $$ = MakeTaggedNode(N::kModuleItemList); }
  ;
genvar_opt
  : TK_genvar
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
net_decl_assign
  : GenericIdentifier '=' expression
    { $$ = MakeTaggedNode(N::kNetDeclarationAssignment, $1, $2, $3); }
  ;
/**
net_decl_assigns
  : net_decl_assigns ',' net_decl_assign
  | net_decl_assign
  ;
**/
net_variable_or_decl_assign
  : net_variable
    { $$ = move($1); }
  | net_decl_assign
    { $$ = move($1); }
  ;
net_variable_or_decl_assigns
  : net_variable_or_decl_assigns ',' net_variable_or_decl_assign
    { $$ = ExtendNode($1, $2, $3); }
  | net_variable_or_decl_assign
    { $$ = MakeTaggedNode(N::kNetVariableDeclarationAssign, $1); }
  ;

bit_logic
  : TK_logic
    { $$ = move($1); }
  | TK_bit
    { $$ = move($1); }
  ;
bit_logic_opt
  : bit_logic
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
port_net_type
  : net_type
    { $$ = move($1); }
  | TK_logic
    { $$ = move($1); }
  ;
net_type
  : TK_wire
    { $$ = move($1); }
  | TK_tri
    { $$ = move($1); }
  | TK_tri1
    { $$ = move($1); }
  | TK_supply0
    { $$ = move($1); }
  | TK_wand
    { $$ = move($1); }
  | TK_triand
    { $$ = move($1); }
  | TK_tri0
    { $$ = move($1); }
  | TK_supply1
    { $$ = move($1); }
  | TK_wor
    { $$ = move($1); }
  | TK_trior
    { $$ = move($1); }
  | TK_wone
    { $$ = move($1); }
  | TK_uwire
    { $$ = move($1); }
  ;
var_type
  : TK_reg
  ;

/**
param_type
  : bit_logic_opt signed_unsigned_opt decl_dimensions_opt
    // includes implicit type
  | integer_atom_type
  | non_integer_type
  ;
**/

  /* this rule introduced to allow:
   *    localparam RANGE ID RANGE
   * allow decl_dimensions_opt in between
   **/
param_type_followed_by_id_and_dimensions_opt
  // : bit_logic_opt signed_unsigned_opt decl_dimensions_opt GenericIdentifier decl_dimensions_opt
  /* apparently, there are user-defined parameter types */
  /* TODO(fangism): enforce more structure here, e.g. kDataType */
  : bit_logic_opt signed_unsigned_opt qualified_id decl_dimensions_opt
    GenericIdentifier decl_dimensions_opt
    { $$ = MakeParamTypeDeclaration(MakeTypeInfoNode($1, $2, $3),
                                    MakePackedDimensionsNode($4),
                                    $5,
                                    MakeUnpackedDimensionsNode($6)); }
  | bit_logic_opt signed_unsigned_opt unqualified_id decl_dimensions_opt
    GenericIdentifier decl_dimensions_opt
    { $$ = MakeParamTypeDeclaration(MakeTypeInfoNode($1, $2, $3),
                                    MakePackedDimensionsNode($4),
                                    $5,
                                    MakeUnpackedDimensionsNode($6)); }
  | bit_logic_opt signed_unsigned_opt unqualified_id decl_dimensions_opt
    { $$ = MakeParamTypeDeclaration(MakeTypeInfoNode($1, $2, nullptr),
                                    /* no packed dimensions */ nullptr,
                                    /* parameter id, not type */ $3,
                                    MakeUnpackedDimensionsNode($4)); }
    /* implicit type.  Declared identifier upgraded to unqualified_id to avoid conflict. */
    /* TODO(fangism): Verify that $3 is only a GenericIdentifier, without parameters. */
  | bit_logic_opt signed_unsigned_opt decl_dimensions
    GenericIdentifier decl_dimensions_opt
    { $$ = MakeParamTypeDeclaration(MakeTypeInfoNode($1, $2, nullptr),
                                    MakePackedDimensionsNode($3),
                                    $4,
                                    MakeUnpackedDimensionsNode($5)); }
  | integer_atom_type signed_unsigned_opt decl_dimensions_opt
    GenericIdentifier decl_dimensions_opt
    { $$ = MakeParamTypeDeclaration(MakeTypeInfoNode($1, $2, nullptr),
                                    MakePackedDimensionsNode($3),
                                    $4,
                                    MakeUnpackedDimensionsNode($5)); }
  | non_integer_type decl_dimensions_opt
    GenericIdentifier decl_dimensions_opt
    { $$ = MakeParamTypeDeclaration(MakeTypeInfoNode($1, nullptr, nullptr),
                                    MakePackedDimensionsNode($2),
                                    $3,
                                    MakeUnpackedDimensionsNode($4)); }
  | TK_reg decl_dimensions_opt
    GenericIdentifier decl_dimensions_opt
    { $$ = MakeParamTypeDeclaration(MakeTypeInfoNode($1, nullptr, nullptr),
                                    MakePackedDimensionsNode($2),
                                    $3,
                                    MakeUnpackedDimensionsNode($4)); }
  | TK_string decl_dimensions_opt
    GenericIdentifier decl_dimensions_opt
    { $$ = MakeParamTypeDeclaration(MakeTypeInfoNode($1, nullptr, nullptr),
                                    MakePackedDimensionsNode($2),
                                    $3,
                                    MakeUnpackedDimensionsNode($4)); }
  /* TODO(fangism): see if this can be simplified to:
   * data_type_or_implicit_basic_followed_by_id_and_dimensions_opt
   */
  ;
parameter_assign_list
  : parameter_assign
    { $$ = MakeTaggedNode(N::kParameterAssignList, $1); }
  | parameter_assign_list ',' parameter_assign
    { $$ = ExtendNode($1, $2, $3); }
  ;
localparam_assign_list
  : localparam_assign
    { $$ = MakeTaggedNode(N::kParameterAssignList, $1); }
  | localparam_assign_list ',' localparam_assign
    { $$ = ExtendNode($1, $2, $3); }
  ;
parameter_assign
  : GenericIdentifier '=' expression parameter_value_ranges_opt
    { $$ = MakeTaggedNode(N::kParameterAssign, $1, $2, $3); }
  ;
localparam_assign
  : GenericIdentifier '=' expression
    { $$ = MakeTaggedNode(N::kParameterAssign, $1, $2, $3); }
  ;
/**
 * Normally localparam assign do not take parameter_value_ranges_opt,
 * whereas param assigns can, but we simplify the grammar by merging these.
 **/
trailing_assign
  /* similar to trailing_decl_assignment */
  : '=' parameter_expr parameter_value_ranges_opt
    { $$ = MakeTaggedNode(N::kTrailingAssign, $1, $2, $3); }
  ;
trailing_assign_opt
  : trailing_assign
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;

parameter_value_ranges_opt
  : parameter_value_ranges
  | /* empty */
  ;
parameter_value_ranges
  : parameter_value_ranges parameter_value_range
  | parameter_value_range
  ;
parameter_value_range
  : from_exclude '[' value_range_expression ':' value_range_expression ']'
  | from_exclude '[' value_range_expression ':' value_range_expression ')'
  | from_exclude '(' value_range_expression ':' value_range_expression ']'
  | from_exclude '(' value_range_expression ':' value_range_expression ')'
  | TK_exclude expression
  ;
value_range_expression
  : expression
  /* Verilog-AMS supports +/- inf in ranges. */
  | TK_inf
  | '+' TK_inf
  | '-' TK_inf
  ;
from_exclude
  : TK_from
  | TK_exclude
  ;
parameter_value_opt
  : parameters
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
parameters
  : '#' '(' parameter_expr_list ')'
    { $$ = MakeTaggedNode(N::kActualParameterList, $1, MakeParenGroup($2, $3, $4)); }
  | '#' '(' parameter_value_byname_list ')'
    { $$ = MakeTaggedNode(N::kActualParameterList, $1, MakeParenGroup($2, $3, $4)); }
    /* TODO(fangism): allow preprocessor_directives in parameter_expr_list
     * by combining with parameter_value_byname_list.
     */
  | '#' '(' ')'
    { $$ = MakeTaggedNode(N::kActualParameterList, $1, MakeParenGroup($2, nullptr, $3)); }
  | '#' TK_DecNumber
    { $$ = MakeTaggedNode(N::kActualParameterList, $1, $2); }
  | '#' TK_RealTime
    { $$ = MakeTaggedNode(N::kActualParameterList, $1, $2); }
  ;
parameter_expr_list
  /* positional arguments */
  : parameter_expr_list ',' parameter_expr
    { $$ = ExtendNode($1, $2, $3); }
  | parameter_expr
    { $$ = MakeTaggedNode(N::kActualParameterPositionalList, $1); }
  ;
parameter_value_byname
  : '.' member_name '(' parameter_expr ')'
    { $$ = MakeTaggedNode(N::kParamByName, $1, $2, MakeParenGroup($3, $4, $5)); }
  | '.' member_name '(' ')'
    { $$ = MakeTaggedNode(N::kParamByName, $1, $2, MakeParenGroup($3, nullptr, $4)); }
  | '.' member_name
    { $$ = MakeTaggedNode(N::kParamByName, $1, $2, nullptr); }
  ;
parameter_value_byname_list
  /* named arguments */
  : parameter_value_byname_list_item_last
    { $$ = move($1); }
  | parameter_value_byname_list_preprocessor_last
    { $$ = move($1); }
  | parameter_value_byname_list_trailing_comma
    { $$ = move($1); }
  ;
parameter_value_byname_list_trailing_comma
  : parameter_value_byname_list ','
    { $$ = ExtendNode($1, $2); }
  | ','
    { $$ = MakeTaggedNode(N::kActualParameterByNameList, $1); }
  ;
parameter_value_byname_list_preprocessor_last
  : parameter_value_byname_list preprocessor_directive
    { $$ = ExtendNode($1, $2); }
  | preprocessor_directive
    { $$ = MakeTaggedNode(N::kActualParameterByNameList, $1); }
  ;
parameter_value_byname_list_item_last
  : parameter_value_byname
    { $$ = MakeTaggedNode(N::kActualParameterByNameList, $1); }
  | parameter_value_byname_list_trailing_comma parameter_value_byname
    { $$ = ExtendNode($1, $2); }
  | parameter_value_byname_list_preprocessor_last parameter_value_byname
    { $$ = ExtendNode($1, $2); }
  ;

parameter_expr
  /* similar to any_argument */
  : expression
    { $$ = move($1); }
  | data_type_primitive
    { $$ = move($1); }
  /* Spec grammar allows any data_type, but all user-defined types can look like
   * expressions as qualified or unqualified identifiers.  So limiting this
   * rule to only primitive types eliminates conflict.
   * Types may come from expression, but that can only be determined by symbol
   * table lookup.
   */
  | interface_type
    { $$ = move($1); }
  ;

/* non-ANSI port declarations are covered in the LRM 23.2.2.1 */
port
  : port_expression trailing_assign_opt
    /* when using a trailing_assign, port_expression should be a port_reference */
    { $$ = MakeTaggedNode(N::kPort, $1, $2); }
  | '.' member_name '(' port_expression_opt ')'
    { $$ = MakeTaggedNode(N::kPort, $1, $2, MakeParenGroup($3, $4, $5)); }
  ;
any_port_list_opt
  : any_port_list
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
any_port_list
  : any_port_list_item_last
    { $$ = move($1); }
  | any_port_list_preprocessor_last
    { $$ = move($1); }
  | any_port_list_trailing_comma
    { $$ = move($1); }
/* TODO(b/36237582): accept a macro item here
  | any_port_list_trailing_macro_item
    { $$ = move($1); }
  ;
*/
any_port_list_trailing_comma
  : any_port_list ','
    { $$ = ExtendNode($1, $2); }
  | ','
    { $$ = MakeTaggedNode(N::kPortActualList, $1); }
  ;
/* TODO(b/36237582): accept a macro item here
   The difficulty around this lies with the reduction path from
   MacroGenericItem -> expr_primary_no_groups -> expression.
   Allowing MacroGenericItem here will hit R/R conflicts.
   Ideally, what we want to express is that commas are optional following
   MacroGenericItems.  If we made commas optional entirely, it would be
   too permissive w.r.t. actual LRM grammar, and would push the responsibility
   to CST validation.
any_port_list_trailing_macro_item
  : any_port_list MacroGenericItem
    { $$ = ExtendNode($1, $2); }
  | MacroGenericItem
    { $$ = MakeTaggedNode(N::kPortActualList, $1); }
  ;
*/
any_port_list_item_last
  : any_port_list_trailing_comma any_port
    { $$ = ExtendNode($1, $2); }
  | any_port_list_preprocessor_last any_port
    { $$ = ExtendNode($1, $2); }
  | any_port
    { $$ = MakeTaggedNode(N::kPortActualList, $1); }
  ;
any_port_list_preprocessor_last
  : any_port_list preprocessor_directive
    { $$ = ExtendNode($1, $2); }
  | preprocessor_directive
    { $$ = MakeTaggedNode(N::kPortActualList, $1); }
  ;
any_port
  : port_named
    { $$ = move($1); }
  | expression
    /* Note: expr_primary_no_groups already covers MacroGenericItem */
    { $$ = MakeTaggedNode(N::kActualPositionalPort, move($1)); }
  ;
port_named
  : '.' member_name '(' expression ')'
    { $$ = MakeTaggedNode(N::kActualNamedPort, $1, $2, MakeParenGroup($3, $4, $5)); }
  | '.' member_name '(' ')'
    { $$ = MakeTaggedNode(N::kActualNamedPort, $1, $2, MakeParenGroup($3, nullptr, $4)); }
  | '.' member_name
    { $$ = MakeTaggedNode(N::kActualNamedPort, $1, $2, nullptr); }
  | TK_DOTSTAR
    { $$ = move($1); }
  ;

member_name
  : GenericIdentifier
    { $$ = move($1); }
  | builtin_array_method
    { $$ = move($1); }
  ;

port_expression_opt
  : port_expression
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;

port_expression
  : port_reference
    { $$ = move($1); }
  | '{' port_reference_list '}'
    { $$ = MakeBraceGroup($1, $2, $3); }
  ;

port_reference
  : unqualified_id decl_dimensions_opt
    { $$ = MakeTaggedNode(N::kPortReference, $1, $2); }
  /* These 'unqualified_id' should all be GenericIdentifier, but were
   * promoted to ease S/R conflict resolution vs. general class_ids in
   * port declarations.
   * TODO(fangism): Verify that $1 is a bare GenericIdentifier.
   * TODO(fangism): Verify that $2 is a constant_select (select_dimensions).
   */
  ;
port_reference_list
  : port_reference
    { $$ = MakeTaggedNode(N::kPortReferenceList, $1); }
  | port_reference_list ',' port_reference
    { $$ = ExtendNode($1, $2, $3); }
  ;
select_dimensions_opt
  : select_dimensions
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
select_dimensions
  : select_variable_dimension
    { $$ = MakeTaggedNode(N::kSelectVariableDimension, $1); }
  | select_dimensions select_variable_dimension
    { $$ = ExtendNode($1, $2); }
  ;

/* Depending on context, these could be either packed or unpacked dimensions.
 * The caller of these rules should annotate as such.
 */
decl_dimensions_opt
  : decl_dimensions
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
decl_dimensions
  : decl_variable_dimension
    { $$ = MakeTaggedNode(N::kDeclarationDimensions, $1); }
  | decl_dimensions decl_variable_dimension
    { $$ = ExtendNode($1, $2); }
  ;

/** merged into: gate_instance_or_register_variable_list
register_variable
  : GenericIdentifier decl_dimensions_opt
  | GenericIdentifier decl_dimensions_opt '=' expression
  ;
register_variable_list
  : register_variable
  | register_variable_list ',' register_variable
  ;
**/
net_variable
  : GenericIdentifier decl_dimensions_opt
    { $$ = MakeTaggedNode(N::kNetVariable, $1,
                          MakeUnpackedDimensionsNode($2)); }
  ;
/** merged into net_variable_or_decl_assigns:
net_variable_list
  : net_variable
  | net_variable_list ',' net_variable
  ;
**/
specify_item
  : TK_specparam specparam_decl ';'
    { $$ = MakeTaggedNode(N::kSpecifyItem, $1, $2, $3); }
  | specify_simple_path_decl ';'
    { $$ = MakeTaggedNode(N::kSpecifyItem, $1, $2); }
  | specify_edge_path_decl ';'
    { $$ = MakeTaggedNode(N::kSpecifyItem, $1, $2); }
  | TK_if '(' expression ')' specify_simple_path_decl ';'
    { $$ = MakeTaggedNode(N::kSpecifyItem, $1,
                          MakeParenGroup($2, $3, $4), $5, $6); }
  | TK_if '(' expression ')' specify_edge_path_decl ';'
    { $$ = MakeTaggedNode(N::kSpecifyItem, $1,
                          MakeParenGroup($2, $3, $4), $5, $6); }
  | TK_ifnone specify_simple_path_decl ';'
    { $$ = MakeTaggedNode(N::kSpecifyItem, $1, $2, $3); }
  | TK_ifnone specify_edge_path_decl ';'
    { $$ = MakeTaggedNode(N::kSpecifyItem, $1, $2, $3); }
  | TK_Sfullskew  '(' spec_reference_event ',' spec_reference_event
    ',' delay_value ',' delay_value spec_notifier_opt ')' ';'
    { $$ = MakeTaggedNode(N::kSpecifyItem, $1,
                          MakeParenGroup($2, MakeNode($3, $4, $5, $6, $7, $8, $9, $10), $11),
                         $12);}
  | TK_Snochange  '(' spec_reference_event ',' spec_reference_event
    ',' delay_value ',' delay_value spec_notifier_opt ')' ';'
    { $$ = MakeTaggedNode(N::kSpecifyItem, $1,
                          MakeParenGroup($2, MakeNode($3, $4, $5, $6, $7, $8, $9, $10), $11),
                          $12);}
  | TK_Srecrem    '(' spec_reference_event ',' spec_reference_event
    ',' delay_value ',' delay_value spec_notifier_opt ')' ';'
    { $$ = MakeTaggedNode(N::kSpecifyItem, $1,
                          MakeParenGroup($2, MakeNode($3, $4, $5, $6, $7, $8, $9, $10), $11),
                          $12);}
  | TK_Ssetuphold '(' spec_reference_event ',' spec_reference_event
    ',' delay_value ',' delay_value spec_notifier_opt ')' ';'
    { $$ = MakeTaggedNode(N::kSpecifyItem, $1,
                          MakeParenGroup($2, MakeNode($3, $4, $5, $6, $7, $8, $9, $10), $11),
                          $12);}

  | TK_Speriod '(' spec_reference_event ',' delay_value spec_notifier_opt ')' ';'
    { $$ = MakeTaggedNode(N::kSpecifyItem, $1,
                          MakeParenGroup($2, MakeNode($3, $4, $5, $6), $7),
                          $8);}
  | TK_Swidth '(' spec_reference_event ',' delay_value ')' ';'
    { $$ = MakeTaggedNode(N::kSpecifyItem, $1,
                          MakeParenGroup($2, MakeNode($3, $4, $5), $6),
                          $7);}
  | TK_Shold  '(' spec_reference_event ',' spec_reference_event
    ',' delay_value  spec_notifier_opt ')' ';'
    { $$ = MakeTaggedNode(N::kSpecifyItem, $1,
                          MakeParenGroup($2, MakeNode($3, $4, $5, $7, $8), $9),
                          $10);}
  | TK_Srecovery '(' spec_reference_event ',' spec_reference_event
    ',' delay_value  spec_notifier_opt ')' ';'
    { $$ = MakeTaggedNode(N::kSpecifyItem, $1,
                          MakeParenGroup($2, MakeNode($3, $4, $5, $6, $7, $8), $9),
                          $10); }
  | TK_Sremoval  '(' spec_reference_event ',' spec_reference_event
    ',' delay_value  spec_notifier_opt ')' ';'
    { $$ = MakeTaggedNode(N::kSpecifyItem, $1,
                          MakeParenGroup($2, MakeNode($3, $4, $5, $6, $7, $8), $9),
                          $10); }
  | TK_Ssetup    '(' spec_reference_event ',' spec_reference_event
    ',' delay_value  spec_notifier_opt ')' ';'
    { $$ = MakeTaggedNode(N::kSpecifyItem, $1,
                          MakeParenGroup($2, MakeNode($3, $4, $5, $6, $7, $8), $9),
                          $10); }
  | TK_Sskew     '(' spec_reference_event ',' spec_reference_event
    ',' delay_value  spec_notifier_opt ')' ';'
    { $$ = MakeTaggedNode(N::kSpecifyItem, $1,
                          MakeParenGroup($2, MakeNode($3, $4, $5, $6, $7, $8), $9),
                          $10); }
  | TK_Stimeskew '(' spec_reference_event ',' spec_reference_event
    ',' delay_value  spec_notifier_opt ')' ';'
    { $$ = MakeTaggedNode(N::kSpecifyItem, $1,
                          MakeParenGroup($2, MakeNode($3, $4, $5, $6, $7, $8), $9),
                          $10); }
  | TK_Swidth    '(' spec_reference_event ',' delay_value
    ',' expression   spec_notifier_opt ')' ';'
    { $$ = MakeTaggedNode(N::kSpecifyItem, $1,
                          MakeParenGroup($2, MakeNode($3, $4, $5, $6, $7, $8), $9),
                          $10); }

  | TK_pulsestyle_onevent  specify_path_identifiers ';'
    { $$ = MakeTaggedNode(N::kSpecifyItem, $1, $2, $3); }
  | TK_pulsestyle_ondetect specify_path_identifiers ';'
    { $$ = MakeTaggedNode(N::kSpecifyItem, $1, $2, $3); }
  | TK_showcancelled       specify_path_identifiers ';'
    { $$ = MakeTaggedNode(N::kSpecifyItem, $1, $2, $3); }
  | TK_noshowcancelled     specify_path_identifiers ';'
    { $$ = MakeTaggedNode(N::kSpecifyItem, $1, $2, $3); }
  | preprocessor_directive
    { $$ = move($1); }
  ;
specify_item_list
  : specify_item
    { $$ = MakeTaggedNode(N::kSpecifyItem, $1); }
  | specify_item_list specify_item
    { $$ = ExtendNode($1, $2); }
  ;
specify_item_list_opt
  : /* empty */
    { $$ = nullptr; }
  | specify_item_list
    { $$ = move($1); }
  ;
specify_edge_path_decl
  : specify_edge_path '=' '(' delay_value_list ')'
    { $$ = MakeTaggedNode(N::kSpecifyPathDeclaration, $1, $2, MakeParenGroup($3, $4, $5)); }
  | specify_edge_path '=' delay_value_simple
    { $$ = MakeTaggedNode(N::kSpecifyPathDeclaration, $1, $2, $3); }
  ;
edge_operator
  : TK_posedge
    { $$ = move($1); }
  | TK_negedge
    { $$ = move($1); }
  | TK_edge
    { $$ = move($1); }
  ;
specify_edge_path
  : '(' specify_path_identifiers spec_polarity
        TK_EG '(' specify_path_identifiers polarity_operator expression ')' ')'
    { $$ = MakeTaggedNode(N::kSpecifyEdgePath, $1, $2, $3, $4,
                          MakeParenGroup($5, MakeNode($6, $7, $8), $9), $10); }
  | '(' specify_path_identifiers spec_polarity
        TK_SG '(' specify_path_identifiers polarity_operator expression ')' ')'
    { $$ = MakeTaggedNode(N::kSpecifyEdgePath, $1, $2, $3, $4,
                          MakeParenGroup($5, MakeNode($6, $7, $8), $9), $10); }
  | '(' edge_operator specify_path_identifiers spec_polarity
        TK_EG '(' specify_path_identifiers polarity_operator expression ')' ')'
     { $$ = MakeTaggedNode(N::kSpecifyEdgePath, $1, $2, $3, $4, $5,
                          MakeParenGroup($6, MakeNode($7, $8, $9), $10), $11); }
  | '(' edge_operator specify_path_identifiers spec_polarity
        TK_SG '(' specify_path_identifiers polarity_operator expression ')' ')'
     { $$ = MakeTaggedNode(N::kSpecifyEdgePath, $1, $2, $3, $4, $5,
                          MakeParenGroup($6, MakeNode($7, $8, $9), $10), $11); }
  ;
polarity_operator
  : TK_PO_POS
    { $$ = move($1); }
  | TK_PO_NEG
    { $$ = move($1); }
  | ':'
    { $$ = move($1); }
  ;
specify_simple_path_decl
  : specify_simple_path '=' '(' delay_value_list ')'
    { $$ = MakeTaggedNode(N::kSpecifyPathDeclaration, $1, $2, MakeParenGroup($3, $4, $5)); }
  | specify_simple_path '=' delay_value_simple
    { $$ = MakeTaggedNode(N::kSpecifyPathDeclaration, $1, $2, $3); }
/**
  | specify_simple_path '=' '(' error ')'
**/
  ;
specify_simple_path
  : '(' specify_path_identifiers spec_polarity
               TK_EG specify_path_identifiers ')'
    { $$ = MakeTaggedNode(N::kSpecifySimplePath, $1, $2, $3, $4, $5, $6); }
  | '(' specify_path_identifiers spec_polarity
               TK_SG specify_path_identifiers ')'
    { $$ = MakeTaggedNode(N::kSpecifySimplePath, $1, $2, $3, $4, $5, $6); }
/**
  | '(' error ')'
**/
  ;
specify_path_identifiers
  : GenericIdentifier
    { $$ = MakeTaggedNode(N::kSpecifyPathIdentifier, $1); }
  | GenericIdentifier '[' expr_primary ']'
    { $$ = MakeTaggedNode(N::kSpecifyPathIdentifier, $1, MakeBracketGroup($2, $3, $4)); }
  | specify_path_identifiers ',' GenericIdentifier
    { $$ = ExtendNode($1, $2, $3); }
  | specify_path_identifiers ',' GenericIdentifier '[' expr_primary ']'
    { $$ = ExtendNode($1, $2, $3, MakeBracketGroup($4, $5, $6)); }
  ;
specparam
  /* all expressions should have constant value */
  : GenericIdentifier '=' expression
    { $$ = MakeTaggedNode(N::kSpecParam, $1, $2, $3); }
  | GenericIdentifier '=' expression ':' expression ':' expression
    { $$ = MakeTaggedNode(N::kSpecParam, $1, $2, MakeTaggedNode(N::kMinTypMaxList,
                                                             $3, $4, $5, $6, $7)); }
  /* TODO(fangism): support pulse_control_specparam.
  | GenericIdentifier '=' '(' expression ',' expression ')'
  */
  ;
specparam_list
  : specparam
    { $$ = MakeTaggedNode(N::kSpecParamList, $1); }
  | specparam_list ',' specparam
    { $$ = ExtendNode($1, $2); }
  ;
specparam_decl
  : specparam_list
    { $$ = move($1); }
  | decl_dimensions specparam_list
    { $$ = MakeTaggedNode(N::kSpecParamDeclaration,
                          MakePackedDimensionsNode($1), $2); }
  ;
spec_polarity
  : '+'
    { $$ = move($1); }
  | '-'
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
spec_reference_event
  /* also known as: timing_check_event */
  : edge_operator specify_terminal_descriptor
    { $$ = MakeTaggedNode(N::kSpecifyReferenceEvent, $1, $2);}
  | edge_operator specify_terminal_descriptor TK_TAND expression
    { $$ = MakeTaggedNode(N::kSpecifyReferenceEvent, $1, $2, $3, $4);}
    /* $4 should be a timing_check_condition */
  | TK_edge '[' edge_descriptor_list ']' specify_terminal_descriptor
    { $$ = MakeTaggedNode(N::kSpecifyReferenceEvent, $1, MakeBracketGroup($2, $3, $4),
                          $5); }
  | TK_edge '[' edge_descriptor_list ']' specify_terminal_descriptor TK_TAND expression
    { $$ = MakeTaggedNode(N::kSpecifyReferenceEvent, $1, MakeBracketGroup($2, $3, $4),
                          $5, $6, $7); }
  | specify_terminal_descriptor TK_TAND expression
    { $$ = MakeTaggedNode(N::kSpecifyReferenceEvent, $1, $2, $3);}
  | specify_terminal_descriptor
    { $$ = MakeTaggedNode(N::kSpecifyReferenceEvent, $1);}
  ;
edge_descriptor_list
  : edge_descriptor_list ',' TK_edge_descriptor
    { $$ = ExtendNode($1, $2, $3); }
  | TK_edge_descriptor
    { $$ = MakeTaggedNode(N::kEdgeDescriptorList, $1); }
  ;
specify_terminal_descriptor
  : reference
    { $$ = move($1); }
  ;
spec_notifier_opt
  : spec_notifier
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
spec_notifier
  : ','
    { $$ = MakeTaggedNode(N::kSpecifyNotifier, $1); }
  | ',' reference
    { $$ = MakeTaggedNode(N::kSpecifyNotifier, $1, $2); }
  | spec_notifier ','
    { $$ = ExtendNode($1, $2); }
  | spec_notifier ',' reference
    { $$ = ExtendNode($1, $2, $3); }
  | GenericIdentifier
    { $$ = MakeTaggedNode(N::kSpecifyNotifier, $1); }
  ;


case_any
  : TK_case
    { $$ = move($1); }
  | TK_casex
    { $$ = move($1); }
  | TK_casez
    { $$ = move($1); }
  ;

blocking_assignment
  /* TODO(fangism): structure kBlockingAssignmentStatement consistently */
  : lpvalue '=' delay_or_event_control expression ';'
    { $$ = MakeTaggedNode(N::kBlockingAssignmentStatement, $1, $2, $3, $4, $5); }
  | lpvalue '=' dynamic_array_new ';'
    { $$ = MakeTaggedNode(N::kBlockingAssignmentStatement, $1, $2, $3, $4); }
  | lpvalue '=' class_new ';'
    { $$ = MakeTaggedNode(N::kBlockingAssignmentStatement, $1, $2, $3, $4); }
  ;

nonblocking_assignment
  /* TODO(fangism): structure kNonblockingAssignmentStatement consistently */
  : lpvalue TK_LE delay_or_event_control_opt expression ';'
    { $$ = MakeTaggedNode(N::kNonblockingAssignmentStatement, $1, $2, $3, $4, $5); }
    /* This rule overlaps with clocking_drive. */
  ;
clocking_drive_only
  : lpvalue TK_LE cycle_delay expression ';'
    { $$ = MakeTaggedNode(N::kNonblockingAssignmentStatement, $1, $2, $3, $4, $5); }
  ;

procedural_continuous_assignment
  : TK_assign lpvalue '=' expression ';'
    { $$ = MakeTaggedNode(N::kProceduralContinuousAssignmentStatement, $1,
                          MakeTaggedNode(N::kNetVariableAssignment, $2, $3, $4),
                          $5); }
  | TK_assign macro_call_or_item
    { $$ = MakeTaggedNode(N::kProceduralContinuousAssignmentStatement, $1, $2); }
    /* allowed because this has been observed in practice */
  | TK_deassign lpvalue ';'
    { $$ = MakeTaggedNode(N::kProceduralContinuousDeassignmentStatement, $1, $2, $3); }
  | TK_force lpvalue '=' expression ';'
    { $$ = MakeTaggedNode(N::kProceduralContinuousForceStatement, $1,
                          MakeTaggedNode(N::kNetVariableAssignment, $2, $3, $4),
                          $5); }
  | TK_release lpvalue ';'
    { $$ = MakeTaggedNode(N::kProceduralContinuousReleaseStatement, $1, $2, $3); }
  ;

case_statement
  /* TODO(jeremycs): maybe add structure */
  /* includes randcase_statement */
  : unique_priority_opt case_any '(' expression ')' case_items TK_endcase
    { $$ = MakeTaggedNode(N::kCaseStatement, $1, $2, MakeParenGroup($3, $4, $5), $6, $7);}
  | unique_priority_opt case_any '(' expression ')' TK_matches
    case_pattern_items TK_endcase
    { $$ = MakeTaggedNode(N::kCaseStatement, $1, $2, MakeParenGroup($3, $4, $5), $6, $7, $8);}
  | unique_priority_opt case_any '(' expression ')' TK_inside
    case_inside_items TK_endcase
    { $$ = MakeTaggedNode(N::kCaseStatement, $1, $2, MakeParenGroup($3, $4, $5), $6, $7, $8);}
    /* $2 should only be TK_case, but case_any avoids S/R conflict. */
  | unique_priority_opt TK_randcase case_items TK_endcase
    { $$ = MakeTaggedNode(N::kRandCaseStatement, $1, $2, $3, $4);}
  /**
  | TK_case '(' expression ')' case_items TK_endcase
  | TK_casex '(' expression ')' case_items TK_endcase
  | TK_casez '(' expression ')' case_items TK_endcase
  **/
  ;

conditional_statement
  : unique_priority_opt TK_if '(' expression ')' statement_or_null
    %prec less_than_TK_else
    { $$ = MakeTaggedNode(N::kConditionalStatement,
             MakeTaggedNode(N::kIfClause,
               MakeTaggedNode(N::kIfHeader, $1, $2, MakeParenGroup($3, $4, $5)),
               MakeTaggedNode(N::kIfBody, $6)));}
  | unique_priority_opt TK_if '(' expression ')' statement_or_null
    TK_else statement_or_null
    { $$ = MakeTaggedNode(N::kConditionalStatement,
             MakeTaggedNode(N::kIfClause,
               MakeTaggedNode(N::kIfHeader, $1, $2, MakeParenGroup($3, $4, $5)),
               MakeTaggedNode(N::kIfBody, $6)),
             MakeTaggedNode(N::kElseClause, $7, MakeTaggedNode(N::kElseBody, $8)));}
  ;

event_trigger
  : TK_TRIGGER reference ';'
    { $$ = MakeTaggedNode(N::kBlockingEventTriggerStatement, $1, $2, $3); }
  | TK_NONBLOCKING_TRIGGER delay_or_event_control_opt reference ';'
    { $$ = MakeTaggedNode(N::kNonblockingEventTriggerStatement,
                          $1, $2, $3, $4); }
  ;

repeat_control
  : TK_repeat '(' expression ')'
    { $$ = MakeTaggedNode(N::kRepeatControl, $1, MakeParenGroup($2, $3, $4)); }
  ;

delay_or_event_control
  : delay1
    { $$ = move($1); }
  | event_control
    { $$ = move($1); }
  | repeat_control event_control
    { $$ = MakeTaggedNode(N::kRepeatEventControl, $1, $2); }
  ;

delay_or_event_control_opt
  : delay_or_event_control
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;

par_block
  : TK_fork label_opt
    /* merged: block_item_decls_opt statement_or_null_list_opt */
    block_item_or_statement_or_null_list_opt
    join_keyword label_opt
    /* TODO(fangism): pair ($1,$2) and ($4,$5) together, like kBegin,kEnd */
    { $$ = MakeTaggedNode(N::kParBlock, $1, $2, $3, $4, $5); }
  ;

procedural_timing_control_statement
  : delay1 statement_or_null
    { $$ = MakeTaggedNode(N::kProceduralTimingControlStatement, $1, $2); }
  | event_control statement_or_null
    { $$ = MakeTaggedNode(N::kProceduralTimingControlStatement, $1, $2); }
  | cycle_delay statement_or_null
    { $$ = MakeTaggedNode(N::kProceduralTimingControlStatement, $1, $2); }
  ;

seq_block
  : begin block_item_or_statement_or_null_list_opt end
    /* merged: block_item_decls_opt statement_or_null_list_opt */
    { $$ = MakeTaggedNode(N::kSeqBlock, $1, $2, $3); }
  ;

wait_statement
  : TK_wait '(' expression ')' statement_or_null
    /* shaped similarly to kIfClause */
    { $$ = MakeTaggedNode(N::kWaitStatement,
                          MakeTaggedNode(N::kWaitHeader,
                                         $1, MakeParenGroup($2, $3, $4)),
                          MakeTaggedNode(N::kWaitBody, $5));}
  | TK_wait TK_fork ';'
    { $$ = MakeTaggedNode(N::kWaitForkStatement, $1, $2, $3);}
    /* TODO(b/144972702): wait_order ... */
  ;

statement_item
  /* TODO(fangism): Some of these may not be valid for both tasks and functions. */
  : blocking_assignment
    { $$ = move($1); }
  | nonblocking_assignment
    { $$ = move($1); }
  | procedural_continuous_assignment
    { $$ = move($1); }
  | case_statement
    { $$ = move($1); }
    /* covers randcase_statement */
  | conditional_statement
    { $$ = move($1); }
  | assignment_statement ';'
    { $$ = ExtendNode($1, $2); }
    /* includes inc_or_dec_expression */
    /* TODO(fangism): expand this from for_step_assignment */
  | disable_statement
    { $$ = move($1); }
  | event_trigger
    { $$ = move($1); }
  | loop_statement
    { $$ = move($1); }
  | jump_statement
    { $$ = move($1); }
  | par_block
    { $$ = move($1); }
  | procedural_timing_control_statement
    { $$ = move($1); }
  | seq_block
    { $$ = move($1); }
  | wait_statement
    { $$ = move($1); }
  | procedural_assertion_statement
    { $$ = move($1); }
  | expect_property_statement
    { $$ = move($1); }
  | clocking_drive_only
    { $$ = move($1); }
  | randsequence_statement
    { $$ = move($1); }
  | subroutine_call ';'
    { $$ = MakeTaggedNode(N::kStatement, $1, $2); }
  /* covered by reference_or_call rule:
  | scoped_hierarchy_identifier '(' argument_list_opt ')' ';'
  | scoped_hierarchy_identifier ';'
  | implicit_class_handle '.' TK_new '(' argument_list_opt ')' ';'
  */
  | randomize_call ';'
    { $$ = MakeTaggedNode(N::kStatement, $1, $2); }
  // seen in the wild:
  | TK_void '\'' '(' expression ')' ';'
  { $$ = MakeTaggedNode(N::kStatement, MakeTaggedNode(N::kVoidcast, $1, $2, MakeParenGroup($3, $4, $5), $6)); }
  | error ';'
    { yyerrok; $$ = Recover(); }
  | MacroGenericItem /* statement that does not end with ; */
    { $$ = move($1); }
  /* reference_or_call already covers: MacroCall ';' */
  /* preprocessor_directive has been split into the following: */
  | preprocessor_balanced_statements
    { $$ = move($1); }
  | preprocessor_action
    { $$ = move($1); }
  ;

preprocessor_balanced_statements
  : preprocessor_if_header block_item_or_statement_or_null_list_opt
    preprocessor_elsif_statements_opt
    preprocessor_else_statement_opt
    PP_endif
    { $$ = MakeTaggedNode(N::kPreprocessorBalancedStatements,
                          ExtendNode($1, $2), ForwardChildren($3), $4, $5);
    }
  ;
preprocessor_elsif_statements_opt
  : preprocessor_elsif_statements
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
preprocessor_elsif_statements
  : preprocessor_elsif_statements preprocessor_elsif_statement
    { $$ = ExtendNode($1, $2); }
  | preprocessor_elsif_statement
    { $$ = MakeNode($1); }  /* Don't bother tagging; node will be flattened. */
  ;
preprocessor_elsif_statement
  : preprocessor_elsif_header block_item_or_statement_or_null_list_opt
    { $$ = ExtendNode($1, $2); }
  ;
preprocessor_else_statement_opt
  : preprocessor_else_statement
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
preprocessor_else_statement
  : PP_else block_item_or_statement_or_null_list_opt
    { $$ = MakeTaggedNode(N::kPreprocessorElseClause, $1, $2); }
  ;

disable_statement
  : TK_disable reference ';'
    { $$ = MakeTaggedNode(N::kDisableStatement, $1, $2, $3); }
  | TK_disable TK_fork ';'
    { $$ = MakeTaggedNode(N::kDisableStatement, $1, $2, $3); }
  ;
assign_modify_statement
  : lpvalue TK_PLUS_EQ expression
    { $$ = MakeTaggedNode(N::kAssignModifyStatement, $1, $2, $3); }
  | lpvalue TK_MINUS_EQ expression
    { $$ = MakeTaggedNode(N::kAssignModifyStatement, $1, $2, $3); }
  | lpvalue TK_MUL_EQ expression
    { $$ = MakeTaggedNode(N::kAssignModifyStatement, $1, $2, $3); }
  | lpvalue TK_DIV_EQ expression
    { $$ = MakeTaggedNode(N::kAssignModifyStatement, $1, $2, $3); }
  | lpvalue TK_MOD_EQ expression
    { $$ = MakeTaggedNode(N::kAssignModifyStatement, $1, $2, $3); }
  | lpvalue TK_AND_EQ expression
    { $$ = MakeTaggedNode(N::kAssignModifyStatement, $1, $2, $3); }
  | lpvalue TK_OR_EQ expression
    { $$ = MakeTaggedNode(N::kAssignModifyStatement, $1, $2, $3); }
  | lpvalue TK_XOR_EQ expression
    { $$ = MakeTaggedNode(N::kAssignModifyStatement, $1, $2, $3); }
  | lpvalue TK_LS_EQ expression
    { $$ = MakeTaggedNode(N::kAssignModifyStatement, $1, $2, $3); }
  | lpvalue TK_RS_EQ expression
    { $$ = MakeTaggedNode(N::kAssignModifyStatement, $1, $2, $3); }
  | lpvalue TK_RSS_EQ expression
    { $$ = MakeTaggedNode(N::kAssignModifyStatement, $1, $2, $3); }
  ;
unique_priority_opt
  : TK_unique
    { $$ = move($1); }
  | TK_unique0
    { $$ = move($1); }
  | TK_priority
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
statement_or_null_list_opt
  : statement_or_null_list
    { $$ = move($1); }
  | /* empty */
    { $$ = MakeTaggedNode(N::kStatementList); }
  ;
statement_or_null_list
  : statement_or_null_list statement_or_null
    { $$ = ExtendNode($1, $2); }
  | statement_or_null
    { $$ = MakeTaggedNode(N::kStatementList, $1); }
  ;
analog_statement
  : branch_probe_expression TK_CONTRIBUTE expression ';'
    { $$ = MakeTaggedNode(N::kAnalogStatement, $1, $2, $3, $4); }
  ;
/* same as function_item */
task_item
  : block_item_decl
    { $$ = move($1); }
  | tf_port_declaration
    { $$ = move($1); }
  ;
/** merged with into tf_item_or_statement_or_null_list
task_item_list
  : task_item_list task_item
  | task_item
  ;
task_item_list_opt
  : task_item_list
  | // empty
  ;
**/
/* introduced to simplify grammar, reduce conflicts */
tf_item_or_statement_or_null
  : task_item
    { $$ = move($1); }
  | statement_or_null
    { $$ = move($1); }
  ;
tf_item_or_statement_or_null_list
  /* TODO(jeremycs): unclear if this should have its own enum */
  : tf_item_or_statement_or_null
    { $$ = MakeTaggedNode(N::kStatementList, $1); }
  | tf_item_or_statement_or_null_list tf_item_or_statement_or_null
    { $$ = ExtendNode($1, $2); }
  ;
tf_item_or_statement_or_null_list_opt
  : tf_item_or_statement_or_null_list
    { $$ = move($1); }
  | /* empty */
    { $$ = MakeTaggedNode(N::kStatementList); }
  ;

tf_port_list_paren_opt
  : '(' tf_port_list_opt ')'
    { $$ = MakeParenGroup($1, $2, $3); }
  | /* empty */
    { $$ = nullptr; }
  ;
tf_port_list_opt
  : tf_port_list
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }

  ;
udp_body
  : TK_table udp_entry_list TK_endtable
    { $$ = MakeTaggedNode(N::kUdpBody, $1, $2, $3); }
  | TK_table TK_endtable
    { $$ = MakeTaggedNode(N::kUdpBody, $1, nullptr, $2); }
/**
  | TK_table error TK_endtable
**/
  ;
udp_entry_list
  : udp_comb_entry_list
    { $$ = move($1); }
  | udp_sequ_entry_list
    { $$ = move($1); }
  | udp_unknown_list
    { $$ = move($1); }
  ;
udp_unknown_list
  : udp_unknown_list preprocessor_directive
    { $$ = ExtendNode($1, $2); }
  | preprocessor_directive
    { $$ = MakeTaggedNode(N::kUdpEntryList, $1); }
  ;
udp_comb_entry
  : udp_input_list ':' udp_output_sym ';'
    { $$ = MakeTaggedNode(N::kUdpCombEntry, $1, $2, $3, $4); }
  ;
udp_comb_entry_list
  : udp_comb_entry
    { $$ = MakeTaggedNode(N::kUdpEntryList, $1); }
  | udp_comb_entry_list udp_comb_entry
    { $$ = ExtendNode($1, $2); }
  | udp_comb_entry_list preprocessor_directive
    { $$ = ExtendNode($1, $2); }
  | udp_unknown_list udp_comb_entry
    { $$ = ExtendNode($1, $2); }
  ;
udp_sequ_entry_list
  : udp_sequ_entry
    { $$ = MakeTaggedNode(N::kUdpEntryList, $1); }
  | udp_sequ_entry_list udp_sequ_entry
    { $$ = ExtendNode($1, $2); }
  | udp_sequ_entry_list preprocessor_directive
    { $$ = ExtendNode($1, $2); }
  | udp_unknown_list udp_sequ_entry
    { $$ = ExtendNode($1, $2); }
  ;
udp_sequ_entry
  : udp_input_list ':' udp_input_sym ':' udp_output_sym ';'
    { $$ = MakeTaggedNode(N::kUdpSequenceEntry, $1, $2, $3, $4, $5, $6); }
  ;
udp_initial
  : TK_initial GenericIdentifier '=' number ';'
    { $$ = MakeTaggedNode(N::kUdpInitial, $1, $2, $3, $4, $5); }
  ;
udp_init_opt
  : udp_initial
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
udp_input_list
  : udp_input_sym
    { $$ = MakeTaggedNode(N::kUdpInputList, $1); }
  | udp_input_list udp_input_sym
    { $$ = ExtendNode($1, $2); }
  ;
udp_input_sym
  : '0'
    { $$ = move($1); }
  | '1'
    { $$ = move($1); }
  | 'x'
    { $$ = move($1); }
  | '?'
    { $$ = move($1); }
  | 'b'
    { $$ = move($1); }
  | '*'
    { $$ = move($1); }
  | '%'
    { $$ = move($1); }
  | 'f'
    { $$ = move($1); }
  | 'F'
    { $$ = move($1); }
  | 'l'
    { $$ = move($1); }
  | 'h'
    { $$ = move($1); }
  | 'B'
    { $$ = move($1); }
  | 'r'
    { $$ = move($1); }
  | 'R'
    { $$ = move($1); }
  | 'M'
    { $$ = move($1); }
  | 'n'
    { $$ = move($1); }
  | 'N'
    { $$ = move($1); }
  | 'p'
    { $$ = move($1); }
  | 'P'
    { $$ = move($1); }
  | 'Q'
    { $$ = move($1); }
  | 'q'
    { $$ = move($1); }
  | '_'
    { $$ = move($1); }
  | '+'
    { $$ = move($1); }
  | TK_DecNumber
    { $$ = move($1); }
  ;
udp_output_sym
  : '0'
    { $$ = move($1); }
  | '1'
    { $$ = move($1); }
  | 'x'
    { $$ = move($1); }
  | '-'
    { $$ = move($1); }
  | TK_DecNumber
    { $$ = move($1); }
  ;
udp_port_decl
  : TK_input list_of_identifiers ';'
    { $$ = MakeTaggedNode(N::kUdpPortDeclaration, nullptr, $1, $2, $3); }
  | TK_output GenericIdentifier ';'
    { $$ = MakeTaggedNode(N::kUdpPortDeclaration, nullptr, $1, $2, $3); }
  | TK_reg GenericIdentifier ';'
    { $$ = MakeTaggedNode(N::kUdpPortDeclaration, $1, nullptr, $2, $3); }
  | TK_reg TK_output GenericIdentifier ';'
    { $$ = MakeTaggedNode(N::kUdpPortDeclaration, $1, $2, $3, $4); }
  ;
udp_port_decls
  : udp_port_decl
    { $$ = MakeTaggedNode(N::kUdpPortDeclarationList, $1); }
  | udp_port_decls udp_port_decl
    { $$ = ExtendNode($1, $2); }
  ;
udp_port_list
  : GenericIdentifier
    { $$ = MakeTaggedNode(N::kUdpPortList, $1); }
  | udp_port_list ',' GenericIdentifier
    { $$ = ExtendNode($1, $2, $3); }
  ;
udp_initial_expr_opt
  : '=' expression
    { $$ = MakeTaggedNode(N::kTrailingAssign, $1, $2); }
  | /* empty */
    { $$ = nullptr; }
  ;
udp_input_declaration_list
  : TK_input GenericIdentifier
    { $$ = MakeTaggedNode(N::kUdpInputDeclarationList, $1, $2); }
  | udp_input_declaration_list ',' TK_input GenericIdentifier
    { $$ = ExtendNode($1, $2, $3, $4); }
  ;
udp_primitive
  : TK_primitive GenericIdentifier '(' udp_port_list ')' ';'
      udp_port_decls
      udp_init_opt
      udp_body
    TK_endprimitive label_opt
    { $$ = MakeTaggedNode(N::kUdpPrimitive, $1, $2, MakeParenGroup($3, $4, $5), $6,
                          $7, $8, $9, $10, $11); }
  | TK_primitive GenericIdentifier
      '(' TK_output TK_reg_opt GenericIdentifier udp_initial_expr_opt ','
      udp_input_declaration_list ')' ';'
      udp_body
    TK_endprimitive label_opt
    { $$ = MakeTaggedNode(N::kUdpPrimitive, $1, $2,
                          MakeParenGroup($3, MakeNode($4, $5, $6, $7, $8, $9), $10),
                          $11, $12, $13, $14); }
  ;
lifetime
  : TK_automatic
    { $$ = move($1); }
  | TK_static
    { $$ = move($1); }
  ;
lifetime_opt
  : lifetime
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
TK_reg_opt
  : TK_reg
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
TK_static_opt
  : TK_static
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
TK_tagged_opt
  : TK_tagged
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
TK_virtual_opt
  : TK_virtual
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;

bind_directive
  : TK_bind reference ':' bind_target_instance_list bind_instantiation ';'
    { $$ = MakeTaggedNode(N::kBindDirective, $1, $2, $3, $4, $5, $6); }
    /* $2 is target scope (module or interface),
     * and should be GenericIdentifier.
     */
  | TK_bind reference bind_instantiation ';'
    { $$ = MakeTaggedNode(N::kBindDirective, $1, $2, nullptr, nullptr, $3, $4); }
    /* if $2 is target scope (module or interface),
     * then $2 should be a GenericIdentifier,
     * if $2 is a bind_target_scope, then it may be hierarchical.
     */
  ;
bind_target_instance_list
  : bind_target_instance_list ',' bind_target_instance
    { $$ = ExtendNode($1, $2, $3); }
  | bind_target_instance
    { $$ = MakeTaggedNode(N::kBindTargetInstanceList, $1); }
  ;
bind_target_instance
  : reference
    { $$ = move($1); }
    /* Any bit selections in []'s must be constant expressions. */
  ;
bind_instantiation
  /* similar to block_item_decl instantiation */
  : class_id gate_instance_or_register_variable_list
    { $$ = MakeTaggedNode(N::kBindTargetInstance, $1, $2); }
    /* covers: program, module, interface, checker instantiation */
  ;

clocking_declaration
  /* TODO(b/19573356): Introduce CodeBlock::CLOCKING. */
  : TK_default TK_clocking identifier_opt event_control ';'
    clocking_item_list_opt
    TK_endclocking label_opt
    { $$ = MakeTaggedNode(N::kClockingDeclaration, $1, $2, $3, $4, $5, $6, $7, $8); }
    /* $4 event_control should be @identifier or @(event_expr) */
  | TK_clocking identifier_opt event_control ';'
    clocking_item_list_opt
    TK_endclocking label_opt
    { $$ = MakeTaggedNode(N::kClockingDeclaration, nullptr, $1, $2, $3, $4, $5, $6, $7); }
    /* $4 event_control should be @identifier or @(event_expr) */
  | TK_global TK_clocking identifier_opt event_control ';'
    TK_endclocking label_opt
    { $$ = MakeTaggedNode(N::kClockingDeclaration, $1, $2, $3, $4, $5, $6, nullptr, $7); }
  ;
clocking_item_list_opt
  : clocking_item_list
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
clocking_item_list
  : clocking_item_list clocking_item
    { $$ = ExtendNode($1, $2); }
  | clocking_item
    { $$ = MakeTaggedNode(N::kClockingItemList, $1); }
  ;
clocking_item
  : TK_default default_skew ';'
    { $$ = MakeTaggedNode(N::kClockingItem, $1, $2, $3); }
  | clocking_direction list_of_clocking_decl_assign ';'
    { $$ = MakeTaggedNode(N::kClockingItem, $1, $2, $3); }
  | /* attribute_list_opt */ assertion_item_declaration
    { $$ = move($1); }
  | let_declaration
    { $$ = move($1); }
  ;
default_skew
  : TK_input clocking_skew
    { $$ = MakeTaggedNode(N::kDefaultSkew, $1, $2);}
  | TK_input clocking_skew TK_output clocking_skew
    { $$ = MakeTaggedNode(N::kDefaultSkew, $1, $2, $3, $4);}
  | TK_output clocking_skew
    { $$ = MakeTaggedNode(N::kDefaultSkew, $1, $2);}
  ;
clocking_skew_opt
  : clocking_skew
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
clocking_skew
  : edge_operator delay3_opt
    { $$ = MakeTaggedNode(N::kClockingSkew, $1, $2);}
  | delay3
    { $$ = MakeTaggedNode(N::kClockingSkew, $1);}
  ;
clocking_direction
  : TK_input clocking_skew_opt
    { $$ = MakeTaggedNode(N::kClockingDirection, $1, $2); }
  | TK_input clocking_skew_opt TK_output clocking_skew_opt
    { $$ = MakeTaggedNode(N::kClockingDirection, $1, $2, $3, $4); }
  | TK_output clocking_skew_opt
    { $$ = MakeTaggedNode(N::kClockingDirection, $1, $2); }
  | TK_inout
    { $$ = MakeTaggedNode(N::kClockingDirection, $1); }
  ;
list_of_clocking_decl_assign
  : list_of_clocking_decl_assign ',' clocking_decl_assign
    { $$ = ExtendNode($1, $2, $3); }
  | clocking_decl_assign
    { $$ = MakeTaggedNode(N::kClockingAssignList, $1); }
  ;
clocking_decl_assign
  : GenericIdentifier trailing_assign_opt
    { $$ = MakeTaggedNode(N::kClockingAssign, $1, $2); }
    /* $1 is signal_identifier */
  ;

assertion_item_declaration
  : property_declaration
    { $$ = move($1); }
  | sequence_declaration
    { $$ = move($1); }
  /* moved to other rules to avoid R/R conflict
  | let_declaration
  */
  ;

let_declaration
  : TK_let GenericIdentifier let_port_list_in_parens_opt '=' expression ';'
    { $$ = MakeTaggedNode(N::kLetDeclaration, $1, $2, $3, $4, $5, $6); }
  ;
let_port_list_in_parens_opt
  : '(' let_port_list ')'
    { $$ = MakeParenGroup($1, $2, $3); }
  | '(' ')'
    { $$ = MakeParenGroup($1, nullptr, $2); }
  | /* empty */
    { $$ = nullptr; }
  ;
let_port_list
  : let_port_list ',' let_port_item
    { $$ = ExtendNode($1, $2, $3); }
  | let_port_item
    { $$ = MakeTaggedNode(N::kLetPortList, $1); }
  ;
let_port_item
  : let_formal_type_followed_by_id decl_dimensions_opt
    /* $2 is not limited to being an unpacked dimension */
    { $$ = MakeTaggedNode(N::kLetPortItem, ForwardChildren($1), $2); }
  | let_formal_type_followed_by_id decl_dimensions_opt '=' expression
    { $$ = MakeTaggedNode(N::kLetPortItem, ForwardChildren($1), $2, $3, $4); }
  ;
let_formal_type_followed_by_id
  /* similar to property_formal_type_followed_by_id */
  : data_type_or_implicit_basic_followed_by_id
    { $$ = move($1); }
  | TK_untyped GenericIdentifier
    { $$ = MakeTaggedNode(N::kDataTypeImplicitBasicId, $1, $2); }
  ;

sequence_declaration
  : TK_sequence GenericIdentifier
    sequence_port_list_in_parens_opt ';'
    assertion_variable_declaration_list
    SemicolonEndOfAssertionVariableDeclarations
    sequence_expr optional_semicolon
    TK_endsequence label_opt
    { $$ = MakeTaggedNode(N::kSequenceDeclaration,
                          $1, $2, $3, $4, ExtendNode($5, $6),
                          MakeTaggedNode(N::kSequenceDeclarationFinalExpr, $7, $8),
                          $9, $10); }
  | TK_sequence GenericIdentifier
    sequence_port_list_in_parens_opt
    SemicolonEndOfAssertionVariableDeclarations
    sequence_expr optional_semicolon
    TK_endsequence label_opt
    { $$ = MakeTaggedNode(N::kSequenceDeclaration,
                          $1, $2, $3, $4, nullptr,
                          MakeTaggedNode(N::kSequenceDeclarationFinalExpr, $5, $6),
                          $7, $8); }
  ;

sequence_port_list_in_parens_opt
  : '(' sequence_port_list_opt ')'
    { $$ = MakeParenGroup($1, $2, $3); }
  | /* empty */
    { $$ = nullptr; }
  ;
sequence_port_list_opt
  : sequence_port_list
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
sequence_port_list
  : sequence_port_list ',' sequence_port_item
    { $$ = ExtendNode($1, $2, $3); }
  | sequence_port_item
    { $$ = MakeTaggedNode(N::kSequencePortList, $1); }
  ;
sequence_port_item
  : /* attribute_list_opt */
    local_sequence_lvar_port_direction_opt
    sequence_port_type_followed_by_id decl_dimensions_opt trailing_assign_opt
    /* $3 is not limited to being an unpacked dimension */
    { $$ = MakeTaggedNode(N::kSequencePortItem, $1, $2, $3, $4); }
  ;
local_sequence_lvar_port_direction_opt
  : TK_local dir
    { $$ = MakeNode($1, $2); }
  | TK_local
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
sequence_port_type_followed_by_id
  /* similar to property_formal_type_followed_by_id */
  : data_type_or_implicit_basic_followed_by_id
    { $$ = move($1); }
  | TK_sequence GenericIdentifier
    { $$ = MakeTaggedNode(N::kDataTypeImplicitBasicId, $1, $2); }
  | TK_untyped GenericIdentifier
    { $$ = MakeTaggedNode(N::kDataTypeImplicitBasicId, $1, $2); }
  ;

property_declaration
  : TK_property GenericIdentifier property_port_list_in_parens_opt ';'
    assertion_variable_declaration_list
    SemicolonEndOfAssertionVariableDeclarations
    property_spec optional_semicolon
    TK_endproperty label_opt
    { $$ = MakeTaggedNode(N::kPropertyDeclaration,
                          $1, $2, $3, $4, ExtendNode($5, $6),
                          ExtendNode($7, $8), $9, $10); }
  | TK_property GenericIdentifier property_port_list_in_parens_opt
    SemicolonEndOfAssertionVariableDeclarations
    property_spec optional_semicolon
    TK_endproperty label_opt
    { $$ = MakeTaggedNode(N::kPropertyDeclaration,
                          $1, $2, $3, $4, nullptr,
                          ExtendNode($5, $6), $7, $8); }
  ;
property_port_list_in_parens_opt
  : '(' property_port_list ')'
    { $$ = MakeParenGroup($1, $2, $3); }
  | /* empty */
    { $$ = nullptr; }
  ;
property_port_list
  : property_port_list ',' property_port_item
    { $$ = ExtendNode($1, $2, $3); }
  | property_port_item
    { $$ = MakeTaggedNode(N::kPropertyPortList, $1); }
  ;
property_port_item
  : /* attribute_list_opt */
    property_port_modifiers_opt
    property_formal_type_followed_by_id
    /* equivalent to: property_formal_type GenericIdentifier,
     * but written as such to allow for implicit types without conflict.
     */
    decl_dimensions_opt
    property_actual_arg_opt
    /* $3 is not limited to being an unpacked dimension */
    { $$ = MakeTaggedNode(N::kPropertyPortItem, $1, $2, $3, $4); }
  ;
property_port_modifiers_opt
  : TK_local TK_input
    { $$ = MakeTaggedNode(N::kPropertyPortModifierList, $1, $2); }
  | TK_local
    { $$ = MakeTaggedNode(N::kPropertyPortModifierList, $1); }
  | /* empty */
    { $$ = nullptr; }
  ;
property_formal_type_followed_by_id
  /* similar to sequence_port_type_followed_by_id */
  : data_type_or_implicit_basic_followed_by_id
    { $$ = move($1); }
  | TK_sequence GenericIdentifier
    { $$ = MakeTaggedNode(N::kDataTypeImplicitBasicId, $1, $2); }
  | TK_untyped GenericIdentifier
    { $$ = MakeTaggedNode(N::kDataTypeImplicitBasicId, $1, $2); }
  | TK_property GenericIdentifier
    { $$ = MakeTaggedNode(N::kDataTypeImplicitBasicId, $1, $2); }
  ;
property_actual_arg_opt
  : '=' property_actual_arg
    { $$ = MakeTaggedNode(N::kTrailingAssign, $1, $2); }
  | /* empty */
    { $$ = nullptr; }
  ;
assertion_variable_declaration_list
  : assertion_variable_declaration_list ';' assertion_variable_declaration
    { $$ = ExtendNode($1, $2, $3); }
  | assertion_variable_declaration
    { $$ = MakeTaggedNode(N::kAssertionVariableDeclarationList, $1); }
  ;
assertion_variable_declaration
  : var_opt data_type_or_implicit_basic_followed_by_id_and_dimensions_opt
    trailing_assign_opt
    { $$ = MakeTaggedNode(N::kAssertionVariableDeclaration, $1, $2, $3); }
  | var_opt data_type_or_implicit_basic_followed_by_id_and_dimensions_opt
    trailing_assign_opt ',' net_variable_or_decl_assigns
    { $$ = MakeTaggedNode(N::kAssertionVariableDeclaration, $1, $2, $3, $4, $5); }
    // TODO(fangism): re-pack $3
  ;

property_actual_arg
  /* Both event_expression and sequence_expr cover plain expression, so
   * to avoid conflict, we expand event_expression without the lone expression rule.
   */
  : edge_operator expression
    { $$ = MakeTaggedNode(N::kPropertyActualArg, $1, $2); }
    /* from event_expression */
  | property_expr
    { $$ = MakeTaggedNode(N::kPropertyActualArg, $1); }
  ;


/* sequence expression operator precedence (high-to-low):
 *   repetition [*] [=] [->]
 *   delay ##
 *   throughout
 *   within
 *   intersect
 *   and
 *   or
 *
 * property expression operator precedence (high-to-low):
 *   not
 *   and
 *   or
 *   if-else
 *   |-> |=> (implication)
 */
/*
 * The language spec contains rules (same with TK_or):
 *   property_expr : property_expr TK_and property_expr
 *   property_expr : sequence_expr
 *   sequence_expr : sequence_expr TK_and_sequence_expr
 * Thus, without context, property_expr and sequence_expr become entangled.
 */
property_expr
  : sequence_expr
    { $$ = move($1); }
  ;
sequence_expr
  /* merged with property_expr */
  : property_implication_expr
    { $$ = move($1); }
  ;

property_prefix_expr
  // TODO(fangism): distinguish different property_prefix_exprs
  : TK_accept_on '(' expression_or_dist ')' property_prefix_expr
    { $$ = MakeTaggedNode(N::kPropertyPrefixExpression, $1, MakeParenGroup($2, $3, $4), $5); }
  | TK_reject_on '(' expression_or_dist ')' property_prefix_expr
    { $$ = MakeTaggedNode(N::kPropertyPrefixExpression, $1, MakeParenGroup($2, $3, $4), $5); }
  | TK_sync_accept_on '(' expression_or_dist ')' property_prefix_expr
    { $$ = MakeTaggedNode(N::kPropertyPrefixExpression, $1, MakeParenGroup($2, $3, $4), $5); }
  | TK_sync_reject_on '(' expression_or_dist ')' property_prefix_expr
    { $$ = MakeTaggedNode(N::kPropertyPrefixExpression, $1, MakeParenGroup($2, $3, $4), $5); }

  /* TODO(fangism):
  // like sequence_instance, this looks like a function/subroutine call.
  | property_instance
  */
  /* temporal_property_expr: */
  | TK_nexttime property_prefix_expr
    { $$ = MakeTaggedNode(N::kPropertyPrefixExpression, $1, nullptr, $2); }
  | TK_nexttime '[' expression ']' property_prefix_expr
    /* $3 is a constant_expression */
    { $$ = MakeTaggedNode(N::kPropertyPrefixExpression, $1,
                          MakeTaggedNode(N::kPropertyExpressionIndex, $2, $3, $4),
                          $5); }
  | TK_s_nexttime property_prefix_expr
    { $$ = MakeTaggedNode(N::kPropertyPrefixExpression, $1, nullptr, $2); }
  | TK_s_nexttime '[' expression ']' property_prefix_expr
    /* $3 is a constant_expression */
    { $$ = MakeTaggedNode(N::kPropertyPrefixExpression, $1,
                          MakeTaggedNode(N::kPropertyExpressionIndex, $2, $3, $4),
                          $5); }
  | TK_always property_prefix_expr
    { $$ = MakeTaggedNode(N::kPropertyPrefixExpression, $1, nullptr, $2); }
  | TK_always '[' cycle_range ']' property_prefix_expr
    /* $3 is a cycle_delay_const_range_expression */
    { $$ = MakeTaggedNode(N::kPropertyPrefixExpression, $1,
                          MakeTaggedNode(N::kCycleDelayConstRange, $2, $3, $4),
                          $5); }
  | TK_s_always '[' cycle_range ']' property_prefix_expr
    /* $3 should be a constant_range */
    { $$ = MakeTaggedNode(N::kPropertyPrefixExpression, $1,
                          MakeTaggedNode(N::kCycleDelayConstRange, $2, $3, $4),
                          $5); }
  | TK_s_eventually property_prefix_expr
    { $$ = MakeTaggedNode(N::kPropertyPrefixExpression, $1, nullptr, $2); }
  | TK_eventually '[' cycle_range ']'  property_prefix_expr
    /* $3 should be a constant_range */
    { $$ = MakeTaggedNode(N::kPropertyPrefixExpression, $1,
                          MakeTaggedNode(N::kCycleDelayConstRange, $2, $3, $4),
                          $5); }
  | TK_s_eventually '[' cycle_range ']'  property_prefix_expr
    /* $3 should be a cycle_delay_const_range_expression */
    { $$ = MakeTaggedNode(N::kPropertyPrefixExpression, $1,
                          MakeTaggedNode(N::kCycleDelayConstRange, $2, $3, $4),
                          $5); }
  | property_if_else_expr
   { $$ = move($1); }
  ;

property_if_else_expr
  : TK_if '(' expression_or_dist ')' property_prefix_expr
    TK_else property_prefix_expr
    { $$ = MakeTaggedNode(N::kPropertyIfElse, $1,
                          MakeParenGroup($2, $3, $4), $5, $6, $7); }
  | TK_if '(' expression_or_dist ')' property_prefix_expr
    %prec less_than_TK_else
    { $$ = MakeTaggedNode(N::kPropertyIfElse, $1, MakeParenGroup($2, $3, $4), $5); }
  | simple_sequence_expr
    { $$ = move($1); }
  ;

simple_sequence_expr
  // TODO: distinguish different sequence expressions' tags
  /* cannot have trailing if */
  : TK_first_match '(' sequence_expr_match_item_list ')'
    { $$ = MakeTaggedNode(N::kPropertySimpleSequenceExpression, $1,
                          MakeParenGroup($2, $3, $4)); }
  /* TODO(fangism): Resolve conflict on sequence_instance.
   * sequence_instance looks like function/subroutine call,
   * already covered by expression.
  | sequence_instance sequence_abbrev_opt
   */
  | property_case_statement
    { $$ = move($1); }
  | TK_strong '(' sequence_expr ')'
    { $$ = MakeTaggedNode(N::kPropertySimpleSequenceExpression, $1,
                          MakeParenGroup($2, $3, $4)); }
  | TK_weak '(' sequence_expr ')'
    { $$ = MakeTaggedNode(N::kPropertySimpleSequenceExpression, $1,
                          MakeParenGroup($2, $3, $4)); }
  | sequence_or_expr
    { $$ = move($1); }
  ;

property_case_statement
  : TK_case '(' expression_or_dist ')'
    property_case_item_list optional_semicolon
    TK_endcase
    { $$ = MakeTaggedNode(N::kPropertyCaseStatement, $1, MakeParenGroup($2, $3, $4),
                          $5, $6, $7); }
  ;
property_case_item_list
  : property_case_item_list ';' property_case_item
    { $$ = ExtendNode($1, $2, $3); }
  | property_case_item
    { $$ = MakeTaggedNode(N::kPropertyCaseItemList, $1); }
  ;
/* Language spec allows semicolons to be optional following each property_case_item,
 * but without a required separator, the grammar becomes unparseable.
 * We still allow the last semicolon to be optional.
 */
property_case_item
  : expression_or_dist_list ':' property_expr
    { $$ = MakeTaggedNode(N::kPropertyCaseItem, $1, $2, $3); }
  | TK_default ':' property_expr
    { $$ = MakeTaggedNode(N::kPropertyDefaultItem, $1, $2, $3); }
  | TK_default property_expr
    { $$ = MakeTaggedNode(N::kPropertyDefaultItem, $1, $2); }
  ;
expression_or_dist_list
  : expression_or_dist_list ',' expression_or_dist
    { $$ = ExtendNode($1, $2, $3); }
  | expression_or_dist
    { $$ = MakeTaggedNode(N::kExpressionDistributionList, $1); }
  ;

property_implication_expr
  : property_implication_expr property_operator property_prefix_expr
    { $$ = ExtendNode($1, $2, $3); }
  | property_prefix_expr
    { $$ = IsExpression($1) ? move($1)
                            : MakeTaggedNode(N::kPropertyImplicationList, $1); }
  ;

property_operator
  /* TODO(fangism): order these operators by precedence. */
    /* Could not find good reference about precedence of followed-by operators. */
  : implication_operator
    { $$ = move($1); }
  | until_operator
    { $$ = move($1); }
  | followed_by_operator
    { $$ = move($1); }
  ;
implication_operator
  : TK_PIPEARROW
    { $$ = move($1); }
  | TK_PIPEARROW2
    { $$ = move($1); }
  | TK_implies
    { $$ = move($1); }
  | TK_iff
    { $$ = move($1); }
  ;
until_operator
  : TK_until
    { $$ = move($1); }
  | TK_until_with
    { $$ = move($1); }
  | TK_s_until
    { $$ = move($1); }
  | TK_s_until_with
    { $$ = move($1); }
  ;
followed_by_operator
  : TK_POUNDEQPOUND
    { $$ = move($1); }
  | TK_POUNDMINUSPOUND
    { $$ = move($1); }
  ;


system_tf_call
  /* This also covers constant_function_subroutine_call. */
  : SystemTFIdentifier '(' argument_list_opt ')'
    { $$ = MakeTaggedNode(N::kSystemTFCall, $1, MakeParenGroup($2, $3, $4)); }
  | SystemTFIdentifier
    { $$ = MakeTaggedNode(N::kSystemTFCall, $1); }
    /* Some system tasks can be 'called' without ()-arguments. */
  ;

sequence_match_item_list
  : sequence_match_item_list ',' sequence_match_item
    { $$ = ExtendNode($1, $2, $3); }
  | sequence_match_item
    { $$ = MakeTaggedNode(N::kSequenceMatchItemList, $1); }

  ;
sequence_expr_match_item_list
  : property_expr ',' sequence_match_item_list
    { $$ = MakeTaggedNode(N::kSequenceMatchItemList, $1, $2,
                          ForwardChildren($3)); }
  | property_expr
    { $$ = MakeTaggedNode(N::kSequenceMatchItemList, $1); }
  ;
sequence_match_item
  : assignment_statement
    { $$ = move($1); }
  | subroutine_call
    { $$ = move($1); }
  ;
subroutine_call
  : reference_or_call
    { $$ = move($1); }
  | system_tf_call
    { $$ = move($1); }
  ;

// TODO(jeremycs): unclear if flattening is correct approach here
sequence_or_expr
  : sequence_or_expr TK_or sequence_and_expr
    { $$ = MakeBinaryExpression($1, $2, $3); }
  | sequence_and_expr
    { $$ = IsExpression($1) ? move($1) : move($1); }
  ;
sequence_and_expr
  : sequence_and_expr TK_and sequence_unary_expr
    { $$ = MakeBinaryExpression($1, $2, $3); }
  | sequence_unary_expr
    { $$ = IsExpression($1) ? move($1) : move($1); }
sequence_unary_expr
  : sequence_intersect_expr
    { $$ = IsExpression($1) ? move($1) : move($1); }
  | TK_not sequence_intersect_expr
    { $$ = MakeTaggedNode(N::kUnaryPrefixExpression, $1, $2); }
    /* only for property_expr */
  ;
sequence_intersect_expr
  : sequence_within_expr
    { $$ = IsExpression($1) ? move($1) : move($1); }
  | sequence_intersect_expr TK_intersect sequence_within_expr
    { $$ = MakeBinaryExpression($1, $2, $3); }
  ;
sequence_within_expr
  : sequence_throughout_expr
    { $$ = IsExpression($1) ? move($1) : move($1); }
  | sequence_within_expr TK_within sequence_throughout_expr
    { $$ = MakeBinaryExpression($1, $2, $3); }
  ;
sequence_throughout_expr
  : sequence_delay_range_expr
    { $$ = IsExpression($1) ? move($1) : move($1); }
  | sequence_throughout_expr TK_throughout sequence_delay_range_expr
    { $$ = MakeBinaryExpression($1, $2, $3); }
  ;

sequence_delay_range_expr
  : sequence_delay_repetition_list
    { $$ = IsExpression($1) ? move($1) : move($1); }
  | cycle_delay_range sequence_delay_repetition_list
    { $$ = MakeTaggedNode(N::kSequenceDelayRange, $1, $2); }
  ;
sequence_delay_repetition_list
  : sequence_delay_repetition_list cycle_delay_range sequence_expr_primary
    { $$ = MakeTaggedNode(N::kSequenceDelayRepetition, $1, $2, $3); }
  | sequence_expr_primary
    { $$ = IsExpression($1) ? move($1) : move($1); }
  ;

cycle_delay
  : TK_POUNDPOUND TK_DecNumber
    { $$ = MakeTaggedNode(N::kCycleDelay, $1, $2); }
  | TK_POUNDPOUND GenericIdentifier
    { $$ = MakeTaggedNode(N::kCycleDelay, $1, $2); }
  | TK_POUNDPOUND '(' expression ')'
    { $$ = MakeTaggedNode(N::kCycleDelay, $1, MakeParenGroup($2, $3, $4)); }
  ;

cycle_delay_range
  : TK_POUNDPOUND '[' cycle_range ']'
    { $$ = MakeTaggedNode(N::kCycleDelayRange, $1, MakeBracketGroup($2, $3, $4)); }
  | TK_POUNDPOUND TK_LBSTARRB
    { $$ = MakeTaggedNode(N::kCycleDelayRange, $1, $2); }
    /* but not lb_star_rb */
  | TK_POUNDPOUND TK_LBPLUSRB
    { $$ = MakeTaggedNode(N::kCycleDelayRange, $1, $2); }
  | TK_POUNDPOUND '(' expression ')'
    { $$ = MakeTaggedNode(N::kCycleDelayRange, $1, MakeParenGroup($2, $3, $4)); }
    /* $2 expression should be constant. */
  | TK_POUNDPOUND GenericIdentifier
    { $$ = MakeTaggedNode(N::kCycleDelayRange, $1, $2); }
  | TK_POUNDPOUND TK_DecNumber
    { $$ = MakeTaggedNode(N::kCycleDelayRange, $1, $2); }
  ;
cycle_range_or_expr
  /* this covers repeat_range */
  : cycle_range
    { $$ = move($1); }
  | expression
    { $$ = move($1); }
  ;
cycle_range
  /* For constant expressions, $3 can be '$'.  */
  : expression ':' expression
    { $$ = MakeTaggedNode(N::kCycleRange, $1, $2, $3); }
  ;
dist_opt
  : TK_dist '{' dist_list '}'
    { $$ = MakeTaggedNode(N::kDistribution, expression_placeholder,
                          $1, MakeBraceGroup($2, $3, $4)); }
  | /* empty */
    { $$ = nullptr; }
  ;
dist_list
  : dist_item
    { $$ = MakeTaggedNode(N::kDistributionItemList, $1); }
  | dist_list ',' dist_item
    { $$ = ExtendNode($1, $2, $3); }
  ;
dist_item
  : value_range dist_weight
    { $$ = MakeTaggedNode(N::kDistributionItem, $1, ForwardChildren($2)); }
  | value_range
    { $$ = MakeTaggedNode(N::kDistributionItem, $1, nullptr, nullptr); }
  ;
dist_weight
  : TK_COLON_EQ expression
    { $$ = MakeNode($1, $2); }
  | TK_COLON_DIV expression
    { $$ = MakeNode($1, $2); }
  ;
/**
sequence_instance
  // looks like a function call
  : reference_or_call
  ;
sequence_arguments_list_opt
  : '(' sequence_arguments_list ')'
  | '(' ')'
  | // empty
  ;
sequence_arguments_list
  : sequence_arguments_list ',' sequence_actual_arg_any
  | sequence_actual_arg_any
  ;
// TODO(fangism): Positional arguments must appear before named arguments.
sequence_actual_arg_any
  : sequence_actual_arg
    // positional argument
  | '.' member_name '(' sequence_actual_arg ')'
    // named argument
  ;
sequence_actual_arg
  // identical to property_actual_arg
  : event_expression
  | sequence_expr
  ;
**/

sequence_expr_primary
  // TODO(fangism): Combine these into a single conflict-free ruleset.
  : sequence_repetition_expr
    { $$ = move($1); }
  /*
  | sequence_expr_match_primary
   * is now 'covered' by sequence_repetition_expr, see explanation under
   * the sequence_repetition_expr nonterminal.
   */
  ;
/*
sequence_expr_match_primary
  : '(' sequence_expr_match_item_list ')' sequence_abbrev_opt
  ;
*/
sequence_repetition_expr
  /* Highest precedence sequence_expr. */
  : expression_or_dist boolean_abbrev_opt
    { $$ = ($2 == nullptr) ? move($1) :
                             MakeTaggedNode(N::kSequenceRepetitionExpression,
                                            $1, $2); }
  /* This covers a simple expression.
   *
   * More importantly, this also covers sequence_expr_match_primary without conflict:
   * expression_or_dist :: expr_primary_parens :: '(' expr_mintypmax ')'
   *   :: '(' sequence_expr_match_item_list ')'
   * expr_mintypmax :: property_expr_or_assignment_list
   * property_expr_or_assignment :: property_expr (start of sequence_expr_match_item_list)
   *   :: subroutine_call (of sequence_match_item)
   * property_expr_or_assignment :: sequence_match_item :: assignment_statement
   * boolean_abbrev_opt :: sequence_abbrev_opt
   */
  ;
expression_or_dist
  : expression dist_opt
    {
      // If dist_opt is nullptr, then we can just forward expression
      // without introducing another layer
      if ($2 == nullptr)  {
        $$ = move($1);
      } else {
        SetChild($2, 0, $1);
        $$ = move($2);
      }
    }
  ;

/* covered by boolean_abbrev_opt
sequence_abbrev_opt
  : consecutive_repetition
  | // empty
  ;
*/
boolean_abbrev_opt
  /* This covers repeat_range_opt. */
  : boolean_abbrev
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
boolean_abbrev
  /* similar to repeat_range */
  : consecutive_repetition
    { $$ = move($1); }
  | nonconsecutive_repetition
    { $$ = move($1); }
  | goto_repetition
    { $$ = move($1); }
  ;
consecutive_repetition
  : TK_LBSTAR cycle_range_or_expr ']'
    { $$ = MakeTaggedNode(N::kConsecutiveRepetition, $1, $2, $3); }
  | TK_LBSTARRB
    { $$ = MakeTaggedNode(N::kConsecutiveRepetition, $1); }
    /* but not lb_star_rb */
  | TK_LBPLUSRB
    { $$ = MakeTaggedNode(N::kConsecutiveRepetition, $1); }
  ;
nonconsecutive_repetition
  : TK_LBEQ cycle_range_or_expr ']'
    { $$ = MakeTaggedNode(N::kNonconsecutiveRepetition, $1, $2, $3); }
  ;
goto_repetition
  : TK_LBRARROW cycle_range_or_expr ']'
    { $$ = MakeTaggedNode(N::kGotoRepetition, $1, $2, $3); }
  ;

covergroup_declaration
  : TK_covergroup GenericIdentifier tf_port_list_paren_opt coverage_event_opt ';'
    coverage_spec_or_option_list_opt
    TK_endgroup label_opt
    { $$ = MakeTaggedNode(N::kCovergroupDeclaration,
                               MakeTaggedNode(N::kCovergroupHeader,
                                              $1, $2, $3, $4, $5),
                               $6, $7, $8); }
  ;

coverage_spec_or_option_list_opt
  : coverage_spec_or_option_list
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
coverage_spec_or_option_list
  : coverage_spec_or_option_list coverage_spec_or_option
    { $$ = ExtendNode($1, $2); }
  | coverage_spec_or_option
    { $$ = MakeTaggedNode(N::kCoverageSpecOptionList, $1); }
  ;
coverage_spec_or_option
  : /* attribute_list_opt */ coverage_spec /* no ';' here */
    { $$ = move($1); }
  | /* attribute_list_opt */ coverage_option ';'
    { $$ = ExtendNode($1, $2); }
  | preprocessor_directive
    { $$ = move($1); }
  | macro_call_or_item
    { $$ = move($1); }
  ;
coverage_option
  : TK_option '.' GenericIdentifier '=' expression
    { $$ = MakeTaggedNode(N::kCoverageOption, $1, $2, $3, $4, $5); }
  | TK_type_option '.' GenericIdentifier '=' expression
    { $$ = MakeTaggedNode(N::kCoverageOption, $1, $2, $3, $4, $5); }
  ;
coverage_spec
  : cover_point
    { $$ = move($1); }
    /* already ends with '}' or ';' */
  | cover_cross
    { $$ = move($1); }
    /* already ends with '}' or ';' */
  ;
coverage_event_opt
  : coverage_event
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
coverage_event
  : event_control
    { $$ = move($1); }
  | TK_with__covergroup TK_function TK_sample '(' tf_port_list_opt ')'
    { $$ = MakeTaggedNode(N::kCoverageEvent, $1, $2, $3, MakeParenGroup($4, $5, $6)); }
  | TK_ATAT '(' block_event_expression ')'
    { $$ = MakeTaggedNode(N::kCoverageEvent, $1 , MakeParenGroup($2, $3, $4)); }
  ;
block_event_expression
  : block_event_or_expr
    { $$ = move($1); }
  ;
block_event_or_expr
  : block_event_or_expr TK_or block_event_expr_primary
    { $$ = ExtendNode($1, $2, $3); }
  | block_event_expr_primary
    { $$ = MakeTaggedNode(N::kCoverageBlockEventOrList, $1); }
  ;
block_event_expr_primary
  : TK_begin reference
    { $$ = MakeTaggedNode(N::kCoverageBlockEventExpression, $1, $2); }
  | TK_end reference
    { $$ = MakeTaggedNode(N::kCoverageBlockEventExpression, $1, $2); }
  ;

cover_cross
  /* optional leading label is painful to disambiguate */
  : data_type_or_implicit_basic_followed_by_id ':'
    TK_cross cross_item_list iff_expr_opt cross_body
    { $$ = MakeTaggedNode(N::kCoverCross, $1, $2, $3, $4, $5, $6); }
    /* $1 should be GenericIdentifier, make sure it has nothing else.
     * promoted as such to ease S/R conflict resolution.
     */
  | TK_cross cross_item_list iff_expr_opt cross_body
    { $$ = MakeTaggedNode(N::kCoverCross, $1, $2, $3, $4); }
  ;
cross_body
  : '{' cross_body_item_list_opt '}'
    { $$ = MakeBraceGroup($1, $2, $3); }
  | ';'
    { $$ = move($1); }
  ;
cross_body_item_list_opt
  : cross_body_item_list
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
cross_body_item_list
  : cross_body_item_list cross_body_item
    { $$ = ExtendNode($1, $2); }
  | cross_body_item
    { $$ = MakeTaggedNode(N::kCrossBodyItemList, $1); }
  ;
cross_item_list
  : cross_item_list ',' cross_item
    { $$ = ExtendNode($1, $2, $3); }
  | cross_item ',' cross_item
    { $$ = MakeTaggedNode(N::kCrossItemList, $1, $2, $3); }
    /* cross_item_list must contain 2 or more items. */
  ;
cross_body_item
  : function_declaration
    { $$ = move($1); }
  | /* attribute_list_opt */ bins_selection ';'
    { $$ = ExtendNode($1, $2); }
  | /* attribute_list_opt */ coverage_option ';'
    { $$ = ExtendNode($1, $2); }
  ;
cross_item
  : reference
    { $$ = move($1); }
    /* refers to a cover_point_identifier or variable_identifier */
    /* NOTE: The LRM only allows a simple GenericIdentifier here,
     * so a general hierarchical reference may be a vendor extension.
     */
  ;
cover_point
  : data_type_or_implicit_basic_followed_by_id ':' TK_coverpoint
    /* $1 is optional label (GenericIdentifier) */
    property_expr
    /* expression iff_expr_opt (covered by property_expr) */
    bins_or_options_list_opt
    { $$ = MakeTaggedNode(N::kCoverPoint, $1, $2, $3, $4, $5); }
    /* must end with '}' or ';' */
  | TK_coverpoint
    property_expr
    /* expression iff_expr_opt (covered by property_expr) */
    bins_or_options_list_opt
    { $$ = MakeTaggedNode(N::kCoverPoint, nullptr, nullptr, $1, $2, $3); }
  ;
iff_expr_opt
  : TK_iff '(' expression ')'
    { $$ = MakeTaggedNode(N::kIffExpression, $1, MakeParenGroup($2, $3, $4)); }
  | /* empty */
    { $$ = nullptr; }
  ;
bins_or_options_list_opt
  : '{' bins_or_options_list '}'
    { $$ = MakeBraceGroup($1, $2, $3); }

    /* Some copies of EBNF spec (sigasi.com) misprints non-literal braces.
     * Code samples confirm that these are in fact required literal braces.
     */
  | ';'
    { $$ = move($1); }
  ;
bins_or_options_list
  : bins_or_options_list bins_or_options
    { $$ = ExtendNode($1, $2); }
  | bins_or_options
    { $$ = MakeTaggedNode(N::kBinOptionList, $1); }
  ;
bins_or_options
  : /* attribute_list_opt */ coverage_option ';'
    { $$ = ExtendNode($1, $2); }
  | /* attribute_list_opt */ coverage_bin ';'
    { $$ = ExtendNode($1, $2); }
  | preprocessor_balanced_bins_or_options_list
    { $$ = move($1); }
  | macro_call_or_item
    { $$ = move($1); }
  ;

bins_or_options_list_opt_pp
  : bins_or_options_list
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
preprocessor_balanced_bins_or_options_list
  : preprocessor_if_header bins_or_options_list_opt_pp
    preprocessor_elsif_bins_or_options_list_opt
    preprocessor_else_bins_or_options_opt
    PP_endif
    { $$ = MakeTaggedNode(N::kPreprocessorBalancedBinsOrOptions,
                          ExtendNode($1, $2), ForwardChildren($3), $4, $5);
    }
  ;
preprocessor_elsif_bins_or_options_list_opt
  : preprocessor_elsif_bins_or_options_list
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
preprocessor_elsif_bins_or_options_list
  : preprocessor_elsif_bins_or_options_list preprocessor_elsif_bins_or_options
    { $$ = ExtendNode($1, $2); }
  | preprocessor_elsif_bins_or_options
    { $$ = MakeNode($1); }  /* Don't bother tagging; node will be flattened. */
  ;
preprocessor_elsif_bins_or_options
  : preprocessor_elsif_header bins_or_options_list_opt_pp
    { $$ = ExtendNode($1, $2); }
  ;
preprocessor_else_bins_or_options_opt
  : preprocessor_else_bins_or_options
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
preprocessor_else_bins_or_options
  : PP_else bins_or_options_list_opt_pp
    { $$ = MakeTaggedNode(N::kPreprocessorElseClause, $1, $2); }
  ;

coverage_bin
  /* This is a simplified approximation that may permit some illegal combinations. */
  : wildcard_opt bins_keyword GenericIdentifier covergroup_expression_bracketed_opt '='
    coverage_bin_rhs iff_expr_opt
    { $$ = MakeTaggedNode(N::kCoverageBin, $1, $2, $3, $4, $5, $6, $7); }
  ;
coverage_bin_rhs
  : set_covergroup_expression_or_covergroup_range_list_or_trans_list
    { $$ = move($1); }
  | TK_default
    { $$ = move($1); }
  | TK_default TK_sequence
    { $$ = MakeNode($1, $2); }
  ;
wildcard_opt
  : TK_wildcard
    { $$ = move($1); }
  | /* empty */
    { $$ = nullptr; }
  ;
bins_keyword
  : TK_bins
    { $$ = move($1); }
  | TK_illegal_bins
    { $$ = move($1); }
  | TK_ignore_bins
    { $$ = move($1); }
  ;
covergroup_expression_bracketed_opt
  : '[' expression ']'
    { $$ = MakeBracketGroup($1, $2, $3); }
  | '[' ']'
    { $$ = MakeBracketGroup($1, nullptr, $2); }
  | /* empty */
    { $$ = nullptr; }
  ;
set_covergroup_expression_or_covergroup_range_list_or_trans_list
  /* This has been grouped together in a permissive manner to mitigate conflicts. */
  : expression_list_proper  /* comma-separated list of expressions */
    { $$ = move($1); }
  /* set_covergroup_expression is covergroup_expression is expression.
   *
   * covers cover_point_identifier with:
   * expression_list_proper :: expression :: GenericIdentifier
   *
   * covers '{' covergroup_range_list '}', which is '{' open_range_list '}':
   * expression_list_proper :: expression :: expr_primary_braces
   *   :: '{' open_range_list '}'
   *
   * covers trans_list (commented below):
   * expression_list_proper :: trans_list
   * trans_item :: expression :: expr_primary_parens :: '(' expr_mintypmax ')'
   * expr_mintypmax :: expr_mintypmax_trans_set :: expr_mintypmax_generalized
   * open_range_list :: value_range
  | trans_list
   */
  ;
/** retained for reference
trans_list
  : trans_list ',' trans_item
  | trans_item
  ;
trans_item
  : '(' trans_set ')'
  ;
trans_set
  : trans_set TK_EG trans_range_list
  | trans_range_list
  ;
trans_range_list
  : open_range_list repeat_range_opt
    // $1 same as covergroup_range_list
  ;
repeat_range_opt
  // covered by to boolean_abbrev_opt
  // repeat_range is covered by cycle_range_or_expr
  : TK_LBSTAR cycle_range_or_expr ']'
  | TK_LBRARROW cycle_range_or_expr ']'
  | TK_LBEQ cycle_range_or_expr ']'
  | // empty
  ;
repeat_range
  : value_range
  ;
*/
bins_selection
  : bins_keyword GenericIdentifier '=' select_expression /* iff_expr_opt */
    { $$ = MakeTaggedNode(N::kBinsSelection, $1, $2, $3, $4); }
    /* select_expression as property_expr covers trailing iff_expr_opt */
  ;
select_expression
  : property_expr
    { $$ = move($1); }
  /* from EBNF:
  : select_condition
  | ! select_condition
  | select_expression && select_expression
  | select_expression || select_expression
  | ( select_expression )
  | select_expression with ( with_covergroup_expression ) [ matches integer_covergroup_expression ]
  | cross_identifier
  | cross_set_expression [ matches integer_covergroup_expression ]
  */
  ;

select_condition
  /* primary select expression */
  : TK_binsof '(' bins_expression ')'
    { $$ = MakeTaggedNode(N::kSelectCondition, $1, MakeParenGroup($2, $3, $4)); }
  /* optional trailing TK_intersect range_list_in_braces is covered by
   * select_expression using property_expr, which contains the
   * TK_intersect operator.
  | TK_binsof '(' bins_expression ')' TK_intersect range_list_in_braces
   * where range_list_in_braces covers covergroup_range_list.
   */
  ;
bins_expression
  : reference
    { $$ = move($1); }
  ;
with_covergroup_expression_in_parens
  : TK_with__covergroup '(' with_covergroup_expression ')'
    // TODO(fangism): optionally trailing 'matches':
    // TK_matches integer_covergroup_expression
  ;
with_covergroup_expression
  : expression
  ;
/*
integer_covergroup_expression
  : expression
  ;
cross_set_expression
  : expression
  ;
*/

/******** LRM: A.6.12 randsequence ********/
randsequence_statement
  : TK_randsequence '(' identifier_opt ')' production_list TK_endsequence
    { $$ = MakeTaggedNode(N::kRandSequenceStatement,
                          $1, MakeParenGroup($2, $3, $4), $5, $6); }
  ;

production_list
  : production_list production
    { $$ = ExtendNode($1, $2); }
  | production
    { $$ = MakeTaggedNode(N::kProductionList, $1); }
  ;

production
  : data_type_or_void_with_id tf_port_list_paren_opt ':' rs_rule_list ';'
    { $$ = MakeTaggedNode(N::kProduction, $1, $2, $3, $4, $5); }
  ;

data_type_or_void_with_id
  : data_type_or_implicit_basic_followed_by_id_and_dimensions_opt
  /* simplify: this is too general, no need for dimensions */
    { $$ = move($1); }
  ;

rs_rule_list
  : rs_rule_list '|' rs_rule
    { $$ = ExtendNode($1, $2, $3); }
  | rs_rule
    { $$ = MakeTaggedNode(N::kRandSequenceRuleList, $1); }
  ;

rs_rule
  : rs_production_list_or_rand_join
    { $$ = MakeTaggedNode(N::kRandSequenceRule, $1); }
  | rs_production_list_or_rand_join TK_COLON_EQ weight_specification
    { $$ = MakeTaggedNode(N::kRandSequenceRule, $1, $2, $3); }
  | rs_production_list_or_rand_join TK_COLON_EQ weight_specification rs_code_block
    { $$ = MakeTaggedNode(N::kRandSequenceRule, $1, $2, $3, $4); }
  ;

rs_production_list_or_rand_join
  : rs_production_list
    { $$ = move($1); }
  | TK_rand TK_join expression_in_parens_opt production_items_list
    { $$ = MakeTaggedNode(N::kRandJoin, $1, $2, $3, $4); }
  ;

expression_in_parens_opt
  : '(' expression ')'
    { $$ = MakeParenGroup($1, $2, $3); }
  | /* empty */
    { $$ = nullptr; }
  ;

rs_production_list
  : rs_production_list rs_prod
    { $$ = ExtendNode($1, $2); }
  | rs_prod
    { $$ = MakeTaggedNode(N::kRandSequenceProductionList, $1); }
  ;

weight_specification
  : number
    /* $1 must be integral */
    { $$ = MakeTaggedNode(N::kWeightSpecification, $1); }
  | GenericIdentifier
    { $$ = MakeTaggedNode(N::kWeightSpecification, $1); }
  | '(' expression ')'
    { $$ = MakeTaggedNode(N::kWeightSpecification, $1, $2, $3); }
  ;

rs_code_block
  : '{' block_item_or_statement_or_null_list_opt '}'
    /* block_item_decls_opt statement_or_null_list_opt */
    { $$ = MakeBraceGroup($1, $2, $3); }
  ;

production_items_list
  : production_items_list production_item
    { $$ = ExtendNode($1, $2); }
  | production_item production_item
    /* at least two elements required */
    { $$ = MakeTaggedNode(N::kProductionItemsList, $1, $2); }
  ;

rs_prod
  : production_item
    { $$ = move($1); }
  | rs_code_block
    { $$ = move($1); }
  | rs_if_else
    { $$ = move($1); }
  | rs_repeat
    { $$ = move($1); }
  | rs_case
    { $$ = move($1); }
  ;

production_item
  : GenericIdentifier '(' any_port_list ')'
    { $$ = MakeTaggedNode(N::kProductionItem, $1, MakeParenGroup($2, $3, $4)); }
  | GenericIdentifier
    { $$ = MakeTaggedNode(N::kProductionItem, $1); }
  ;

rs_if_else
  : TK_if '(' expression ')' production_item TK_else production_item
    { $$ = MakeTaggedNode(N::kRandSequenceConditional,
                          $1, MakeParenGroup($2, $3, $4), $5, $6, $7); }
  | TK_if '(' expression ')' production_item
    { $$ = MakeTaggedNode(N::kRandSequenceConditional,
                          $1, MakeParenGroup($2, $3, $4), $5); }
  ;

rs_repeat
  : repeat_control production_item
    { $$ = MakeTaggedNode(N::kRandSequenceLoop, $1, $2); }
  ;

rs_case
  : TK_case '(' expression ')' rs_case_item_list TK_endcase
    { $$ = MakeTaggedNode(N::kRandSequenceCase,
                          $1, MakeParenGroup($2, $3, $4), $5, $6); }
  ;

rs_case_item_list
  : rs_case_item_list rs_case_item
    { $$ = ExtendNode($1, $2); }
  | rs_case_item
    { $$ = MakeTaggedNode(N::kRandSequenceCaseItemList, $1); }
  ;

rs_case_item
  : case_item_expression_list ':' production_item ';'
    { $$ = MakeTaggedNode(N::kRandSequenceCaseItem, $1, $2, $3, $4); }
  | TK_default ':' production_item ';'
    { $$ = MakeTaggedNode(N::kRandSequenceDefaultItem, $1, $2, $3, $4); }
  | TK_default     production_item ';'
    { $$ = MakeTaggedNode(N::kRandSequenceDefaultItem, $1, nullptr, $2, $3); }
  ;

case_item_expression_list
  : case_item_expression_list ',' case_item_expression
    { $$ = ExtendNode($1, $2, $3); }
  | case_item_expression
    { $$ = MakeTaggedNode(N::kExpressionList, $1); }
  ;

case_item_expression
  : expression
    { $$ = move($1); }
  ;

%%

// Ensure type consistency with StateStack in parser_param.h
static_assert(std::is_same<yytype_int16, verible::bison_state_int_type>::value,
    "Update bison_state_int_type in parser_param.h to match yy.tab.cc.");

// Expose the token names for diagnostic messages.
const char* verilog_symbol_name(size_t symbol_enum) {
  // This (static) table contains YYNTOKENS token names,
  // followed by YYNNTS nonterminal symbol names.
  const int yytoken = YYTRANSLATE(symbol_enum);
  CHECK(yytoken <= YYNTOKENS + YYNNTS);
  return yytname[yytoken];
}

}  // namespace verilog
