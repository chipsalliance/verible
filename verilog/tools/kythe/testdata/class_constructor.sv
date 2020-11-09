//- _FileNode.node/kind file

//- @my_class3 defines/binding MyClass3
//- MyClass3.node/kind record
//- MyClass3.complete definition
class my_class3;

  //- @var1 defines/binding Var1Def
  //- Var1Def.node/kind variable
  //- Var1Def.complete definition
  //- Var1Def childof MyClass3
  int var1 = 1;

  //- @new defines/binding New
  //- New.node/kind function
  //- New.subkind constructor
  //- New childof MyClass3
  //- @my_arg1 defines/binding MyArg1
  //- MyArg1.node/kind variable
  //- MyArg1.complete definition
  //- MyArg1 childof New
  //- @my_arg2 defines/binding MyArg2
  //- MyArg2.node/kind variable
  //- MyArg2.complete definition
  //- MyArg2 childof New
  function new(int my_arg1, int my_arg2);
    // //- @var1 ref Var1Def
    // //- @my_arg1 reg MyArg1
    // //- @my_arg2 reg MyArg2
    var1 = my_arg1 + my_arg2;
  endfunction
  //- @my_class3 ref MyClass3
endclass : my_class3

//- @my_function defines/binding MyFunctionOut
//- MyFunctionOut.node/kind function
//- MyFunctionOut.complete definition
//- @my_arg3 defines/binding MyArg3
//- MyArg3.node/kind variable
//- MyArg3.complete definition
//- MyArg3 childof MyFunctionOut
//- @my_arg4 defines/binding MyArg4
//- MyArg4.node/kind variable
//- MyArg4.complete definition
//- MyArg4 childof MyFunctionOut
function int my_function(int my_arg3, int my_arg4);
  //- @my_arg3 ref MyArg3
  //- @my_arg4 ref MyArg4
  return my_arg3 + my_arg4;
endfunction

//- @my_module defines/binding MyModule
//- MyModule.node/kind record
//- MyModule.subkind module
//- MyModule.complete definition
module my_module;

  //- @num defines/binding Num
  //- Num.node/kind variable
  //- Num.complete definition
  //- Num childof MyModule
  int num = 1;

  //- @my_class3 ref MyClass3
  //- @handle1 defines/binding Handle1
  //- Handle1.node/kind variable
  //- Handle1.complete definition
  //- Handle1 childof MyModule
  //- @#0num ref Num
  //- @#1num ref Num
  //- @#2num ref Num
  //- @my_function ref MyFunctionOut
  //- @my_function ref/call MyFunctionOut
  static my_class3 handle1 = new(num, my_function(num, num));

endmodule
