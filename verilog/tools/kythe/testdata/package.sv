//- _FileNode.node/kind file

//- @my_pkg1 defines/binding MyPkg1
//- MyPkg1.node/kind package
package my_pkg1;
  //- @w1 defines/binding W1Def
  //- W1Def.node/kind variable
  //- W1Def.complete definition
  //- W1Def childof MyPkg1
  wire w1;

  //- @my_class1 defines/binding MyClass1
  //- MyClass1.node/kind record
  //- MyClass1.complete definition
  //- MyClass1 childof MyPkg1
  class my_class1;

    //- @my_function1 defines/binding MyFunction1
    //- MyFunction1.node/kind function
    //- MyFunction1.complete definition
    //- MyFunction1 childof MyClass1
    function int my_function1();
      return 1;
    endfunction
  endclass

  //- @my_function2 defines/binding MyFunction2
  //- MyFunction2.node/kind function
  //- MyFunction2.complete definition
  //- MyFunction2 childof MyPkg1
  function int my_function2();
    return 1;
  endfunction

  //- @my_pkg1 ref MyPkg1
endpackage : my_pkg1

//- @my_pkg2 defines/binding MyPkg2
//- MyPkg2.node/kind package
package my_pkg2;

  //- @w2 defines/binding W2Def
  //- W2Def.node/kind variable
  //- W2Def.complete definition
  //- W2Def childof MyPkg2
  wire w2;

  //- @my_class_in_pkg2 defines/binding MyClassPkg2
  //- MyClassPkg2.node/kind record
  //- MyClassPkg2.complete definition
  //- MyClassPkg2 childof MyPkg2
  class my_class_in_pkg2;
  endclass

endpackage

//- @my_module defines/binding MyModule
//- MyModule.node/kind record
//- MyModule.subkind module
//- MyModule.complete definition
module my_module (
    input x
);
  //- @#0my_pkg1 ref/imports MyPkg1
  //- @#1my_pkg1 ref/imports MyPkg1
  //- @my_class1 ref MyClass1
  //- @my_function2 ref MyFunction2
  import my_pkg1::my_class1, my_pkg1::my_function2;

  //- @my_pkg2 ref/imports MyPkg2
  //- @my_class_in_pkg2 ref MyClassPkg2
  import my_pkg2::my_class_in_pkg2;

  //- @my_function2 ref MyFunction2
  //- @my_function2 ref/call MyFunction2
  initial $display(my_function2());
endmodule

//- @second_module defines/binding MyModule1
//- MyModule1.node/kind record
//- MyModule1.subkind module
//- MyModule1.complete definition
module second_module;
  //- @my_pkg1 ref/imports MyPkg1
  import my_pkg1::*;
  //- @my_pkg3 ref/imports MyPkg3
  import my_pkg3::*;

  //- @my_module ref MyModule
  //- @instance1 defines/binding MyModuleInstance
  //- MyModuleInstance.node/kind variable
  //- MyModuleInstance.complete definition
  //- MyModuleInstance childof MyModule1
  //- @w1 ref W1Def
  my_module instance1 (w1);

  //- @my_function3 ref MyFunction3
  //- @my_function3 ref/call MyFunction3
  initial $display(my_function3());
endmodule

//- @my_pkg3 defines/binding MyPkg3
//- MyPkg3.node/kind package
package my_pkg3;

  //- @my_class_pkg3 defines/binding MyClassPkg3
  //- MyClassPkg3.node/kind record
  //- MyClassPkg3.complete definition
  //- MyClassPkg3 childof MyPkg3
  class my_class_pkg3;
  endclass

  //- @my_function3 defines/binding MyFunction3
  //- MyFunction3.node/kind function
  //- MyFunction3.complete definition
  //- MyFunction3 childof MyPkg3
  function int my_function3();
    return 6;
  endfunction
endpackage
