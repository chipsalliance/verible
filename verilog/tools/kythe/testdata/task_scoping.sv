// Copyright 2020 Google LLC.
// SPDX-License-Identifier: Apache-2.0

package my_pkg;
  //- @my_task defines/binding MyTask
  //- MyTask.node/kind function
  task automatic my_task(input int a, b, output int c);
    c = a + b;
  endtask
endpackage

class task_class;
  //- @my_task defines/binding ClassTask
  //- ClassTask.node/kind function
  task my_task(input int a, b, output int c);
    c = a + b;
  endtask
endclass

module task_scoping;
  initial begin
    int result;

    //- @tc_instance defines/binding TcInstance
    automatic task_class tc_instance = new();
    //- @tc_instance ref TcInstance
    //- @my_task ref/call ClassTask
    tc_instance.my_task(1, 2, result);

    //- @my_task ref/call MyTask
    my_pkg::my_task(1, 2, result);
  end
endmodule
