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

#include "verilog/formatting/verilog_token.h"

#include "absl/container/node_hash_map.h"
#include "common/util/container_util.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace formatter {

using FTT = FormatTokenType;

// Mapping of yytokentype enum to FormatTokenType
static const auto* FormatTokenTypeMap =
    new absl::node_hash_map<yytokentype, FTT>({
        // keywords
        {yytokentype::PP_include, FTT::keyword},
        {yytokentype::PP_define, FTT::keyword},
        {yytokentype::PP_define_body, FTT::keyword},
        {yytokentype::PP_ifdef, FTT::keyword},
        {yytokentype::PP_ifndef, FTT::keyword},
        {yytokentype::PP_else, FTT::keyword},
        {yytokentype::PP_elsif, FTT::keyword},
        {yytokentype::PP_endif, FTT::keyword},
        {yytokentype::PP_undef, FTT::keyword},
        {yytokentype::PP_default_text, FTT::keyword},
        {yytokentype::DR_timescale, FTT::keyword},
        {yytokentype::DR_resetall, FTT::keyword},
        {yytokentype::DR_celldefine, FTT::keyword},
        {yytokentype::DR_endcelldefine, FTT::keyword},
        {yytokentype::DR_unconnected_drive, FTT::keyword},
        {yytokentype::DR_nounconnected_drive, FTT::keyword},
        {yytokentype::DR_default_nettype, FTT::keyword},
        {yytokentype::DR_suppress_faults, FTT::keyword},
        {yytokentype::DR_nosuppress_faults, FTT::keyword},
        {yytokentype::DR_enable_portfaults, FTT::keyword},
        {yytokentype::DR_disable_portfaults, FTT::keyword},
        {yytokentype::DR_delay_mode_distributed, FTT::keyword},
        {yytokentype::DR_delay_mode_path, FTT::keyword},
        {yytokentype::DR_delay_mode_unit, FTT::keyword},
        {yytokentype::DR_delay_mode_zero, FTT::keyword},
        {yytokentype::DR_default_decay_time, FTT::keyword},
        {yytokentype::DR_default_trireg_strength, FTT::keyword},
        {yytokentype::DR_pragma, FTT::keyword},
        {yytokentype::DR_uselib, FTT::keyword},
        {yytokentype::DR_begin_keywords, FTT::keyword},
        {yytokentype::DR_end_keywords, FTT::keyword},
        {yytokentype::DR_protect, FTT::keyword},
        {yytokentype::DR_endprotect, FTT::keyword},
        {yytokentype::MacroCallId, FTT::identifier},
        {yytokentype::MacroIdItem, FTT::identifier},
        {yytokentype::TK_DOTSTAR, FTT::keyword},
        {yytokentype::TK_1step, FTT::keyword},
        {yytokentype::TK_always, FTT::keyword},
        {yytokentype::TK_and, FTT::keyword},
        {yytokentype::TK_assign, FTT::keyword},
        {yytokentype::TK_begin, FTT::keyword},
        {yytokentype::TK_buf, FTT::keyword},
        {yytokentype::TK_bufif0, FTT::keyword},
        {yytokentype::TK_bufif1, FTT::keyword},
        {yytokentype::TK_case, FTT::keyword},
        {yytokentype::TK_casex, FTT::keyword},
        {yytokentype::TK_casez, FTT::keyword},
        {yytokentype::TK_cmos, FTT::keyword},
        {yytokentype::TK_deassign, FTT::keyword},
        {yytokentype::TK_default, FTT::keyword},
        {yytokentype::TK_defparam, FTT::keyword},
        {yytokentype::TK_disable, FTT::keyword},
        {yytokentype::TK_edge, FTT::keyword},
        {yytokentype::TK_else, FTT::keyword},
        {yytokentype::TK_end, FTT::keyword},
        {yytokentype::TK_endcase, FTT::keyword},
        {yytokentype::TK_endfunction, FTT::keyword},
        {yytokentype::TK_endmodule, FTT::keyword},
        {yytokentype::TK_endprimitive, FTT::keyword},
        {yytokentype::TK_endspecify, FTT::keyword},
        {yytokentype::TK_endtable, FTT::keyword},
        {yytokentype::TK_endtask, FTT::keyword},
        {yytokentype::TK_event, FTT::keyword},
        {yytokentype::TK_for, FTT::keyword},
        {yytokentype::TK_force, FTT::keyword},
        {yytokentype::TK_forever, FTT::keyword},
        {yytokentype::TK_fork, FTT::keyword},
        {yytokentype::TK_function, FTT::keyword},
        {yytokentype::TK_highz0, FTT::keyword},
        {yytokentype::TK_highz1, FTT::keyword},
        {yytokentype::TK_if, FTT::keyword},
        {yytokentype::TK_ifnone, FTT::keyword},
        {yytokentype::TK_initial, FTT::keyword},
        {yytokentype::TK_inout, FTT::keyword},
        {yytokentype::TK_input, FTT::keyword},
        {yytokentype::TK_integer, FTT::keyword},
        {yytokentype::TK_join, FTT::keyword},
        {yytokentype::TK_large, FTT::keyword},
        {yytokentype::TK_macromodule, FTT::keyword},
        {yytokentype::TK_medium, FTT::keyword},
        {yytokentype::TK_module, FTT::keyword},
        {yytokentype::TK_nand, FTT::keyword},
        {yytokentype::TK_negedge, FTT::keyword},
        {yytokentype::TK_nmos, FTT::keyword},
        {yytokentype::TK_nor, FTT::keyword},
        {yytokentype::TK_not, FTT::keyword},
        {yytokentype::TK_notif0, FTT::keyword},
        {yytokentype::TK_notif1, FTT::keyword},
        {yytokentype::TK_or, FTT::keyword},
        {yytokentype::TK_option, FTT::keyword},
        {yytokentype::TK_output, FTT::keyword},
        {yytokentype::TK_parameter, FTT::keyword},
        {yytokentype::TK_pmos, FTT::keyword},
        {yytokentype::TK_posedge, FTT::keyword},
        {yytokentype::TK_primitive, FTT::keyword},
        {yytokentype::TK_pull0, FTT::keyword},
        {yytokentype::TK_pull1, FTT::keyword},
        {yytokentype::TK_pulldown, FTT::keyword},
        {yytokentype::TK_pullup, FTT::keyword},
        {yytokentype::TK_rcmos, FTT::keyword},
        {yytokentype::TK_real, FTT::keyword},
        {yytokentype::TK_realtime, FTT::keyword},
        {yytokentype::TK_reg, FTT::keyword},
        {yytokentype::TK_release, FTT::keyword},
        {yytokentype::TK_repeat, FTT::keyword},
        {yytokentype::TK_rnmos, FTT::keyword},
        {yytokentype::TK_rpmos, FTT::keyword},
        {yytokentype::TK_rtran, FTT::keyword},
        {yytokentype::TK_rtranif0, FTT::keyword},
        {yytokentype::TK_rtranif1, FTT::keyword},
        {yytokentype::TK_sample, FTT::keyword},
        {yytokentype::TK_scalared, FTT::keyword},
        {yytokentype::TK_small, FTT::keyword},
        {yytokentype::TK_specify, FTT::keyword},
        {yytokentype::TK_specparam, FTT::keyword},
        {yytokentype::TK_strong0, FTT::keyword},
        {yytokentype::TK_strong1, FTT::keyword},
        {yytokentype::TK_supply0, FTT::keyword},
        {yytokentype::TK_supply1, FTT::keyword},
        {yytokentype::TK_table, FTT::keyword},
        {yytokentype::TK_task, FTT::keyword},
        {yytokentype::TK_time, FTT::keyword},
        {yytokentype::TK_tran, FTT::keyword},
        {yytokentype::TK_tranif0, FTT::keyword},
        {yytokentype::TK_tranif1, FTT::keyword},
        {yytokentype::TK_tri, FTT::keyword},
        {yytokentype::TK_tri0, FTT::keyword},
        {yytokentype::TK_tri1, FTT::keyword},
        {yytokentype::TK_triand, FTT::keyword},
        {yytokentype::TK_trior, FTT::keyword},
        {yytokentype::TK_trireg, FTT::keyword},
        {yytokentype::TK_type_option, FTT::keyword},
        {yytokentype::TK_vectored, FTT::keyword},
        {yytokentype::TK_wait, FTT::keyword},
        {yytokentype::TK_wand, FTT::keyword},
        {yytokentype::TK_weak0, FTT::keyword},
        {yytokentype::TK_weak1, FTT::keyword},
        {yytokentype::TK_while, FTT::keyword},
        {yytokentype::TK_wire, FTT::keyword},
        {yytokentype::TK_wor, FTT::keyword},
        {yytokentype::TK_xnor, FTT::keyword},
        {yytokentype::TK_xor, FTT::keyword},
        {yytokentype::TK_Shold, FTT::keyword},
        {yytokentype::TK_Snochange, FTT::keyword},
        {yytokentype::TK_Speriod, FTT::keyword},
        {yytokentype::TK_Srecovery, FTT::keyword},
        {yytokentype::TK_Ssetup, FTT::keyword},
        {yytokentype::TK_Ssetuphold, FTT::keyword},
        {yytokentype::TK_Sskew, FTT::keyword},
        {yytokentype::TK_Swidth, FTT::keyword},
        {yytokentype::TKK_attribute,
         FTT::keyword},  // these look like comments (*...*)
        {yytokentype::TK_bool, FTT::keyword},
        {yytokentype::TK_automatic, FTT::keyword},
        {yytokentype::TK_endgenerate, FTT::keyword},
        {yytokentype::TK_generate, FTT::keyword},
        {yytokentype::TK_genvar, FTT::keyword},
        {yytokentype::TK_localparam, FTT::keyword},
        {yytokentype::TK_noshowcancelled, FTT::keyword},
        {yytokentype::TK_pulsestyle_onevent, FTT::keyword},
        {yytokentype::TK_pulsestyle_ondetect, FTT::keyword},
        {yytokentype::TK_showcancelled, FTT::keyword},
        {yytokentype::TK_signed, FTT::keyword},
        {yytokentype::TK_unsigned, FTT::keyword},
        {yytokentype::TK_Sfullskew, FTT::keyword},
        {yytokentype::TK_Srecrem, FTT::keyword},
        {yytokentype::TK_Sremoval, FTT::keyword},
        {yytokentype::TK_Stimeskew, FTT::keyword},
        {yytokentype::TK_cell, FTT::keyword},
        {yytokentype::TK_config, FTT::keyword},
        {yytokentype::TK_design, FTT::keyword},
        {yytokentype::TK_endconfig, FTT::keyword},
        {yytokentype::TK_incdir, FTT::keyword},
        {yytokentype::TK_include, FTT::keyword},
        {yytokentype::TK_instance, FTT::keyword},
        {yytokentype::TK_liblist, FTT::keyword},
        {yytokentype::TK_library, FTT::keyword},
        {yytokentype::TK_use, FTT::keyword},
        {yytokentype::TK_wone, FTT::keyword},
        {yytokentype::TK_uwire, FTT::keyword},
        {yytokentype::TK_alias, FTT::keyword},
        {yytokentype::TK_always_comb, FTT::keyword},
        {yytokentype::TK_always_ff, FTT::keyword},
        {yytokentype::TK_always_latch, FTT::keyword},
        {yytokentype::TK_assert, FTT::keyword},
        {yytokentype::TK_assume, FTT::keyword},
        {yytokentype::TK_before, FTT::keyword},
        {yytokentype::TK_bind, FTT::keyword},
        {yytokentype::TK_bins, FTT::keyword},
        {yytokentype::TK_binsof, FTT::keyword},
        {yytokentype::TK_bit, FTT::keyword},
        {yytokentype::TK_break, FTT::keyword},
        {yytokentype::TK_byte, FTT::keyword},
        {yytokentype::TK_chandle, FTT::keyword},
        {yytokentype::TK_class, FTT::keyword},
        {yytokentype::TK_clocking, FTT::keyword},
        {yytokentype::TK_const, FTT::keyword},
        {yytokentype::TK_constraint, FTT::keyword},
        {yytokentype::TK_context, FTT::keyword},
        {yytokentype::TK_continue, FTT::keyword},
        {yytokentype::TK_cover, FTT::keyword},
        {yytokentype::TK_covergroup, FTT::keyword},
        {yytokentype::TK_coverpoint, FTT::keyword},
        {yytokentype::TK_cross, FTT::keyword},
        {yytokentype::TK_dist, FTT::keyword},
        {yytokentype::TK_do, FTT::keyword},
        {yytokentype::TK_endclass, FTT::keyword},
        {yytokentype::TK_endclocking, FTT::keyword},
        {yytokentype::TK_endgroup, FTT::keyword},
        {yytokentype::TK_endinterface, FTT::keyword},
        {yytokentype::TK_endpackage, FTT::keyword},
        {yytokentype::TK_endprogram, FTT::keyword},
        {yytokentype::TK_endproperty, FTT::keyword},
        {yytokentype::TK_endsequence, FTT::keyword},
        {yytokentype::TK_enum, FTT::keyword},
        {yytokentype::TK_expect, FTT::keyword},
        {yytokentype::TK_export, FTT::keyword},
        {yytokentype::TK_extends, FTT::keyword},
        {yytokentype::TK_extern, FTT::keyword},
        {yytokentype::TK_final, FTT::keyword},
        {yytokentype::TK_first_match, FTT::keyword},
        {yytokentype::TK_foreach, FTT::keyword},
        {yytokentype::TK_forkjoin, FTT::keyword},
        {yytokentype::TK_iff, FTT::keyword},
        {yytokentype::TK_ignore_bins, FTT::keyword},
        {yytokentype::TK_illegal_bins, FTT::keyword},
        {yytokentype::TK_import, FTT::keyword},
        {yytokentype::TK_inside, FTT::keyword},
        {yytokentype::TK_int, FTT::keyword},
        {yytokentype::TK_interface, FTT::keyword},
        {yytokentype::TK_intersect, FTT::keyword},
        {yytokentype::TK_join_any, FTT::keyword},
        {yytokentype::TK_join_none, FTT::keyword},
        {yytokentype::TK_local, FTT::keyword},
        {yytokentype::TK_local_SCOPE, FTT::keyword},
        {yytokentype::TK_logic, FTT::keyword},
        {yytokentype::TK_longint, FTT::keyword},
        {yytokentype::TK_matches, FTT::keyword},
        {yytokentype::TK_modport, FTT::keyword},
        {yytokentype::TK_new, FTT::keyword},
        {yytokentype::TK_null, FTT::keyword},
        {yytokentype::TK_package, FTT::keyword},
        {yytokentype::TK_packed, FTT::keyword},
        {yytokentype::TK_priority, FTT::keyword},
        {yytokentype::TK_program, FTT::keyword},
        {yytokentype::TK_property, FTT::keyword},
        {yytokentype::TK_protected, FTT::keyword},
        {yytokentype::TK_pure, FTT::keyword},
        {yytokentype::TK_rand, FTT::keyword},
        {yytokentype::TK_randc, FTT::keyword},
        {yytokentype::TK_randcase, FTT::keyword},
        {yytokentype::TK_randsequence, FTT::keyword},
        {yytokentype::TK_randomize, FTT::keyword},
        {yytokentype::TK_ref, FTT::keyword},
        {yytokentype::TK_return, FTT::keyword},
        {yytokentype::TK_Sroot, FTT::keyword},
        {yytokentype::TK_sequence, FTT::keyword},
        {yytokentype::TK_shortint, FTT::keyword},
        {yytokentype::TK_shortreal, FTT::keyword},
        {yytokentype::TK_solve, FTT::keyword},
        {yytokentype::TK_static, FTT::keyword},
        {yytokentype::TK_string, FTT::keyword},
        {yytokentype::TK_struct, FTT::keyword},
        {yytokentype::TK_super, FTT::keyword},
        {yytokentype::TK_tagged, FTT::keyword},
        {yytokentype::TK_this, FTT::keyword},
        {yytokentype::TK_throughout, FTT::keyword},
        {yytokentype::TK_timeprecision, FTT::keyword},
        {yytokentype::TK_timeunit, FTT::keyword},
        {yytokentype::TK_timescale_unit, FTT::keyword},
        {yytokentype::TK_type, FTT::keyword},
        {yytokentype::TK_typedef, FTT::keyword},
        {yytokentype::TK_union, FTT::keyword},
        {yytokentype::TK_unique, FTT::keyword},
        {yytokentype::TK_unique_index, FTT::keyword},
        {yytokentype::TK_Sunit, FTT::keyword},
        {yytokentype::TK_var, FTT::keyword},
        {yytokentype::TK_virtual, FTT::keyword},
        {yytokentype::TK_void, FTT::keyword},
        {yytokentype::TK_wait_order, FTT::keyword},
        {yytokentype::TK_wildcard, FTT::keyword},
        {yytokentype::TK_with, FTT::keyword},
        {yytokentype::TK_with__covergroup, FTT::keyword},
        {yytokentype::TK_within, FTT::keyword},
        {yytokentype::TK_timeprecision_check, FTT::keyword},
        {yytokentype::TK_timeunit_check, FTT::keyword},
        {yytokentype::TK_accept_on, FTT::keyword},
        {yytokentype::TK_checker, FTT::keyword},
        {yytokentype::TK_endchecker, FTT::keyword},
        {yytokentype::TK_eventually, FTT::keyword},
        {yytokentype::TK_global, FTT::keyword},
        {yytokentype::TK_implies, FTT::keyword},
        {yytokentype::TK_let, FTT::keyword},
        {yytokentype::TK_nexttime, FTT::keyword},
        {yytokentype::TK_reject_on, FTT::keyword},
        {yytokentype::TK_restrict, FTT::keyword},
        {yytokentype::TK_s_always, FTT::keyword},
        {yytokentype::TK_s_eventually, FTT::keyword},
        {yytokentype::TK_s_nexttime, FTT::keyword},
        {yytokentype::TK_s_until, FTT::keyword},
        {yytokentype::TK_s_until_with, FTT::keyword},
        {yytokentype::TK_strong, FTT::keyword},
        {yytokentype::TK_sync_accept_on, FTT::keyword},
        {yytokentype::TK_sync_reject_on, FTT::keyword},
        {yytokentype::TK_unique0, FTT::keyword},
        {yytokentype::TK_until, FTT::keyword},
        {yytokentype::TK_until_with, FTT::keyword},
        {yytokentype::TK_untyped, FTT::keyword},
        {yytokentype::TK_weak, FTT::keyword},
        {yytokentype::TK_implements, FTT::keyword},
        {yytokentype::TK_interconnect, FTT::keyword},
        {yytokentype::TK_nettype, FTT::keyword},
        {yytokentype::TK_soft, FTT::keyword},
        {yytokentype::TK_above, FTT::keyword},
        {yytokentype::TK_abs, FTT::keyword},
        {yytokentype::TK_absdelay, FTT::keyword},
        {yytokentype::TK_abstol, FTT::keyword},
        {yytokentype::TK_access, FTT::keyword},
        {yytokentype::TK_acos, FTT::keyword},
        {yytokentype::TK_acosh, FTT::keyword},
        {yytokentype::TK_ac_stim, FTT::keyword},
        {yytokentype::TK_aliasparam, FTT::keyword},
        {yytokentype::TK_analog, FTT::keyword},
        {yytokentype::TK_analysis, FTT::keyword},
        {yytokentype::TK_asin, FTT::keyword},
        {yytokentype::TK_asinh, FTT::keyword},
        {yytokentype::TK_atan, FTT::keyword},
        {yytokentype::TK_atan2, FTT::keyword},
        {yytokentype::TK_atanh, FTT::keyword},
        {yytokentype::TK_branch, FTT::keyword},
        {yytokentype::TK_ceil, FTT::keyword},
        {yytokentype::TK_connect, FTT::keyword},
        {yytokentype::TK_connectmodule, FTT::keyword},
        {yytokentype::TK_connectrules, FTT::keyword},
        {yytokentype::TK_continuous, FTT::keyword},
        {yytokentype::TK_cos, FTT::keyword},
        {yytokentype::TK_cosh, FTT::keyword},
        {yytokentype::TK_ddt, FTT::keyword},
        {yytokentype::TK_ddt_nature, FTT::keyword},
        {yytokentype::TK_ddx, FTT::keyword},
        {yytokentype::TK_discipline, FTT::keyword},
        {yytokentype::TK_discrete, FTT::keyword},
        {yytokentype::TK_domain, FTT::keyword},
        {yytokentype::TK_driver_update, FTT::keyword},
        {yytokentype::TK_endconnectrules, FTT::keyword},
        {yytokentype::TK_enddiscipline, FTT::keyword},
        {yytokentype::TK_endnature, FTT::keyword},
        {yytokentype::TK_endparamset, FTT::keyword},
        {yytokentype::TK_exclude, FTT::keyword},
        {yytokentype::TK_exp, FTT::keyword},
        {yytokentype::TK_final_step, FTT::keyword},
        {yytokentype::TK_flicker_noise, FTT::keyword},
        {yytokentype::TK_floor, FTT::keyword},
        {yytokentype::TK_flow, FTT::keyword},
        {yytokentype::TK_from, FTT::keyword},
        {yytokentype::TK_ground, FTT::keyword},
        {yytokentype::TK_hypot, FTT::keyword},
        {yytokentype::TK_idt, FTT::keyword},
        {yytokentype::TK_idtmod, FTT::keyword},
        {yytokentype::TK_idt_nature, FTT::keyword},
        {yytokentype::TK_inf, FTT::keyword},
        {yytokentype::TK_infinite, FTT::keyword},
        {yytokentype::TK_initial_step, FTT::keyword},
        {yytokentype::TK_laplace_nd, FTT::keyword},
        {yytokentype::TK_laplace_np, FTT::keyword},
        {yytokentype::TK_laplace_zd, FTT::keyword},
        {yytokentype::TK_laplace_zp, FTT::keyword},
        {yytokentype::TK_last_crossing, FTT::keyword},
        {yytokentype::TK_limexp, FTT::keyword},
        {yytokentype::TK_ln, FTT::keyword},
        {yytokentype::TK_log, FTT::keyword},
        {yytokentype::TK_max, FTT::keyword},
        {yytokentype::TK_merged, FTT::keyword},
        {yytokentype::TK_min, FTT::keyword},
        {yytokentype::TK_nature, FTT::keyword},
        {yytokentype::TK_net_resolution, FTT::keyword},
        {yytokentype::TK_noise_table, FTT::keyword},
        {yytokentype::TK_paramset, FTT::keyword},
        {yytokentype::TK_potential, FTT::keyword},
        {yytokentype::TK_pow, FTT::keyword},
        {yytokentype::TK_resolveto, FTT::keyword},
        {yytokentype::TK_sin, FTT::keyword},
        {yytokentype::TK_sinh, FTT::keyword},
        {yytokentype::TK_slew, FTT::keyword},
        {yytokentype::TK_split, FTT::keyword},
        {yytokentype::TK_sqrt, FTT::keyword},
        {yytokentype::TK_tan, FTT::keyword},
        {yytokentype::TK_tanh, FTT::keyword},
        {yytokentype::TK_timer, FTT::keyword},
        {yytokentype::TK_transition, FTT::keyword},
        {yytokentype::TK_units, FTT::keyword},
        {yytokentype::TK_white_noise, FTT::keyword},
        {yytokentype::TK_wreal, FTT::keyword},
        {yytokentype::TK_zi_nd, FTT::keyword},
        {yytokentype::TK_zi_np, FTT::keyword},
        {yytokentype::TK_zi_zd, FTT::keyword},
        {yytokentype::TK_zi_zp, FTT::keyword},

        // TODO(fangism): These are built-in function identifiers, and there
        // are even more above, e.g. math functions.
        {yytokentype::TK_find, FTT::keyword},
        {yytokentype::TK_find_index, FTT::keyword},
        {yytokentype::TK_find_first, FTT::keyword},
        {yytokentype::TK_find_first_index, FTT::keyword},
        {yytokentype::TK_find_last, FTT::keyword},
        {yytokentype::TK_find_last_index, FTT::keyword},
        {yytokentype::TK_sort, FTT::keyword},
        {yytokentype::TK_rsort, FTT::keyword},
        {yytokentype::TK_reverse, FTT::keyword},
        {yytokentype::TK_shuffle, FTT::keyword},
        {yytokentype::TK_sum, FTT::keyword},
        {yytokentype::TK_product, FTT::keyword},

        // numeric literals
        {yytokentype::MacroNumericWidth, FTT::numeric_literal},
        {yytokentype::TK_DecNumber, FTT::numeric_literal},
        {yytokentype::TK_RealTime, FTT::numeric_literal},
        {yytokentype::TK_TimeLiteral, FTT::numeric_literal},
        {yytokentype::TK_DecDigits, FTT::numeric_literal},
        {yytokentype::TK_BinDigits, FTT::numeric_literal},
        {yytokentype::TK_OctDigits, FTT::numeric_literal},
        {yytokentype::TK_HexDigits, FTT::numeric_literal},
        {yytokentype::TK_UnBasedNumber, FTT::numeric_literal},

        // numeric bases
        {yytokentype::TK_DecBase, FTT::numeric_base},
        {yytokentype::TK_BinBase, FTT::numeric_base},
        {yytokentype::TK_OctBase, FTT::numeric_base},
        {yytokentype::TK_HexBase, FTT::numeric_base},

        // binary operators
        {yytokentype::TK_PIPEARROW, FTT::binary_operator},
        {yytokentype::TK_PIPEARROW2, FTT::binary_operator},
        {yytokentype::TK_SG, FTT::binary_operator},
        {yytokentype::TK_WILDCARD_EQ, FTT::binary_operator},
        {yytokentype::TK_EQ, FTT::binary_operator},
        {yytokentype::TK_PLUS_EQ, FTT::binary_operator},
        {yytokentype::TK_MINUS_EQ, FTT::binary_operator},
        {yytokentype::TK_MUL_EQ, FTT::binary_operator},
        {yytokentype::TK_DIV_EQ, FTT::binary_operator},
        {yytokentype::TK_MOD_EQ, FTT::binary_operator},
        {yytokentype::TK_AND_EQ, FTT::binary_operator},
        {yytokentype::TK_OR_EQ, FTT::binary_operator},
        {yytokentype::TK_XOR_EQ, FTT::binary_operator},
        {yytokentype::TK_LE, FTT::binary_operator},
        {yytokentype::TK_GE, FTT::binary_operator},
        {yytokentype::TK_EG, FTT::binary_operator},
        {yytokentype::TK_NE, FTT::binary_operator},
        {yytokentype::TK_WILDCARD_NE, FTT::binary_operator},
        {yytokentype::TK_CEQ, FTT::binary_operator},
        {yytokentype::TK_CNE, FTT::binary_operator},
        {yytokentype::TK_LP, FTT::open_group},
        {yytokentype::TK_LS, FTT::binary_operator},
        {yytokentype::TK_RS, FTT::binary_operator},
        {yytokentype::TK_RSS, FTT::binary_operator},
        {yytokentype::TK_CONTRIBUTE, FTT::binary_operator},
        {yytokentype::TK_PO_POS, FTT::binary_operator},
        {yytokentype::TK_PO_NEG, FTT::binary_operator},
        {yytokentype::TK_POW, FTT::binary_operator},
        {yytokentype::TK_LOR, FTT::binary_operator},
        {yytokentype::TK_LAND, FTT::binary_operator},
        {yytokentype::TK_TAND, FTT::binary_operator},
        {yytokentype::TK_NXOR, FTT::binary_operator},
        {yytokentype::TK_LOGEQUIV, FTT::binary_operator},
        {yytokentype::TK_TRIGGER, FTT::binary_operator},
        {yytokentype::TK_LOGICAL_IMPLIES, FTT::binary_operator},
        {yytokentype::TK_CONSTRAINT_IMPLIES, FTT::binary_operator},
        {yytokentype::TK_COLON_EQ, FTT::binary_operator},
        {yytokentype::TK_COLON_DIV, FTT::binary_operator},
        {yytokentype::TK_POUNDPOUND, FTT::binary_operator},
        {yytokentype::TK_LBSTARRB, FTT::binary_operator},
        {yytokentype::TK_LBPLUSRB, FTT::binary_operator},
        {yytokentype::TK_LBSTAR, FTT::binary_operator},
        {yytokentype::TK_LBEQ, FTT::binary_operator},
        {yytokentype::TK_LBRARROW, FTT::binary_operator},
        {yytokentype::TK_POUNDMINUSPOUND, FTT::binary_operator},
        {yytokentype::TK_POUNDEQPOUND, FTT::binary_operator},
        {yytokentype::TK_ATAT, FTT::binary_operator},
        {yytokentype::TK_SPACE, FTT::binary_operator},
        {yytokentype::TK_NEWLINE, FTT::binary_operator},
        {yytokentype::TK_ATTRIBUTE, FTT::binary_operator},
        {yytokentype::TK_OTHER, FTT::binary_operator},
        {yytokentype::TK_LS_EQ, FTT::binary_operator},
        {yytokentype::TK_RS_EQ, FTT::binary_operator},
        {yytokentype::TK_RSS_EQ, FTT::binary_operator},
        {yytokentype::less_than_TK_else, FTT::binary_operator},

        // This is unlexed text.  TODO(fangism): re-categorize.
        {yytokentype::MacroArg, FTT::binary_operator},

        // balance/grouping tokens
        {yytokentype::MacroCallCloseToEndLine, FTT::close_group},  // ")"

        // identifiers
        {yytokentype::PP_Identifier, FTT::identifier},
        {yytokentype::SymbolIdentifier, FTT::identifier},
        {yytokentype::EscapedIdentifier, FTT::identifier},
        {yytokentype::SystemTFIdentifier, FTT::identifier},
        {yytokentype::MacroIdentifier, FTT::identifier},

        // string_literal
        {yytokentype::TK_StringLiteral, FTT::string_literal},

        // unary operators
        {yytokentype::TK_INCR, FTT::unary_operator},
        {yytokentype::TK_DECR, FTT::unary_operator},
        {yytokentype::TK_NAND, FTT::unary_operator},
        {yytokentype::TK_NOR, FTT::unary_operator},

        // hierarchy
        {yytokentype::TK_SCOPE_RES, FTT::hierarchy},

        // edge descriptors
        {yytokentype::TK_edge_descriptor, FTT::edge_descriptor},

        // various comment styles
        {yytokentype::TK_COMMENT_BLOCK, FTT::comment_block},

        // end of line comment
        {yytokentype::TK_EOL_COMMENT, FTT::eol_comment},

        // TODO(fangism): {yytokentype::TK_LINE_CONT, FTT::???},
    });

FTT GetFormatTokenType(yytokentype e) {
  using verible::container::FindWithDefault;
  // lex/yacc convention: this value is the lower bound of a user-defined
  // token's enum value, where lower values are ASCII character values.
  if (e > 257) {
    return FindWithDefault(*FormatTokenTypeMap, e, FTT::unknown);
  } else {
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
}

bool IsComment(FormatTokenType token_type) {
  return (token_type == FTT::eol_comment || token_type == FTT::comment_block);
}

bool IsComment(yytokentype token_type) {
  return (token_type == yytokentype::TK_COMMENT_BLOCK ||
          token_type == yytokentype::TK_EOL_COMMENT);
}

bool IsUnaryOperator(yytokentype token_type) {
  switch (static_cast<int>(token_type)) {
    // See verilog/parser/verilog.y
    // TODO(fangism): find a way to generate this function automatically
    // from the yacc file, perhaps with extra annotations or metadata.
    case '+':
    case '-':
    case '~':
    case '&':
    case '!':
    case '|':
    case '^':
    case TK_NAND:
    case TK_NOR:
    case TK_NXOR:
    case TK_INCR:
    case TK_DECR:
      return true;
    default:
      return false;
  }
}

bool IsPreprocessorControlFlow(yytokentype token_type) {
  switch (static_cast<int>(token_type)) {
    case yytokentype::PP_ifdef:
    case yytokentype::PP_ifndef:
    case yytokentype::PP_elsif:
    case yytokentype::PP_else:
    case yytokentype::PP_endif:
      return true;
    default:
      break;
  }
  return false;
}

bool IsEndKeyword(yytokentype token_type) {
  switch (static_cast<int>(token_type)) {
    case yytokentype::TK_end:
    case yytokentype::TK_endcase:
    case yytokentype::TK_endgroup:
    case yytokentype::TK_endpackage:
    case yytokentype::TK_endgenerate:
    case yytokentype::TK_endinterface:
    case yytokentype::TK_endfunction:
    case yytokentype::TK_endtask:
    case yytokentype::TK_endproperty:
    case yytokentype::TK_endclocking:
    case yytokentype::TK_endclass:
    case yytokentype::TK_endmodule:
      // TODO(fangism): join and join* keywords?
      return true;
    default:
      break;
  }
  return false;
}

}  // namespace formatter
}  // namespace verilog
