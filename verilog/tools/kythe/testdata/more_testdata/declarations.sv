// Copyright 2020 Google LLC.
// SPDX-License-Identifier: Apache-2.0

//- _FileNode.node/kind file
//- @my_pkg defines/binding _
package my_pkg;

//- @my_class defines/binding _
  class my_class;
  endclass

//- @my_integer defines/binding _
  integer my_integer = 10;

//- @my_enum1 defines/binding _
//- @my_enum2 defines/binding _
//- @my_var1 defines/binding _
//- @my_var2 defines/binding _
  enum {my_enum1, my_enum2} my_var1, my_var2;

//- @my_function defines/binding _
//- @my_arg1 defines/binding _
  function void my_function(int my_arg1);
  endfunction

//- @my_task defines/binding _
//- @my_arg2 defines/binding _
  task my_task(int my_arg2);
  endtask

endpackage

//- @my_module defines/binding _
module my_module; endmodule

//- @my_program defines/binding _
program my_program; endprogram

//- @my_interface defines/binding _
interface my_interface; endinterface


