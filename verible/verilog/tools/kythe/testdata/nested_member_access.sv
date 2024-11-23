//- _FileNode.node/kind file

//- @nested_class2 defines/binding NestedClass2
//- NestedClass2.node/kind record
class nested_class2;
  //- @var1 defines/binding Var1Def
  //- Var1Def.node/kind variable
  //- Var1Def.complete definition
  //- Var1Def childof NestedClass2
  int var1 = 1;

  //- @nested_function defines/binding NestedFunctionNestedClass2
  //- NestedFunctionNestedClass2.node/kind function
  //- NestedFunctionNestedClass2.complete definition
  //- NestedFunctionNestedClass2 childof NestedClass2
  function int nested_function();
    return 1;
  endfunction

endclass

//- @nested_class1 defines/binding NestedClass1
//- NestedClass1.node/kind record
//- NestedClass1.complete definition
class nested_class1;

  //- @nested_class2 ref NestedClass2
  //- @handle2 defines/binding Handle2
  //- Handle2.node/kind variable
  //- Handle2.complete definition
  //- Handle2 childof NestedClass1
  static nested_class2 handle2 = new();

endclass

//- @nested_class0 defines/binding NestedClass0
//- NestedClass0.node/kind record
//- NestedClass0.complete definition
class nested_class0;

  //- @nested_class1 ref NestedClass1
  //- @handle1 defines/binding Handle1
  //- Handle1.node/kind variable
  //- Handle1.complete definition
  //- Handle1 childof NestedClass0
  static nested_class1 handle1 = new();

  //- @inner_class defines/binding InnerClass
  //- InnerClass.node/kind record
  //- InnerClass.complete definition
  //- InnerClass childof NestedClass0
  class inner_class;
    //- @nested_function defines/binding NestedFunction
    //- NestedFunction.node/kind function
    //- NestedFunction.complete definition
    //- NestedFunction childof InnerClass
    function int nested_function();
      return 1;
    endfunction
  endclass
endclass

//- @my_module defines/binding MyModule
//- MyModule.node/kind record
//- MyModule.subkind module
//- MyModule.complete definition
module my_module ();
  //- @nested_class2 ref NestedClass2
  //- @var1 ref Var1Def
  initial $display(nested_class2::var1);

  //- @nested_class1 ref NestedClass1
  //- @handle2 ref Handle2
  //- @var1 ref Var1Def
  initial $display(nested_class1::handle2::var1);

  //- @nested_class0 ref NestedClass0
  //- @handle1 ref Handle1
  //- @handle2 ref Handle2
  //- @var1 ref Var1Def
  initial $display(nested_class0::handle1::handle2::var1);

  //- @nested_class0 ref NestedClass0
  //- @inner_class ref InnerClass
  //- @nested_function ref NestedFunction
  //- @nested_function ref/call NestedFunction
  initial $display(nested_class0::inner_class::nested_function());

  //- @nested_class0 ref NestedClass0
  //- @handle1 ref Handle1
  //- @handle2 ref Handle2
  //- @nested_function ref NestedFunctionNestedClass2
  //- @nested_function ref/call NestedFunctionNestedClass2
  initial $display(nested_class0::handle1::handle2::nested_function());

  //- @nested_class0 ref NestedClass0
  //- @handle1 ref Handle1
  //- @handle2 ref Handle2
  //- @nested_function ref NestedFunctionNestedClass2
  //- @nested_function ref/call NestedFunctionNestedClass2
  initial $display(nested_class0::handle1::handle2.nested_function());

  //- @nested_class2 ref NestedClass2
  //- @nested_function ref NestedFunctionNestedClass2
  //- @nested_function ref/call NestedFunctionNestedClass2
  initial $display(nested_class2::nested_function());

  //- @my_module ref MyModule
endmodule : my_module
