// Copyright 2020 Google LLC.
// SPDX-License-Identifier: Apache-2.0

//- @PRINT_STRING defines/binding _
`define PRINT_STRING(str1) $display("%s\n", str1)

//- @PRINT_3_STRINGS defines/binding Print3Strings
`define PRINT_3_STRINGS(str1, str2, str3) \
    `PRINT_STRING(str1); \
    `PRINT_STRING(str2); \
    `PRINT_STRING(str3);

//- @TEN defines/binding Ten
`define TEN 10

package my_pkg;
  //- @my_function defines/binding MyFunction
  function automatic my_function(int arg1, int arg2);
    return arg1 + arg2;
  endfunction
endpackage

//- @macro defines/binding _
module macro;
  initial begin
    //- @arg2 defines/binding Arg2
    automatic integer arg2 = 2;

    //- @ii defines/binding _
    //- @my_function ref/call MyFunction
    //- @"TEN" ref/expands Ten
    //- @arg2 ref Arg2
    automatic integer ii = my_pkg::my_function(`TEN, arg2);

    //- @"PRINT_3_STRINGS" ref/expands Print3Strings
    `PRINT_3_STRINGS("Grand", "Tour", "S4");
  end
endmodule
