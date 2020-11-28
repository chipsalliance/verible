// Copyright 2020 Google LLC.
// SPDX-License-Identifier: Apache-2.0

//- @"\"included-file-2.svh\"" ref/includes IncludedFile2Svh
//- IncludedFile2Svh.node/kind file
`include "included-file-2.svh"

//- @my_module2 defines/binding _
module my_module2();

    initial begin
        //- @MyClassIncludedFile1 ref MyClassIncludedFile1
        //- @instance1 defines/binding Instance1
        static MyClassIncludedFile1 instance1 = new();

        //- @MyClassIncludedFile2 ref MyClassIncludedFile2
        //- @MyClassIncludedFile1 ref MyClassIncludedFile1
        //- @var5 ref Var5
        //- @var6 ref Var6
        MyClassIncludedFile2::var5 = MyClassIncludedFile1::var6;

        //- @instance1 ref Instance1
        //- @my_fun1 ref MyFun1
        //- @my_fun1 ref/call MyFun1
        $display(instance1.my_fun1());

        //- @MyClassIncludedFile1 ref MyClassIncludedFile1
        //- @my_fun1 ref MyFun1
        //- @my_fun1 ref/call MyFun1
        $display(MyClassIncludedFile1.my_fun1());
    end

endmodule
