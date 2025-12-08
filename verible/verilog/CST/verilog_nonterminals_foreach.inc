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

// This files is used to generate repetitive code for every enumeration
// value in "verilog/CST/verilog_nonterminals.h".
// To use this #define a CONSIDER() macro before including this file,
// and #undef it after including this file.

#ifndef CONSIDER
#error \
    "Define the CONSIDER macro before including this file " \
       "(and undefine it after)."
#endif  // CONSIDER

// CONSIDER(...) cases are auto-generated from verilog_nonterminals.h' enum.
#include "verible/verilog/CST/verilog_nonterminals_foreach-gen.inc"
