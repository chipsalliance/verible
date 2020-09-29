// Copyright 2020 Google LLC.
// SPDX-License-Identifier: Apache-2.0

//- @top_pkg defines/binding TopPkg
package top_pkg;
  //- @my_function defines/binding PkgFunction
  //- PkgFunction.node/kind function
  //- @my_arg1 defines/binding PkgArg
  function automatic int my_function(int my_arg1);
    //- @my_arg1 ref PkgArg
    return my_arg1;
  endfunction

  //- @your_function defines/binding _
  function automatic int your_function(int your_arg1);
    //- @my_function ref PkgFunction
    //- @my_function ref/call PkgFunction
    //- @top_pkg ref TopPkg
    return top_pkg::my_function(your_arg1);
  endfunction
endpackage

class nested_class;
  //- @my_function defines/binding ClassFunction
  //- ClassFunction.node/kind function
  //- @my_arg1 defines/binding ClassArg
  function int my_function(int my_arg1);
    //- @my_arg1 ref ClassArg
    return my_arg1;
  endfunction
endclass

module function_scoping;
  import top_pkg::*;

  initial begin
    //- @my_function ref PkgFunction
    //- @my_function ref/call PkgFunction
    automatic int i = my_function(2);

    //- @nc_instance defines/binding ClassInstance
    automatic nested_class nc_instance = new();

    //- @nc_instance ref ClassInstance
    //- @my_function ref ClassFunction
    //- @my_function ref/call ClassFunction
    automatic int j = nc_instance.my_function(2);
  end
endmodule
