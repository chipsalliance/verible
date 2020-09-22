//- _FileNode.node/kind file

//- @my_function defines/binding MyFunction
//- MyFunction.node/kind function
//- MyFunction.complete definition
//- @my_arg1 defines/binding MyArg1
//- MyArg1.node/kind variable
//- MyArg1.complete definition
//- MyArg1 childof MyFunction
//- @my_arg2 defines/binding MyArg2
//- MyArg2.node/kind variable
//- MyArg2.complete definition
//- MyArg2 childof MyFunction
function int my_function(int my_arg1, int my_arg2);
  //- @my_arg1 ref MyArg1
  //- @my_arg2 ref MyArg2
  return my_arg1 + my_arg2;
endfunction

//- @my_task defines/binding MyTask
//- MyFunction.node/kind function
//- MyFunction.complete definition
//- @my_arg3 defines/binding MyArg3
//- MyArg3.node/kind variable
//- MyArg3.complete definition
//- MyArg3 childof MyTask
//- @my_arg4 defines/binding MyArg4
//- MyArg4.node/kind variable
//- MyArg4.complete definition
//- MyArg4 childof MyTask
task my_task(int my_arg3, int my_arg4);
  //- @my_arg3 ref MyArg3
  //- @my_arg4 ref MyArg4
  $display(my_arg3 + my_arg4);
endtask : my_task

module my_module;
  initial begin
    //- @my_function ref MyFunction
    //- @my_function ref/call MyFunction
    my_function(2, 5);
    //- @my_task ref MyTask
    //- @my_task ref/call MyTask
    my_task(1, 4);
  end
endmodule
