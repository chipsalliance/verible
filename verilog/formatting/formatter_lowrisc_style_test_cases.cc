// Copyright 2017-2021 The Verible Authors.
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

// Test cases in this file should check LowRISC style compliance
// Other test cases should be placed in formatter_test.cc and
//     formatter_tuning_test.cc

#include "verilog/formatting/formatter_lowrisc_style_test_cases.h"

namespace verilog {
namespace formatter {
namespace tests {

static const ComplianceTestCase kComplianceTestCases[] = {
  {"Constraint blocks\n"
   "\n"
   "tags: constraint block formatter\n"
   "\n"
   "lrm: IEEE Std 1800-2017 18.5 \"Constraint blocks\"\n"
   "\n"
   "Related:\n"
   "https://github.com/google/verible/issues/445\n"
   "https://github.com/google/verible/issues/445#issuecomment-806232188\n"},

  {
    "Expand expression containing brackets (if-statement)",

    LowRISCFormatStyle(),

    "constraint c_iv {"
    "    if (fixed_iv_en) {"
    "        aes_iv == fixed_iv"
    "    };"
    "}",

    // Expected
    "constraint c_iv {\n"
    "  if (fixed_iv_en)\n"
    "  {aes_iv == fixed_iv};\n"
    "}\n",

    // Compliant
    "constraint c_iv {\n"
    "  if (fixed_iv_en) {\n"
    "    aes_iv == fixed_iv\n"
    "  };\n"
    "}\n",
  },
  {
    "Expand expression containing brackets",

    LowRISCFormatStyle(),

    "constraint data_size_c {\n"
    "    data.size() inside {[1:65536]};\n"
    "  }\n",

    "constraint data_size_c {data.size() inside {[1 : 65536]};}\n",

    "constraint data_size_c {\n"
    "  data.size() inside {[1:65536]};\n"
    "}\n",
  },

  {
    "Expand blocks with two or more expressions (two statements)",

    LowRISCFormatStyle(),

    "constraint param_c {\n"
    "  a_param == 0;\n"
    "  d_param == 0;\n"
    "}\n",

    "constraint param_c {\n"
    "  a_param == 0;\n"
    "  d_param == 0;\n"
    "}\n",
  },

  {
    "Compact constraint blocks with one expression",

    LowRISCFormatStyle(),

    "constraint only_vec_instr_c {soft only_vec_instr == 0;}",
    "constraint only_vec_instr_c {soft only_vec_instr == 0;}\n",
  },
  {
    "Compact blocks with one expression (column limited to 40)",

    LowRISCFormatStyle(40),

    "constraint only_vec_instr_c {soft only_vec_instr == 0;}",

    "constraint only_vec_instr_c {\n"
    "  soft only_vec_instr == 0;\n"
    "}\n",
  },
  {
    "Compact blocks with one expression (function call)",

    LowRISCFormatStyle(),

    "constraint mask_contiguous_c {\n"
    "  $countones(a_mask ^ {a_mask[MaskWidth-2:0], 1'b0}) <= 2;\n"
    "}\n",

    "constraint mask_contiguous_c {\n"
    "  $countones(\n"
    "      a_mask ^ {a_mask[MaskWidth-2:0], 1'b0}\n"
    "  ) <= 2;\n"
    "}\n",

    "constraint mask_contiguous_c {\n"
    "  $countones(a_mask ^ {a_mask[MaskWidth-2:0], 1'b0}) <= 2;\n"
    "}\n",
  },
  {
    "Compact blocks with one expression",

    LowRISCFormatStyle(),

    "constraint d_opcode_c {\n"
    "  d_opcode inside {AccessAckData, AccessAck};\n"
    "}\n",

    "constraint d_opcode_c {d_opcode inside {AccessAckData, AccessAck};}\n",

    "constraint d_opcode_c {\n"
    "  d_opcode inside {AccessAckData, AccessAck};\n"
    "}\n",
  },


  {
    "Functional coverage\n"
    "\n"
    "LRM: IEEE Std 1800-2017 19.3 \"Defining the coverage model: covergroup\"\n"
  },
  {
    "ref: https://github.com/lowRISC/opentitan/blob/"
    "8933d96c28e0e1054ea488d56940093109451c68/hw/dv/sv/"
    "cip_lib/cip_base_env_cov.sv",

    LowRISCFormatStyle(),

    "covergroup intr_cg (uint num_interrupts) with function sample(uint intr,\n"
    "                                                              bit intr_en,\n"
    "                                                              bit intr_state);\n"
    "endgroup\n",

    "covergroup intr_cg(\n"
    "    uint num_interrupts\n"
    ") with function sample (\n"
    "    uint intr, bit intr_en, bit intr_state\n"
    ");\n"
    "endgroup\n",

    "covergroup intr_cg (uint num_interrupts) with function sample(uint intr,\n"
    "                                                              bit intr_en,\n"
    "                                                              bit intr_state);\n"
    "endgroup\n",
  },
  // FIXME(ldk): column_limit 80, 50, etc...


  {"Import declarations\n"
   "\n"
   "tags: declaration dpi import\n"
   "\n"
   "LRM:\n"
   "IEEE Std 1800-2017 35.5.4 \"Import declarations\"\n"},

  {
    "Import declarations",

    LowRISCFormatStyle(),

    "import \"DPI-C\" function chandle spidpi_create(input string name, input int mode,\n"
    "                                              input int loglevel);\n",
   
    "import \"DPI-C\" function chandle spidpi_create(input string name, input int mode,\n"
    "                                              input int loglevel);\n",

    "import \"DPI-C\"\n"
    "function chandle spidpi_create(input string name, input int mode,\n"
    "                               input int loglevel);\n",
  },
  {
    "",

    LowRISCFormatStyle(),

    "import \"DPI-C\""
    "function void dmidpi_tick(input chandle ctx, output bit dmi_req_valid,"
    "                          input bit dmi_req_ready, output bit [6:0] dmi_req_addr,"
    "                          output bit [1:0] dmi_req_op, output bit [31:0] dmi_req_data,"
    "                          input bit dmi_rsp_valid, output bit dmi_rsp_ready,"
    "                          input bit [31:0] dmi_rsp_data, input bit [1:0] dmi_rsp_resp,"
    "                          output bit dmi_rst_n);",

    "import \"DPI-C\" function void dmidpi_tick(\n"
    "    input chandle ctx, output bit dmi_req_valid, input bit dmi_req_ready,\n"
    "    output bit [6:0] dmi_req_addr, output bit [1:0] dmi_req_op, output bit [31:0] dmi_req_data,\n"
    "    input bit dmi_rsp_valid, output bit dmi_rsp_ready, input bit [31:0] dmi_rsp_data,\n"
    "    input bit [1:0] dmi_rsp_resp, output bit dmi_rst_n);\n",

    "import \"DPI-C\"\n"
    "function void dmidpi_tick(input chandle ctx, output bit dmi_req_valid,\n"
    "                          input bit dmi_req_ready, output bit [6:0] dmi_req_addr,\n"
    "                          output bit [1:0] dmi_req_op, output bit [31:0] dmi_req_data,\n"
    "                          input bit dmi_rsp_valid, output bit dmi_rsp_ready,\n"
    "                          input bit [31:0] dmi_rsp_data, input bit [1:0] dmi_rsp_resp,\n"
    "                          output bit dmi_rst_n);\n",
  },

  {"Continuous assignments"},

  {
    "Continuous assignment should be in one line (if fits)",

    LowRISCFormatStyle(),

    "assign d2p = {spi_device_sdo_i, spi_device_sdo_en_i};\n",

    "assign d2p = {spi_device_sdo_i, spi_device_sdo_en_i};\n",
  },
  {
    "Continuous assignment (column limited to 40)",

    LowRISCFormatStyle(40),

    "assign d2p = {spi_device_sdo_i, spi_device_sdo_en_i};\n",

    "assign d2p = {\n"
    "  spi_device_sdo_i, spi_device_sdo_en_i\n"
    "};\n",

    // Desired output
    "assign d2p = {\n"
    "  spi_device_sdo_i,\n"
    "  spi_device_sdo_en_i\n"
    "};\n",
  },
  {
    "",

    LowRISCFormatStyle(20),

    "assign d2p = {spi_device_sdo_i, spi_device_sdo_en_i};\n",

    "assign d2p = {\n"
    "  spi_device_sdo_i,\n"
    "  spi_device_sdo_en_i\n"
    "};\n",
  },


  {"Module declaration"},

  {
    "Module parameters",

    LowRISCFormatStyle(),

    "module spidpi\n"
    "  #(\n"
    "  parameter string NAME = \"spi0\",\n"
    "  parameter MODE = 0,\n"
    "  parameter LOG_LEVEL = 9\n"
    ");\n"
    "endmodule",

    "module spidpi #(\n"
    "  parameter string NAME      = \"spi0\",\n"
    "  parameter        MODE      = 0,\n"
    "  parameter        LOG_LEVEL = 9\n"
    ");\n"
    "endmodule\n",
  },

  {
    "Module port list",

    LowRISCFormatStyle(),

    "module spidpi ("
    "input  logic clk_i,"
    "input  logic rst_ni,"
    "output logic spi_device_sck_o,"
    "output logic spi_device_csb_o,"
    "output logic spi_device_sdi_o,"
    "input  logic spi_device_sdo_i,"
    "input  logic spi_device_sdo_en_i);"
    "endmodule",

    "module spidpi (\n"
    "    input  logic clk_i,\n"
    "    input  logic rst_ni,\n"
    "    output logic spi_device_sck_o,\n"
    "    output logic spi_device_csb_o,\n"
    "    output logic spi_device_sdi_o,\n"
    "    input  logic spi_device_sdo_i,\n"
    "    input  logic spi_device_sdo_en_i\n"
    ");\n"
    "endmodule\n",
  },

  {
    "Module with ports and parameters",

    LowRISCFormatStyle(),

    "module spidpi"
    "  #("
    "  parameter string NAME = \"spi0\","
    "  parameter MODE = 0,"
    "  parameter LOG_LEVEL = 9"
    "  )("
    "  input logic clk_i,"
    "  input logic rst_ni,"
    "  output logic spi_device_sck_o,"
    "  output logic spi_device_csb_o,"
    "  output logic spi_device_sdi_o,"
    "  input logic spi_device_sdo_i,"
    "  input logic spi_device_sdo_en_i"
    ");endmodule",

    "module spidpi #(\n"
    "  parameter string NAME      = \"spi0\",\n"
    "  parameter        MODE      = 0,\n"
    "  parameter        LOG_LEVEL = 9\n"
    ") (\n"
    "    input logic clk_i,\n"
    "    input logic rst_ni,\n"
    "    output logic spi_device_sck_o,\n"
    "    output logic spi_device_csb_o,\n"
    "    output logic spi_device_sdi_o,\n"
    "    input logic spi_device_sdo_i,\n"
    "    input logic spi_device_sdo_en_i\n"
    ");\n"
    "endmodule\n",

    "module spidpi #(\n"
    "  parameter string NAME      = \"spi0\",\n"
    "  parameter        MODE      = 0,\n"
    "  parameter        LOG_LEVEL = 9\n"
    ") (\n"
    "  input  logic clk_i,\n"
    "  input  logic rst_ni,\n"
    "  output logic spi_device_sck_o,\n"
    "  output logic spi_device_csb_o,\n"
    "  output logic spi_device_sdi_o,\n"
    "  input  logic spi_device_sdo_i,\n"
    "  input  logic spi_device_sdo_en_i\n"
    ");\n"
    "endmodule\n",
  },

  {"Binary operators"},

  {
    "Binary operators",

    LowRISCFormatStyle(100),

    "parameter int KMAC_REQ_DATA_WIDTH = keymgr_pkg::KmacDataIfWidth\n"
    "                                    + keymgr_pkg::KmacDataIfWidth / 8\n"
    "                                    + 1;\n",

    "parameter\n"
    "    int KMAC_REQ_DATA_WIDTH = keymgr_pkg::KmacDataIfWidth + keymgr_pkg::KmacDataIfWidth / 8 + 1;\n"
  },
  {
    "",

    LowRISCFormatStyle(80),

    "parameter int KMAC_REQ_DATA_WIDTH = keymgr_pkg::KmacDataIfWidth\n"
    "                                    + keymgr_pkg::KmacDataIfWidth / 8\n"
    "                                    + 1;\n",

    "parameter int KMAC_REQ_DATA_WIDTH =\n"
    "    keymgr_pkg::KmacDataIfWidth + keymgr_pkg::KmacDataIfWidth / 8 + 1;\n"
  },

  {
    "ref: https://github.com/lowRISC/opentitan/blob/"
    "8933d96c28e0e1054ea488d56940093109451c68/"
    "hw/dv/sv/csr_utils/csr_seq_lib.sv",

    LowRISCFormatStyle(),

    "class csr_aliasing_seq extends csr_base_seq;\n"
    "  virtual task body();\n"
    "      foreach (all_csrs[j]) begin\n"
    "        if (is_excl(all_csrs[j], CsrExclInitCheck, CsrAliasingTest) ||\n"
    "            is_excl(all_csrs[j], CsrExclWriteCheck, CsrAliasingTest)) begin\n"
    "        end\n"
    "    end\n"
    "  endtask\n"
    "endclass\n",

    "class csr_aliasing_seq extends csr_base_seq;\n"
    "  virtual task body();\n"
    "    foreach (all_csrs[j]) begin\n"
    "      if (is_excl(\n"
    "              all_csrs[j], CsrExclInitCheck, CsrAliasingTest\n"
    "          ) || is_excl(\n"
    "              all_csrs[j], CsrExclWriteCheck, CsrAliasingTest\n"
    "          )) begin\n"
    "      end\n"
    "    end\n"
    "  endtask\n"
    "endclass\n",

    "class csr_aliasing_seq extends csr_base_seq;\n"
    "  virtual task body();\n"
    "      foreach (all_csrs[j]) begin\n"
    "        if (is_excl(all_csrs[j], CsrExclInitCheck, CsrAliasingTest) ||\n"
    "            is_excl(all_csrs[j], CsrExclWriteCheck, CsrAliasingTest)) begin\n"
    "        end\n"
    "    end\n"
    "  endtask\n"
    "endclass\n",
  },

  {
    "Ternary operators"
  },

  {
    "ref: https://github.com/lowRISC/opentitan/blob/"
    "8933d96c28e0e1054ea488d56940093109451c68/"
    "hw/dv/sv/alert_esc_agent/esc_receiver_driver.sv",

    LowRISCFormatStyle(),

    "class esc_receiver_driver extends alert_esc_base_driver;\n"
    "  virtual task drive_esc_resp(alert_esc_seq_item req);\n"
    "        int toggle_cycle = req.int_err ? cfg.ping_timeout_cycle / 2 : 1;\n"
    "  endtask\n"
    "endclass",

    "class esc_receiver_driver extends alert_esc_base_driver;\n"
    "  virtual task drive_esc_resp(alert_esc_seq_item req);\n"
    "    int toggle_cycle = req.int_err ? cfg.ping_timeout_cycle / 2 : 1;\n"
    "  endtask\n"
    "endclass\n"
  },

  {
    "ref: https://github.com/lowRISC/opentitan/blob/"
    "8933d96c28e0e1054ea488d56940093109451c68/"
    "hw/dv/sv/alert_esc_agent/alert_receiver_driver.sv",

    LowRISCFormatStyle(),

    "class alert_receiver_driver extends alert_esc_base_driver;\n"
    "  virtual task drive_alert_ping(alert_esc_seq_item req);\n"
    "    int unsigned ping_delay = (cfg.use_seq_item_ping_delay) ? req.ping_delay :\n"
    "                               $urandom_range(cfg.ping_delay_max, cfg.ping_delay_min);\n"
    "  endtask\n"
    "endclass\n",

    "class alert_receiver_driver extends alert_esc_base_driver;\n"
    "  virtual task drive_alert_ping(alert_esc_seq_item req);\n"
    "    int unsigned ping_delay = (cfg.use_seq_item_ping_delay) ? req.ping_delay : $urandom_range(\n"
    "        cfg.ping_delay_max, cfg.ping_delay_min\n"
    "    );\n"
    "  endtask\n"
    "endclass\n",

    "class alert_receiver_driver extends alert_esc_base_driver;\n"
    "  virtual task drive_alert_ping(alert_esc_seq_item req);\n"
    "    int unsigned ping_delay = (cfg.use_seq_item_ping_delay) ? req.ping_delay :\n"
    "                               $urandom_range(cfg.ping_delay_max, cfg.ping_delay_min);\n"
    "  endtask\n"
    "endclass\n",
  },

  {"Labels\n"
   "\n"
   "StyleGuide:\n"
   "https://github.com/lowRISC/style-guides/blob/master/VerilogCodingStyle.md#labels\n"},

  {
    "When labeling code blocks, add one space before and after the colon.\n"
    "\n"
    "ref: https://raw.githubusercontent.com/lowRISC/opentitan/"
    "8933d96c28e0e1054ea488d56940093109451c68/hw/dv/sv/"
    "push_pull_agent/push_pull_agent_pkg.sv",

    LowRISCFormatStyle(),

    "package push_pull_agent_pkg;\n"
    "endpackage: push_pull_agent_pkg\n",

    "package push_pull_agent_pkg;\n"
    "endpackage : push_pull_agent_pkg\n",
  },

  {
    "ref: https://github.com/lowRISC/opentitan/blob/"
    "8933d96c28e0e1054ea488d56940093109451c68/hw/dv/"
    "sv/dv_lib/dv_base_monitor.sv",

    LowRISCFormatStyle(),

    "class dv_base_monitor;\n"
    "virtual task watchdog_ok_to_end(uvm_phase run_phase);\n"
    "  fork\n"
    "    begin: isolation_fork\n"
    "    end: isolation_fork\n"
    "  join\n"
    "endtask\n"
    "endclass\n",

    "class dv_base_monitor;\n"
    "  virtual task watchdog_ok_to_end(uvm_phase run_phase);\n"
    "    fork\n"
    "      begin : isolation_fork\n"
    "      end : isolation_fork\n"
    "    join\n"
    "  endtask\n"
    "endclass\n",
  },

  {
    "Line wrapping\n"
    "\n"
    "guide: https://github.com/lowRISC/style-guides/blob/"
    "master/VerilogCodingStyle.md#line-wrapping"
  },

  {
    "Open syntax characters such as { or ( that end one line of"
    " a multi-line expression should be terminated with close "
    "characters (}, )) on their own line.\n"
    "\n"
    "ref: https://github.com/lowRISC/opentitan/blob/"
    "8933d96c28e0e1054ea488d56940093109451c68/hw/dv/sv/"
    "push_pull_agent/push_pull_item.sv",

    LowRISCFormatStyle(),

    "class push_pull_item;\n"
    "  virtual function string convert2string();\n"
    "    return {$sformatf(\"h_data = 0x%0x \", h_data),\n"
    "            $sformatf(\"d_data = 0x%0x \", d_data),\n"
    "            $sformatf(\"host_delay = 0x%0x \", host_delay),\n"
    "            $sformatf(\"device_delay = 0x%0x \", device_delay)};\n"
    "  endfunction\n"
    "endclass\n",

    "class push_pull_item;\n"
    "  virtual function string convert2string();\n"
    "    return {\n"
    "      $sformatf(\"h_data = 0x%0x \", h_data),\n"
    "      $sformatf(\"d_data = 0x%0x \", d_data),\n"
    "      $sformatf(\"host_delay = 0x%0x \", host_delay),\n"
    "      $sformatf(\"device_delay = 0x%0x \", device_delay)\n"
    "    };\n"
    "  endfunction\n"
    "endclass\n",
  },
  {
    "ref: https://github.com/lowRISC/opentitan/blob/"
    "8933d96c28e0e1054ea488d56940093109451c68/hw/dv/sv/"
    "uart_agent/uart_agent_cov.sv",

    LowRISCFormatStyle(),

    "class uart_agent_cov;\n"
    "covergroup uart_reset_cg;\n"
    "  cp_dir:        coverpoint dir;\n"
    "  cp_rst_pos:    coverpoint bit_position {\n"
    "    bins values[]  = {[0:NUM_UART_XFER_BITS_WO_PARITY]};\n"
    "  }\n"
    "  cross cp_dir, cp_rst_pos;\n"
    "endgroup\n"
    "endclass\n",

    "class uart_agent_cov;\n"
    "  covergroup uart_reset_cg;\n"
    "    cp_dir: coverpoint dir;\n"
    "    cp_rst_pos: coverpoint bit_position {bins values[] = {[0 : NUM_UART_XFER_BITS_WO_PARITY]};}\n"
    "    cross cp_dir, cp_rst_pos;\n"
    "  endgroup\n"
    "endclass\n",
  },

  {
    "",

    LowRISCFormatStyle(80),

    "class uart_agent_cov;\n"
    "covergroup uart_reset_cg;\n"
    "  cp_dir:        coverpoint dir;\n"
    "  cp_rst_pos:    coverpoint bit_position {\n"
    "    bins values[]  = {[0:NUM_UART_XFER_BITS_WO_PARITY]};\n"
    "  }\n"
    "  cross cp_dir, cp_rst_pos;\n"
    "endgroup\n"
    "endclass\n",

    "class uart_agent_cov;\n"
    "  covergroup uart_reset_cg;\n"
    "    cp_dir: coverpoint dir;\n"
    "    cp_rst_pos: coverpoint bit_position {\n"
    "      bins values[] = {[0 : NUM_UART_XFER_BITS_WO_PARITY]};\n"
    "    }\n"
    "    cross cp_dir, cp_rst_pos;\n"
    "  endgroup\n"
    "endclass\n"
  },

  {
    "",

    LowRISCFormatStyle(50),

    "class uart_agent_cov;\n"
    "covergroup uart_reset_cg;\n"
    "  cp_dir:        coverpoint dir;\n"
    "  cp_rst_pos:    coverpoint bit_position {\n"
    "    bins values[]  = {[0:NUM_UART_XFER_BITS_WO_PARITY]};\n"
    "  }\n"
    "  cross cp_dir, cp_rst_pos;\n"
    "endgroup\n"
    "endclass\n",

    "class uart_agent_cov;\n"
    "  covergroup uart_reset_cg;\n"
    "    cp_dir: coverpoint dir;\n"
    "    cp_rst_pos: coverpoint bit_position {\n"
    "      bins values[] = {\n"
    "        [0 : NUM_UART_XFER_BITS_WO_PARITY]\n"
    "      };\n"
    "    }\n"
    "    cross cp_dir, cp_rst_pos;\n"
    "  endgroup\n"
    "endclass\n"
  },

  {
    "ref: https://github.com/lowRISC/opentitan/blob/"
    "8933d96c28e0e1054ea488d56940093109451c68/hw/dv/sv/"
    "cip_lib/cip_base_env_cov.sv",

    LowRISCFormatStyle(),

    "covergroup intr_test_cg;"
    "  cross cp_intr, cp_intr_test, cp_intr_en, cp_intr_state {\n"
    "    illegal_bins test_1_state_0 = binsof(cp_intr_test) intersect {1} &&\n"
    "                                  binsof(cp_intr_state) intersect {0};\n"
    "  }\n"
    "endgroup\n",

    "covergroup intr_test_cg;\n"
    "  cross cp_intr, cp_intr_test, cp_intr_en, cp_intr_state{\n"
    "    illegal_bins test_1_state_0 = binsof (cp_intr_test) intersect {\n"
    "      1\n"
    "    } && binsof (cp_intr_state) intersect {\n"
    "      0\n"
    "    };\n"
    "  }\n"
    "endgroup\n",

    "covergroup intr_test_cg;"
    "  cross cp_intr, cp_intr_test, cp_intr_en, cp_intr_state {\n"
    "    illegal_bins test_1_state_0 = binsof(cp_intr_test) intersect {1} &&\n"
    "                                  binsof(cp_intr_state) intersect {0};\n"
    "  }\n"
    "endgroup\n"
  },

  {
    "ref: https://github.com/lowRISC/opentitan/blob/"
    "8933d96c28e0e1054ea488d56940093109451c68/hw/dv/sv/"
    "test_vectors/test_vectors_pkg.sv",

    LowRISCFormatStyle(),

    "string sha_file_list[]        = {\"vectors/sha/sha256/SHA256ShortMsg.rsp\",\n"
    "                                 \"vectors/sha/sha256/SHA256LongMsg.rsp\"\n"
    "                                };\n",

    "string sha_file_list[] = {\n"
    "  \"vectors/sha/sha256/SHA256ShortMsg.rsp\", \"vectors/sha/sha256/SHA256LongMsg.rsp\"\n"
    "};\n"
  },
  {
    "",

    LowRISCFormatStyle(80),

    "string sha_file_list[]        = {\"vectors/sha/sha256/SHA256ShortMsg.rsp\",\n"
    "                                 \"vectors/sha/sha256/SHA256LongMsg.rsp\"\n"
    "                                };\n",

    "string sha_file_list[] = {\n"
    "  \"vectors/sha/sha256/SHA256ShortMsg.rsp\",\n"
    "  \"vectors/sha/sha256/SHA256LongMsg.rsp\"\n"
    "};\n"
  },

  {
    "ref: https://github.com/lowRISC/opentitan/blob/"
    "8933d96c28e0e1054ea488d56940093109451c68/"
    "hw/dv/sv/tl_agent/seq_lib/tl_host_seq.sv",

    LowRISCFormatStyle(),

    "class tl_host_seq;\n"
    "  virtual function void randomize_req(REQ req, int idx);\n"
    "    if (!(req.randomize() with {\n"
    "        a_valid_delay inside {[min_req_delay:max_req_delay]};})) begin\n"
    "      `uvm_fatal(`gfn, \"Cannot randomize req\")\n"
    "    end\n"
    "  endfunction\n"
    "endclass\n",

    "class tl_host_seq;\n"
    "  virtual function void randomize_req(REQ req, int idx);\n"
    "    if (!(req.randomize() with {a_valid_delay inside {[min_req_delay : max_req_delay]};})) begin\n"
    "      `uvm_fatal(`gfn, \"Cannot randomize req\")\n"
    "    end\n"
    "  endfunction\n"
    "endclass\n"
  },

  {
    "",

    LowRISCFormatStyle(80),

    "class tl_host_seq;\n"
    "  virtual function void randomize_req(REQ req, int idx);\n"
    "    if (!(req.randomize() with {\n"
    "        a_valid_delay inside {[min_req_delay:max_req_delay]};})) begin\n"
    "      `uvm_fatal(`gfn, \"Cannot randomize req\")\n"
    "    end\n"
    "  endfunction\n"
    "endclass\n",

    "class tl_host_seq;\n"
    "  virtual function void randomize_req(REQ req, int idx);\n"
    "    if (!(req.randomize() with {\n"
    "          a_valid_delay inside {[min_req_delay : max_req_delay]};\n"
    "        })) begin\n"
    "      `uvm_fatal(`gfn, \"Cannot randomize req\")\n"
    "    end\n"
    "  endfunction\n"
    "endclass\n",

    "class tl_host_seq;\n"
    "  virtual function void randomize_req(REQ req, int idx);\n"
    "    if (!(req.randomize() with {\n"
    "          a_valid_delay inside {[min_req_delay : max_req_delay]};})) begin\n"
    "      `uvm_fatal(`gfn, \"Cannot randomize req\")\n"
    "    end\n"
    "  endfunction\n"
    "endclass\n"
  },

  {
    "Nested function calls",
  },

  {
    "",

    LowRISCFormatStyle(),

    "`uvm_info(`gtn, $sformatf(\"Verifying reset value of register %0s\",\n"
    "                          test_csrs[i].get_full_name()), UVM_MEDIUM)\n",

    "`uvm_info(`gtn, $sformatf(\"Verifying reset value of register %0s\", test_csrs[i].get_full_name()),\n"
    "          UVM_MEDIUM)\n"
  },
  {
    "",

    LowRISCFormatStyle(80),

    "`uvm_info(`gtn, $sformatf(\"Verifying reset value of register %0s\",\n"
    "                          test_csrs[i].get_full_name()), UVM_MEDIUM)\n",

    "`uvm_info(`gtn, $sformatf(\n"
    "          \"Verifying reset value of register %0s\", test_csrs[i].get_full_name()\n"
    "          ), UVM_MEDIUM)\n",

    "`uvm_info(`gtn, $sformatf(\"Verifying reset value of register %0s\",\n"
    "                          test_csrs[i].get_full_name()), UVM_MEDIUM)\n"
  },

  {
    "Alignment"
  },

  {
    "ref:https://github.com/lowRISC/opentitan/blob/"
    "8933d96c28e0e1054ea488d56940093109451c68/"
    "hw/dv/sv/csr_utils/csr_seq_lib.sv",

    LowRISCFormatStyle(),

    "class csr_bit_bash_seq extends csr_base_seq;\n"
    "  task bash_kth_bit;\n"
    "    repeat (2) begin\n"
    "      csr_rd_check(.ptr           (rg),\n"
    "                   .blocking      (0),\n"
    "                   .compare       (!external_checker),\n"
    "                   .compare_vs_ral(1'b1),\n"
    "                   .compare_mask  (~mask),\n"
    "                   .err_msg       (err_msg));\n"
    "    end\n"
    "  endtask: bash_kth_bit\n"
    "endclass\n",

    "class csr_bit_bash_seq extends csr_base_seq;\n"
    "  task bash_kth_bit;\n"
    "    repeat (2) begin\n"
    "      csr_rd_check(.ptr(rg), .blocking(0), .compare(!external_checker), .compare_vs_ral(1'b1),\n"
    "                   .compare_mask(~mask), .err_msg(err_msg));\n"
    "    end\n"
    "  endtask : bash_kth_bit\n"
    "endclass\n",

    "class csr_bit_bash_seq extends csr_base_seq;\n"
    "  task bash_kth_bit;\n"
    "    repeat (2) begin\n"
    "      csr_rd_check(.ptr           (rg),\n"
    "                   .blocking      (0),\n"
    "                   .compare       (!external_checker),\n"
    "                   .compare_vs_ral(1'b1),\n"
    "                   .compare_mask  (~mask),\n"
    "                   .err_msg       (err_msg));\n"
    "    end\n"
    "  endtask: bash_kth_bit\n"
    "endclass\n",
  },

  {
    "ref: https://github.com/lowRISC/opentitan/blob/"
    "8933d96c28e0e1054ea488d56940093109451c68/"
    "hw/dv/sv/kmac_app_agent/seq_lib/kmac_app_device_seq.sv",

    LowRISCFormatStyle(),

    "class kmac_app_device_seq extends kmac_app_base_seq;\n"
    "  virtual function void randomize_item(REQ item);\n"
    "    `DV_CHECK_RANDOMIZE_WITH_FATAL(item,\n"
    "      if (cfg.zero_delays) {\n"
    "        rsp_delay == 0;\n"
    "      } else {\n"
    "        rsp_delay inside {[cfg.rsp_delay_min : cfg.rsp_delay_max]};\n"
    "      }\n"
    "      is_kmac_rsp_err dist {1 :/ cfg.error_rsp_pct,\n"
    "                            0 :/ 100 - cfg.error_rsp_pct};\n"
    "    )\n"
    "  endfunction\n"
    "endclass\n",

    "class kmac_app_device_seq extends kmac_app_base_seq;\n"
    "  virtual function void randomize_item(REQ item);\n"
    "    `DV_CHECK_RANDOMIZE_WITH_FATAL(item,\n"
    "                                   if (cfg.zero_delays) {\n"
    "        rsp_delay == 0;\n"
    "      } else {\n"
    "        rsp_delay inside {[cfg.rsp_delay_min : cfg.rsp_delay_max]};\n"
    "      }\n"
    "      is_kmac_rsp_err dist {1 :/ cfg.error_rsp_pct,\n"
    "                            0 :/ 100 - cfg.error_rsp_pct};)\n"
    "  endfunction\n"
    "endclass\n",

    "class kmac_app_device_seq extends kmac_app_base_seq;\n"
    "  virtual function void randomize_item(REQ item);\n"
    "    `DV_CHECK_RANDOMIZE_WITH_FATAL(\n"
    "        item,\n"
    "        if (cfg.zero_delays) {\n"
    "          rsp_delay == 0;\n"
    "        } else {\n"
    "          rsp_delay inside {[cfg.rsp_delay_min : cfg.rsp_delay_max]};\n"
    "        }\n"
    "        is_kmac_rsp_err dist {1 :/ cfg.error_rsp_pct,\n"
    "                              0 :/ 100 - cfg.error_rsp_pct};)\n"
    "  endfunction\n"
    "endclass\n",
  },
};

template<typename T, std::size_t N>
constexpr std::size_t arraySize(T (&)[N]) noexcept {
  return N;
}

std::pair<const ComplianceTestCase*, size_t>
GetLowRISCComplianceTestCases() {
  return std::make_pair<const ComplianceTestCase*, size_t>(
      kComplianceTestCases, arraySize(kComplianceTestCases));
}

}  // namespace tests
}  // namespace formatter
}  // namespace verilog
