// Copyright 2020 Google LLC.
// SPDX-License-Identifier: Apache-2.0

//- _FileNode.node/kind file
//- @package_with_include defines/binding MyPkg
package package_with_include;
  //- @"\"package_with_include_class.svh\"" ref/includes PackageWithIncludeClassSvh
  //- PackageWithIncludeClassSvh.node/kind file
  `include "package_with_include_class.svh"
endpackage

module my_module;
  import package_with_include::*;
  initial begin
    //- @my_class1 ref MyClass1
    static my_class1 handle1 = new();

    //- @my_var ref MyVar
    handle1::my_var = 10;

    //- @my_function ref MyFunction
    $display(handle1.my_function());

    //- @my_task ref/call MyTask
    handle1.my_task(2);

    //- @my_class2 ref MyClass2
    //- @my_static_var ref MyStaticVar
    $display(my_class2::my_static_var);

  end
endmodule
