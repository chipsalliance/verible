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

#include "verible/verilog/formatting/verilog-token.h"

#include "absl/container/node_hash_map.h"
#include "verible/common/util/container-util.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace formatter {

using FTT = FormatTokenType;

// Mapping of verilog_tokentype enum to FormatTokenType
static const absl::node_hash_map<verilog_tokentype, FTT> &FormatTokenTypeMap() {
  static const absl::node_hash_map<verilog_tokentype, FTT> kFormatTokenMap({
      // keywords
      {verilog_tokentype::PP_include, FTT::keyword},
      {verilog_tokentype::PP_define, FTT::keyword},
      {verilog_tokentype::PP_define_body, FTT::keyword},
      {verilog_tokentype::PP_ifdef, FTT::keyword},
      {verilog_tokentype::PP_ifndef, FTT::keyword},
      {verilog_tokentype::PP_else, FTT::keyword},
      {verilog_tokentype::PP_elsif, FTT::keyword},
      {verilog_tokentype::PP_endif, FTT::keyword},
      {verilog_tokentype::PP_undef, FTT::keyword},
      {verilog_tokentype::PP_default_text, FTT::keyword},
      {verilog_tokentype::PP_TOKEN_CONCAT, FTT::binary_operator},
      {verilog_tokentype::DR_timescale, FTT::keyword},
      {verilog_tokentype::DR_resetall, FTT::keyword},
      {verilog_tokentype::DR_celldefine, FTT::keyword},
      {verilog_tokentype::DR_endcelldefine, FTT::keyword},
      {verilog_tokentype::DR_unconnected_drive, FTT::keyword},
      {verilog_tokentype::DR_nounconnected_drive, FTT::keyword},
      {verilog_tokentype::DR_default_nettype, FTT::keyword},
      {verilog_tokentype::DR_suppress_faults, FTT::keyword},
      {verilog_tokentype::DR_nosuppress_faults, FTT::keyword},
      {verilog_tokentype::DR_enable_portfaults, FTT::keyword},
      {verilog_tokentype::DR_disable_portfaults, FTT::keyword},
      {verilog_tokentype::DR_delay_mode_distributed, FTT::keyword},
      {verilog_tokentype::DR_delay_mode_path, FTT::keyword},
      {verilog_tokentype::DR_delay_mode_unit, FTT::keyword},
      {verilog_tokentype::DR_delay_mode_zero, FTT::keyword},
      {verilog_tokentype::DR_default_decay_time, FTT::keyword},
      {verilog_tokentype::DR_default_trireg_strength, FTT::keyword},
      {verilog_tokentype::DR_pragma, FTT::keyword},
      {verilog_tokentype::DR_uselib, FTT::keyword},
      {verilog_tokentype::DR_begin_keywords, FTT::keyword},
      {verilog_tokentype::DR_end_keywords, FTT::keyword},
      {verilog_tokentype::DR_protect, FTT::keyword},
      {verilog_tokentype::DR_endprotect, FTT::keyword},
      {verilog_tokentype::MacroCallId, FTT::identifier},
      {verilog_tokentype::MacroIdItem, FTT::identifier},
      {verilog_tokentype::TK_DOTSTAR, FTT::keyword},
      {verilog_tokentype::TK_1step, FTT::keyword},
      {verilog_tokentype::TK_always, FTT::keyword},
      {verilog_tokentype::TK_and, FTT::keyword},
      {verilog_tokentype::TK_assign, FTT::keyword},
      {verilog_tokentype::TK_begin, FTT::keyword},
      {verilog_tokentype::TK_buf, FTT::keyword},
      {verilog_tokentype::TK_bufif0, FTT::keyword},
      {verilog_tokentype::TK_bufif1, FTT::keyword},
      {verilog_tokentype::TK_case, FTT::keyword},
      {verilog_tokentype::TK_casex, FTT::keyword},
      {verilog_tokentype::TK_casez, FTT::keyword},
      {verilog_tokentype::TK_cmos, FTT::keyword},
      {verilog_tokentype::TK_deassign, FTT::keyword},
      {verilog_tokentype::TK_default, FTT::keyword},
      {verilog_tokentype::TK_defparam, FTT::keyword},
      {verilog_tokentype::TK_disable, FTT::keyword},
      {verilog_tokentype::TK_edge, FTT::keyword},
      {verilog_tokentype::TK_else, FTT::keyword},
      {verilog_tokentype::TK_end, FTT::keyword},
      {verilog_tokentype::TK_endcase, FTT::keyword},
      {verilog_tokentype::TK_endfunction, FTT::keyword},
      {verilog_tokentype::TK_endmodule, FTT::keyword},
      {verilog_tokentype::TK_endprimitive, FTT::keyword},
      {verilog_tokentype::TK_endspecify, FTT::keyword},
      {verilog_tokentype::TK_endtable, FTT::keyword},
      {verilog_tokentype::TK_endtask, FTT::keyword},
      {verilog_tokentype::TK_event, FTT::keyword},
      {verilog_tokentype::TK_for, FTT::keyword},
      {verilog_tokentype::TK_force, FTT::keyword},
      {verilog_tokentype::TK_forever, FTT::keyword},
      {verilog_tokentype::TK_fork, FTT::keyword},
      {verilog_tokentype::TK_function, FTT::keyword},
      {verilog_tokentype::TK_highz0, FTT::keyword},
      {verilog_tokentype::TK_highz1, FTT::keyword},
      {verilog_tokentype::TK_if, FTT::keyword},
      {verilog_tokentype::TK_ifnone, FTT::keyword},
      {verilog_tokentype::TK_initial, FTT::keyword},
      {verilog_tokentype::TK_inout, FTT::keyword},
      {verilog_tokentype::TK_input, FTT::keyword},
      {verilog_tokentype::TK_integer, FTT::keyword},
      {verilog_tokentype::TK_join, FTT::keyword},
      {verilog_tokentype::TK_large, FTT::keyword},
      {verilog_tokentype::TK_macromodule, FTT::keyword},
      {verilog_tokentype::TK_medium, FTT::keyword},
      {verilog_tokentype::TK_module, FTT::keyword},
      {verilog_tokentype::TK_nand, FTT::keyword},
      {verilog_tokentype::TK_negedge, FTT::keyword},
      {verilog_tokentype::TK_nmos, FTT::keyword},
      {verilog_tokentype::TK_nor, FTT::keyword},
      {verilog_tokentype::TK_not, FTT::keyword},
      {verilog_tokentype::TK_notif0, FTT::keyword},
      {verilog_tokentype::TK_notif1, FTT::keyword},
      {verilog_tokentype::TK_or, FTT::keyword},
      {verilog_tokentype::TK_option, FTT::keyword},
      {verilog_tokentype::TK_output, FTT::keyword},
      {verilog_tokentype::TK_parameter, FTT::keyword},
      {verilog_tokentype::TK_pmos, FTT::keyword},
      {verilog_tokentype::TK_posedge, FTT::keyword},
      {verilog_tokentype::TK_primitive, FTT::keyword},
      {verilog_tokentype::TK_pull0, FTT::keyword},
      {verilog_tokentype::TK_pull1, FTT::keyword},
      {verilog_tokentype::TK_pulldown, FTT::keyword},
      {verilog_tokentype::TK_pullup, FTT::keyword},
      {verilog_tokentype::TK_rcmos, FTT::keyword},
      {verilog_tokentype::TK_real, FTT::keyword},
      {verilog_tokentype::TK_realtime, FTT::keyword},
      {verilog_tokentype::TK_reg, FTT::keyword},
      {verilog_tokentype::TK_release, FTT::keyword},
      {verilog_tokentype::TK_repeat, FTT::keyword},
      {verilog_tokentype::TK_rnmos, FTT::keyword},
      {verilog_tokentype::TK_rpmos, FTT::keyword},
      {verilog_tokentype::TK_rtran, FTT::keyword},
      {verilog_tokentype::TK_rtranif0, FTT::keyword},
      {verilog_tokentype::TK_rtranif1, FTT::keyword},
      {verilog_tokentype::TK_sample, FTT::keyword},
      {verilog_tokentype::TK_scalared, FTT::keyword},
      {verilog_tokentype::TK_small, FTT::keyword},
      {verilog_tokentype::TK_specify, FTT::keyword},
      {verilog_tokentype::TK_specparam, FTT::keyword},
      {verilog_tokentype::TK_strong0, FTT::keyword},
      {verilog_tokentype::TK_strong1, FTT::keyword},
      {verilog_tokentype::TK_supply0, FTT::keyword},
      {verilog_tokentype::TK_supply1, FTT::keyword},
      {verilog_tokentype::TK_table, FTT::keyword},
      {verilog_tokentype::TK_task, FTT::keyword},
      {verilog_tokentype::TK_time, FTT::keyword},
      {verilog_tokentype::TK_tran, FTT::keyword},
      {verilog_tokentype::TK_tranif0, FTT::keyword},
      {verilog_tokentype::TK_tranif1, FTT::keyword},
      {verilog_tokentype::TK_tri, FTT::keyword},
      {verilog_tokentype::TK_tri0, FTT::keyword},
      {verilog_tokentype::TK_tri1, FTT::keyword},
      {verilog_tokentype::TK_triand, FTT::keyword},
      {verilog_tokentype::TK_trior, FTT::keyword},
      {verilog_tokentype::TK_trireg, FTT::keyword},
      {verilog_tokentype::TK_type_option, FTT::keyword},
      {verilog_tokentype::TK_vectored, FTT::keyword},
      {verilog_tokentype::TK_wait, FTT::keyword},
      {verilog_tokentype::TK_wand, FTT::keyword},
      {verilog_tokentype::TK_weak0, FTT::keyword},
      {verilog_tokentype::TK_weak1, FTT::keyword},
      {verilog_tokentype::TK_while, FTT::keyword},
      {verilog_tokentype::TK_wire, FTT::keyword},
      {verilog_tokentype::TK_wor, FTT::keyword},
      {verilog_tokentype::TK_xnor, FTT::keyword},
      {verilog_tokentype::TK_xor, FTT::keyword},
      {verilog_tokentype::TKK_attribute,
       FTT::keyword},  // these look like comments (*...*)
      {verilog_tokentype::TK_automatic, FTT::keyword},
      {verilog_tokentype::TK_endgenerate, FTT::keyword},
      {verilog_tokentype::TK_generate, FTT::keyword},
      {verilog_tokentype::TK_genvar, FTT::keyword},
      {verilog_tokentype::TK_localparam, FTT::keyword},
      {verilog_tokentype::TK_noshowcancelled, FTT::keyword},
      {verilog_tokentype::TK_pulsestyle_onevent, FTT::keyword},
      {verilog_tokentype::TK_pulsestyle_ondetect, FTT::keyword},
      {verilog_tokentype::TK_showcancelled, FTT::keyword},
      {verilog_tokentype::TK_signed, FTT::keyword},
      {verilog_tokentype::TK_unsigned, FTT::keyword},
      {verilog_tokentype::TK_cell, FTT::keyword},
      {verilog_tokentype::TK_config, FTT::keyword},
      {verilog_tokentype::TK_design, FTT::keyword},
      {verilog_tokentype::TK_endconfig, FTT::keyword},
      {verilog_tokentype::TK_incdir, FTT::keyword},
      {verilog_tokentype::TK_include, FTT::keyword},
      {verilog_tokentype::TK_instance, FTT::keyword},
      {verilog_tokentype::TK_liblist, FTT::keyword},
      {verilog_tokentype::TK_library, FTT::keyword},
      {verilog_tokentype::TK_use, FTT::keyword},
      {verilog_tokentype::TK_wone, FTT::keyword},
      {verilog_tokentype::TK_uwire, FTT::keyword},
      {verilog_tokentype::TK_alias, FTT::keyword},
      {verilog_tokentype::TK_always_comb, FTT::keyword},
      {verilog_tokentype::TK_always_ff, FTT::keyword},
      {verilog_tokentype::TK_always_latch, FTT::keyword},
      {verilog_tokentype::TK_assert, FTT::keyword},
      {verilog_tokentype::TK_assume, FTT::keyword},
      {verilog_tokentype::TK_before, FTT::keyword},
      {verilog_tokentype::TK_bind, FTT::keyword},
      {verilog_tokentype::TK_bins, FTT::keyword},
      {verilog_tokentype::TK_binsof, FTT::keyword},
      {verilog_tokentype::TK_bit, FTT::keyword},
      {verilog_tokentype::TK_break, FTT::keyword},
      {verilog_tokentype::TK_byte, FTT::keyword},
      {verilog_tokentype::TK_chandle, FTT::keyword},
      {verilog_tokentype::TK_class, FTT::keyword},
      {verilog_tokentype::TK_clocking, FTT::keyword},
      {verilog_tokentype::TK_const, FTT::keyword},
      {verilog_tokentype::TK_constraint, FTT::keyword},
      {verilog_tokentype::TK_context, FTT::keyword},
      {verilog_tokentype::TK_continue, FTT::keyword},
      {verilog_tokentype::TK_cover, FTT::keyword},
      {verilog_tokentype::TK_covergroup, FTT::keyword},
      {verilog_tokentype::TK_coverpoint, FTT::keyword},
      {verilog_tokentype::TK_cross, FTT::keyword},
      {verilog_tokentype::TK_dist, FTT::keyword},
      {verilog_tokentype::TK_do, FTT::keyword},
      {verilog_tokentype::TK_endclass, FTT::keyword},
      {verilog_tokentype::TK_endclocking, FTT::keyword},
      {verilog_tokentype::TK_endgroup, FTT::keyword},
      {verilog_tokentype::TK_endinterface, FTT::keyword},
      {verilog_tokentype::TK_endpackage, FTT::keyword},
      {verilog_tokentype::TK_endprogram, FTT::keyword},
      {verilog_tokentype::TK_endproperty, FTT::keyword},
      {verilog_tokentype::TK_endsequence, FTT::keyword},
      {verilog_tokentype::TK_enum, FTT::keyword},
      {verilog_tokentype::TK_expect, FTT::keyword},
      {verilog_tokentype::TK_export, FTT::keyword},
      {verilog_tokentype::TK_extends, FTT::keyword},
      {verilog_tokentype::TK_extern, FTT::keyword},
      {verilog_tokentype::TK_final, FTT::keyword},
      {verilog_tokentype::TK_first_match, FTT::keyword},
      {verilog_tokentype::TK_foreach, FTT::keyword},
      {verilog_tokentype::TK_forkjoin, FTT::keyword},
      {verilog_tokentype::TK_iff, FTT::keyword},
      {verilog_tokentype::TK_ignore_bins, FTT::keyword},
      {verilog_tokentype::TK_illegal_bins, FTT::keyword},
      {verilog_tokentype::TK_import, FTT::keyword},
      {verilog_tokentype::TK_inside, FTT::keyword},
      {verilog_tokentype::TK_int, FTT::keyword},
      {verilog_tokentype::TK_interface, FTT::keyword},
      {verilog_tokentype::TK_intersect, FTT::keyword},
      {verilog_tokentype::TK_join_any, FTT::keyword},
      {verilog_tokentype::TK_join_none, FTT::keyword},
      {verilog_tokentype::TK_local, FTT::keyword},
      {verilog_tokentype::TK_local_SCOPE, FTT::keyword},
      {verilog_tokentype::TK_logic, FTT::keyword},
      {verilog_tokentype::TK_longint, FTT::keyword},
      {verilog_tokentype::TK_matches, FTT::keyword},
      {verilog_tokentype::TK_modport, FTT::keyword},
      {verilog_tokentype::TK_new, FTT::keyword},
      {verilog_tokentype::TK_null, FTT::keyword},
      {verilog_tokentype::TK_package, FTT::keyword},
      {verilog_tokentype::TK_packed, FTT::keyword},
      {verilog_tokentype::TK_priority, FTT::keyword},
      {verilog_tokentype::TK_program, FTT::keyword},
      {verilog_tokentype::TK_property, FTT::keyword},
      {verilog_tokentype::TK_protected, FTT::keyword},
      {verilog_tokentype::TK_pure, FTT::keyword},
      {verilog_tokentype::TK_rand, FTT::keyword},
      {verilog_tokentype::TK_randc, FTT::keyword},
      {verilog_tokentype::TK_randcase, FTT::keyword},
      {verilog_tokentype::TK_randsequence, FTT::keyword},
      {verilog_tokentype::TK_randomize, FTT::keyword},
      {verilog_tokentype::TK_ref, FTT::keyword},
      {verilog_tokentype::TK_return, FTT::keyword},
      {verilog_tokentype::TK_Sroot, FTT::keyword},
      {verilog_tokentype::TK_sequence, FTT::keyword},
      {verilog_tokentype::TK_shortint, FTT::keyword},
      {verilog_tokentype::TK_shortreal, FTT::keyword},
      {verilog_tokentype::TK_solve, FTT::keyword},
      {verilog_tokentype::TK_static, FTT::keyword},
      {verilog_tokentype::TK_string, FTT::keyword},
      {verilog_tokentype::TK_struct, FTT::keyword},
      {verilog_tokentype::TK_super, FTT::keyword},
      {verilog_tokentype::TK_tagged, FTT::keyword},
      {verilog_tokentype::TK_this, FTT::keyword},
      {verilog_tokentype::TK_throughout, FTT::keyword},
      {verilog_tokentype::TK_timeprecision, FTT::keyword},
      {verilog_tokentype::TK_timeunit, FTT::keyword},
      {verilog_tokentype::TK_timescale_unit, FTT::keyword},
      {verilog_tokentype::TK_type, FTT::keyword},
      {verilog_tokentype::TK_typedef, FTT::keyword},
      {verilog_tokentype::TK_union, FTT::keyword},
      {verilog_tokentype::TK_unique, FTT::keyword},
      {verilog_tokentype::TK_unique_index, FTT::keyword},
      {verilog_tokentype::TK_Sunit, FTT::keyword},
      {verilog_tokentype::TK_var, FTT::keyword},
      {verilog_tokentype::TK_virtual, FTT::keyword},
      {verilog_tokentype::TK_void, FTT::keyword},
      {verilog_tokentype::TK_wait_order, FTT::keyword},
      {verilog_tokentype::TK_wildcard, FTT::keyword},
      {verilog_tokentype::TK_with, FTT::keyword},
      {verilog_tokentype::TK_with__covergroup, FTT::keyword},
      {verilog_tokentype::TK_within, FTT::keyword},
      {verilog_tokentype::TK_timeprecision_check, FTT::keyword},
      {verilog_tokentype::TK_timeunit_check, FTT::keyword},
      {verilog_tokentype::TK_accept_on, FTT::keyword},
      {verilog_tokentype::TK_checker, FTT::keyword},
      {verilog_tokentype::TK_endchecker, FTT::keyword},
      {verilog_tokentype::TK_eventually, FTT::keyword},
      {verilog_tokentype::TK_global, FTT::keyword},
      {verilog_tokentype::TK_implies, FTT::keyword},
      {verilog_tokentype::TK_let, FTT::keyword},
      {verilog_tokentype::TK_nexttime, FTT::keyword},
      {verilog_tokentype::TK_reject_on, FTT::keyword},
      {verilog_tokentype::TK_restrict, FTT::keyword},
      {verilog_tokentype::TK_s_always, FTT::keyword},
      {verilog_tokentype::TK_s_eventually, FTT::keyword},
      {verilog_tokentype::TK_s_nexttime, FTT::keyword},
      {verilog_tokentype::TK_s_until, FTT::keyword},
      {verilog_tokentype::TK_s_until_with, FTT::keyword},
      {verilog_tokentype::TK_strong, FTT::keyword},
      {verilog_tokentype::TK_sync_accept_on, FTT::keyword},
      {verilog_tokentype::TK_sync_reject_on, FTT::keyword},
      {verilog_tokentype::TK_unique0, FTT::keyword},
      {verilog_tokentype::TK_until, FTT::keyword},
      {verilog_tokentype::TK_until_with, FTT::keyword},
      {verilog_tokentype::TK_untyped, FTT::keyword},
      {verilog_tokentype::TK_weak, FTT::keyword},
      {verilog_tokentype::TK_implements, FTT::keyword},
      {verilog_tokentype::TK_interconnect, FTT::keyword},
      {verilog_tokentype::TK_nettype, FTT::keyword},
      {verilog_tokentype::TK_soft, FTT::keyword},
      {verilog_tokentype::TK_absdelay, FTT::keyword},
      {verilog_tokentype::TK_abstol, FTT::keyword},
      {verilog_tokentype::TK_access, FTT::keyword},
      {verilog_tokentype::TK_ac_stim, FTT::keyword},
      {verilog_tokentype::TK_aliasparam, FTT::keyword},
      {verilog_tokentype::TK_analog, FTT::keyword},
      {verilog_tokentype::TK_analysis, FTT::keyword},
      {verilog_tokentype::TK_connectmodule, FTT::keyword},
      {verilog_tokentype::TK_connectrules, FTT::keyword},
      {verilog_tokentype::TK_continuous, FTT::keyword},
      {verilog_tokentype::TK_ddt_nature, FTT::keyword},
      {verilog_tokentype::TK_discipline, FTT::keyword},
      {verilog_tokentype::TK_discrete, FTT::keyword},
      {verilog_tokentype::TK_domain, FTT::keyword},
      {verilog_tokentype::TK_driver_update, FTT::keyword},
      {verilog_tokentype::TK_endconnectrules, FTT::keyword},
      {verilog_tokentype::TK_enddiscipline, FTT::keyword},
      {verilog_tokentype::TK_endnature, FTT::keyword},
      {verilog_tokentype::TK_endparamset, FTT::keyword},
      {verilog_tokentype::TK_exclude, FTT::keyword},
      {verilog_tokentype::TK_flicker_noise, FTT::keyword},
      {verilog_tokentype::TK_flow, FTT::keyword},
      {verilog_tokentype::TK_from, FTT::keyword},
      {verilog_tokentype::TK_idt_nature, FTT::keyword},
      {verilog_tokentype::TK_inf, FTT::keyword},
      {verilog_tokentype::TK_infinite, FTT::keyword},
      {verilog_tokentype::TK_laplace_nd, FTT::keyword},
      {verilog_tokentype::TK_laplace_np, FTT::keyword},
      {verilog_tokentype::TK_laplace_zd, FTT::keyword},
      {verilog_tokentype::TK_laplace_zp, FTT::keyword},
      {verilog_tokentype::TK_last_crossing, FTT::keyword},
      {verilog_tokentype::TK_limexp, FTT::keyword},
      {verilog_tokentype::TK_max, FTT::keyword},
      {verilog_tokentype::TK_min, FTT::keyword},
      {verilog_tokentype::TK_nature, FTT::keyword},
      {verilog_tokentype::TK_net_resolution, FTT::keyword},
      {verilog_tokentype::TK_noise_table, FTT::keyword},
      {verilog_tokentype::TK_paramset, FTT::keyword},
      {verilog_tokentype::TK_potential, FTT::keyword},
      {verilog_tokentype::TK_resolveto, FTT::keyword},
      {verilog_tokentype::TK_transition, FTT::keyword},
      {verilog_tokentype::TK_units, FTT::keyword},
      {verilog_tokentype::TK_white_noise, FTT::keyword},
      {verilog_tokentype::TK_wreal, FTT::keyword},
      {verilog_tokentype::TK_zi_nd, FTT::keyword},
      {verilog_tokentype::TK_zi_np, FTT::keyword},
      {verilog_tokentype::TK_zi_zd, FTT::keyword},
      {verilog_tokentype::TK_zi_zp, FTT::keyword},

      // internal parser directives
      {verilog_tokentype::PD_LIBRARY_SYNTAX_BEGIN, FTT::keyword},
      {verilog_tokentype::PD_LIBRARY_SYNTAX_END, FTT::keyword},

      // TODO(fangism): These are built-in function identifiers, and there
      // are even more above, e.g. math functions.
      {verilog_tokentype::TK_find, FTT::keyword},
      {verilog_tokentype::TK_find_index, FTT::keyword},
      {verilog_tokentype::TK_find_first, FTT::keyword},
      {verilog_tokentype::TK_find_first_index, FTT::keyword},
      {verilog_tokentype::TK_find_last, FTT::keyword},
      {verilog_tokentype::TK_find_last_index, FTT::keyword},
      {verilog_tokentype::TK_sort, FTT::keyword},
      {verilog_tokentype::TK_rsort, FTT::keyword},
      {verilog_tokentype::TK_reverse, FTT::keyword},
      {verilog_tokentype::TK_shuffle, FTT::keyword},
      {verilog_tokentype::TK_sum, FTT::keyword},
      {verilog_tokentype::TK_product, FTT::keyword},

      // numeric literals
      {verilog_tokentype::MacroNumericWidth, FTT::numeric_literal},
      {verilog_tokentype::TK_DecNumber, FTT::numeric_literal},
      {verilog_tokentype::TK_RealTime, FTT::numeric_literal},
      {verilog_tokentype::TK_TimeLiteral, FTT::numeric_literal},
      {verilog_tokentype::TK_DecDigits, FTT::numeric_literal},
      {verilog_tokentype::TK_BinDigits, FTT::numeric_literal},
      {verilog_tokentype::TK_OctDigits, FTT::numeric_literal},
      {verilog_tokentype::TK_HexDigits, FTT::numeric_literal},
      {verilog_tokentype::TK_UnBasedNumber, FTT::numeric_literal},

      // numeric bases
      {verilog_tokentype::TK_DecBase, FTT::numeric_base},
      {verilog_tokentype::TK_BinBase, FTT::numeric_base},
      {verilog_tokentype::TK_OctBase, FTT::numeric_base},
      {verilog_tokentype::TK_HexBase, FTT::numeric_base},

      // binary operators
      {verilog_tokentype::TK_PIPEARROW, FTT::binary_operator},
      {verilog_tokentype::TK_PIPEARROW2, FTT::binary_operator},
      {verilog_tokentype::TK_SG, FTT::binary_operator},
      {verilog_tokentype::TK_WILDCARD_EQ, FTT::binary_operator},
      {verilog_tokentype::TK_EQ, FTT::binary_operator},
      {verilog_tokentype::TK_PLUS_EQ, FTT::binary_operator},
      {verilog_tokentype::TK_MINUS_EQ, FTT::binary_operator},
      {verilog_tokentype::TK_MUL_EQ, FTT::binary_operator},
      {verilog_tokentype::TK_DIV_EQ, FTT::binary_operator},
      {verilog_tokentype::TK_MOD_EQ, FTT::binary_operator},
      {verilog_tokentype::TK_AND_EQ, FTT::binary_operator},
      {verilog_tokentype::TK_OR_EQ, FTT::binary_operator},
      {verilog_tokentype::TK_XOR_EQ, FTT::binary_operator},
      {verilog_tokentype::TK_LE, FTT::binary_operator},
      {verilog_tokentype::TK_GE, FTT::binary_operator},
      {verilog_tokentype::TK_EG, FTT::binary_operator},
      {verilog_tokentype::TK_NE, FTT::binary_operator},
      {verilog_tokentype::TK_WILDCARD_NE, FTT::binary_operator},
      {verilog_tokentype::TK_CEQ, FTT::binary_operator},
      {verilog_tokentype::TK_CNE, FTT::binary_operator},
      {verilog_tokentype::TK_LP, FTT::open_group},
      {verilog_tokentype::TK_LS, FTT::binary_operator},
      {verilog_tokentype::TK_RS, FTT::binary_operator},
      {verilog_tokentype::TK_RSS, FTT::binary_operator},
      {verilog_tokentype::TK_CONTRIBUTE, FTT::binary_operator},
      {verilog_tokentype::TK_PO_POS, FTT::binary_operator},
      {verilog_tokentype::TK_PO_NEG, FTT::binary_operator},
      {verilog_tokentype::TK_POW, FTT::binary_operator},
      {verilog_tokentype::TK_LOR, FTT::binary_operator},
      {verilog_tokentype::TK_LAND, FTT::binary_operator},
      {verilog_tokentype::TK_TAND, FTT::binary_operator},
      {verilog_tokentype::TK_NXOR, FTT::binary_operator},
      {verilog_tokentype::TK_LOGEQUIV, FTT::binary_operator},
      {verilog_tokentype::TK_LOGICAL_IMPLIES, FTT::binary_operator},
      {verilog_tokentype::TK_CONSTRAINT_IMPLIES, FTT::binary_operator},
      {verilog_tokentype::TK_COLON_EQ, FTT::binary_operator},
      {verilog_tokentype::TK_COLON_DIV, FTT::binary_operator},
      {verilog_tokentype::TK_POUNDPOUND, FTT::unary_operator},
      {verilog_tokentype::TK_LBSTARRB, FTT::binary_operator},
      {verilog_tokentype::TK_LBPLUSRB, FTT::binary_operator},
      {verilog_tokentype::TK_LBSTAR, FTT::binary_operator},
      {verilog_tokentype::TK_LBEQ, FTT::binary_operator},
      {verilog_tokentype::TK_LBRARROW, FTT::binary_operator},
      {verilog_tokentype::TK_POUNDMINUSPOUND, FTT::binary_operator},
      {verilog_tokentype::TK_POUNDEQPOUND, FTT::binary_operator},
      {verilog_tokentype::TK_ATAT, FTT::binary_operator},
      {verilog_tokentype::TK_SPACE, FTT::binary_operator},
      {verilog_tokentype::TK_NEWLINE, FTT::binary_operator},
      {verilog_tokentype::TK_ATTRIBUTE, FTT::binary_operator},
      {verilog_tokentype::TK_OTHER, FTT::binary_operator},
      {verilog_tokentype::TK_LS_EQ, FTT::binary_operator},
      {verilog_tokentype::TK_RS_EQ, FTT::binary_operator},
      {verilog_tokentype::TK_RSS_EQ, FTT::binary_operator},
      {verilog_tokentype::less_than_TK_else, FTT::binary_operator},

      // This is unlexed text.  TODO(fangism): re-categorize.
      {verilog_tokentype::MacroArg, FTT::binary_operator},

      // balance/grouping tokens
      {verilog_tokentype::MacroCallCloseToEndLine, FTT::close_group},  // ")"

      // identifiers
      {verilog_tokentype::PP_Identifier, FTT::identifier},
      {verilog_tokentype::SymbolIdentifier, FTT::identifier},
      {verilog_tokentype::EscapedIdentifier, FTT::identifier},
      {verilog_tokentype::SystemTFIdentifier, FTT::identifier},
      {verilog_tokentype::MacroIdentifier, FTT::identifier},
      // treat these built-in functions like identifiers
      {verilog_tokentype::TK_Shold, FTT::identifier},
      {verilog_tokentype::TK_Snochange, FTT::identifier},
      {verilog_tokentype::TK_Speriod, FTT::identifier},
      {verilog_tokentype::TK_Srecovery, FTT::identifier},
      {verilog_tokentype::TK_Ssetup, FTT::identifier},
      {verilog_tokentype::TK_Ssetuphold, FTT::identifier},
      {verilog_tokentype::TK_Sskew, FTT::identifier},
      {verilog_tokentype::TK_Swidth, FTT::identifier},
      {verilog_tokentype::TK_Sfullskew, FTT::identifier},
      {verilog_tokentype::TK_Srecrem, FTT::identifier},
      {verilog_tokentype::TK_Sremoval, FTT::identifier},
      {verilog_tokentype::TK_Stimeskew, FTT::identifier},

      // string_literal
      {verilog_tokentype::TK_StringLiteral, FTT::string_literal},
      {verilog_tokentype::TK_EvalStringLiteral, FTT::string_literal},
      {verilog_tokentype::TK_AngleBracketInclude, FTT::string_literal},
      {verilog_tokentype::TK_FILEPATH, FTT::string_literal},

      // unary operators
      {verilog_tokentype::TK_INCR, FTT::unary_operator},
      {verilog_tokentype::TK_DECR, FTT::unary_operator},
      {verilog_tokentype::TK_NAND, FTT::unary_operator},
      {verilog_tokentype::TK_NOR, FTT::unary_operator},
      {verilog_tokentype::TK_TRIGGER, FTT::unary_operator},
      {verilog_tokentype::TK_NONBLOCKING_TRIGGER, FTT::unary_operator},

      // hierarchy
      {verilog_tokentype::TK_SCOPE_RES, FTT::hierarchy},

      // edge descriptors
      {verilog_tokentype::TK_edge_descriptor, FTT::edge_descriptor},

      // various comment styles
      {verilog_tokentype::TK_COMMENT_BLOCK, FTT::comment_block},

      // end of line comment
      {verilog_tokentype::TK_EOL_COMMENT, FTT::eol_comment},

      // TODO(fangism): {verilog_tokentype::TK_LINE_CONT, FTT::???},
  });
  return kFormatTokenMap;
}

FTT GetFormatTokenType(verilog_tokentype e) {
  using verible::container::FindWithDefault;
  // lex/yacc convention: this value is the lower bound of a user-defined
  // token's enum value, where lower values are ASCII character values.
  if (e > 257) {
    return FindWithDefault(FormatTokenTypeMap(), e, FTT::unknown);
  }
  // single char tokens which use ASCII values as enum.
  switch (static_cast<int>(e)) {
    /* arithmetic */
    case '+':
    case '-':
    case '*':
    case '/':
    case '%':
    /* bitwise */
    case '&':
    case '|':
    case '^':
    /* relational */
    case '<':
    case '>':
      return FTT::binary_operator;
    case '?':
      // Technically, ?: is a ternary operator, but nonetheless we
      // space it the same way as a binary operator.
      return FTT::binary_operator;
    // TODO(fangism): handle the ':' separator, but use context-sensitivity
    // to accommodate the cases where spacing is undesirable.
    case '=':
      // Though technically = is an assignment operator, and not an expression
      // operator, we lump it with binary_operator for convenience.
      // It is also used in contexts like default values.
      // Make sure this stays consistent with nonblocking assignment '<=',
      // which happens to be an overloaded TK_LE.
      return FTT::binary_operator;
    case '.':
      // TODO(fangism): this is actually context-dependent.  .port(foo) in a
      // port actual list, vs. a.b.c for a member reference.  distinguish
      // these.
      return FTT::hierarchy;
    case '(':
    case '[':
    case '{':
      return FTT::open_group;
    case ')':
    case ']':
    case '}':
      return FTT::close_group;
    default:
      return FTT::unknown;
  }
}

bool IsComment(FormatTokenType token_type) {
  return (token_type == FTT::eol_comment || token_type == FTT::comment_block);
}

}  // namespace formatter
}  // namespace verilog
