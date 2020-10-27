//- _FileNode.node/kind file

//- @my_pkg defines/binding MyPkg
//- MyPkg.node/kind package
package my_pkg;

  //- @my_class1 defines/binding MyClass1
  //- MyClass1.node/kind record
  //- MyClass1.complete definition
  //- MyClass1 childof MyPkg
  class my_class1;

    //- @my_var defines/binding MyVar
    int my_var;

    //- @my_static_var defines/binding MyStaticVar
    static int my_static_var;

    //- @my_function defines/binding MyFunction
    //- MyFunction.node/kind function
    //- MyFunction.complete definition
    //- MyFunction childof MyClass1
    virtual function int my_function();
      //- @my_var ref MyVar
      return my_var;
    endfunction

    //- @my_task defines/binding MyTask
    //- MyTask.node/kind function
    //- MyTask.complete definition
    //- MyTask childof MyClass1
    //- @my_arg2 defines/binding MyArg2
    task my_task(int my_arg2);
      //- @my_arg2 ref MyArg2
      $display(my_arg2);
    endtask

    //- @nested_class defines/binding NestedClass
    //- NestedClass.node/kind record
    //- NestedClass.complete definition
    //- NestedClass childof MyClass1
    class nested_class;
      //- @nested_function defines/binding NestedFunction
      //- NestedFunction.node/kind function
      //- NestedFunction.complete definition
      //- NestedFunction childof NestedClass
      function int nested_function();
        return 1;
      endfunction
    endclass

    //- @my_class1 ref MyClass1
  endclass : my_class1

  //- @my_pkg ref MyPkg
endpackage : my_pkg

//- @my_module defines/binding MyModule
//- MyModule.node/kind record
//- MyModule.subkind module
//- MyModule.complete definition
module my_module;
  //- @my_pkg ref/imports MyPkg
  import my_pkg::*;
  initial begin
    //- @my_class1 ref MyClass1
    //- @handle1 defines/binding Handle1
    //- Handle1.node/kind variable
    //- Handle1.complete definition
    //- Handle1 childof MyModule
    static my_class1 handle1 = new();

    //- @my_class1 ref MyClass1
    //- @handle3 defines/binding Handle3
    //- Handle3.node/kind variable
    //- Handle3.complete definition
    //- Handle3 childof MyModule
    //- @handle4 defines/binding Handle4
    //- Handle4.node/kind variable
    //- Handle4.complete definition
    //- Handle4 childof MyModule
    my_class1 handle3 = new(), handle4 = new();

    //- @handle1 ref Handle1
    //- @my_function ref MyFunction
    //- @my_function ref/call MyFunction
    $display(handle1.my_function());

    //- @handle1 ref Handle1
    //- @my_task ref/call MyTask
    handle1.my_task(2);


    //- @my_class1 ref MyClass1
    //- @my_static_var ref MyStaticVar
    $display(my_class1::my_static_var);
  end

  //- @my_class3 defines/binding MyClass3
  //- MyClass3.node/kind record
  //- MyClass3.complete definition
  //- MyClass3 childof MyModule
  class my_class3;
    //- @my_function3 defines/binding MyFunction3
    //- MyFunction3.node/kind function
    //- MyFunction3.complete definition
    //- MyFunction3 childof MyClass3
    function int my_function3();
      return 1;
    endfunction
    //- @my_class3 ref MyClass3
  endclass : my_class3
endmodule
