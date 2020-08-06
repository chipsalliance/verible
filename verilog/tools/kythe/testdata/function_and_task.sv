//- _FileNode.node/kind file
package my_pkg;

  //- @my_function defines/binding MyFunction
  //- MyFunction.node/kind function
  //- @my_arg1 defines/binding MyArg1
  function int my_function(int my_arg1);
    //- @my_arg1 ref MyArg1
    return my_arg1;
  endfunction

  //- @my_task defines/binding MyTask
  //- MyTask.node/kind function
  //- @my_arg2 defines/binding MyArg2
  task my_task(int my_arg2);
    //- @my_arg2 ref MyArg2
    $display(my_arg2);
  endtask

endpackage


module my_module;
  import my_pkg::*;
  initial begin
    //- @ii defines/binding VarI
    //- @my_function ref MyFunction
    //- @"my_function(2)" ref/call MyFunction
    automatic integer ii = my_function(2);
    //- @ii ref VarI
    //- @my_task ref/call MyTask
    my_task(ii);
  end
endmodule
