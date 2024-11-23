// Copyright 2020 Google LLC.
// SPDX-License-Identifier: Apache-2.0

//- _FileNode.node/kind file
//- @my_class1 defines/binding MyClass1
//- MyClass1 childof MyPkg
class my_class1;
  //- @my_var defines/binding MyVar
  int my_var;

  //- @my_function defines/binding MyFunction
  virtual function int my_function();
    return my_var;
  endfunction

  //- @my_task defines/binding MyTask
  task my_task(int my_arg2);
    $display(my_arg2);
  endtask

endclass

//- @my_class2 defines/binding MyClass2
//- MyClass2 childof MyPkg
class my_class2;

  //- @my_static_var defines/binding MyStaticVar
  static int my_static_var;

  //- @my_function defines/binding _
  function int my_function();
    //- @my_static_var ref MyStaticVar
    return my_static_var;
  endfunction
endclass
