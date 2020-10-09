//- @"\"included-file-2.svh\"" ref/includes vname(_,_, _, "./verilog/tools/kythe/testdata/more_testdata/include_file_test/included-file-2.svh", _)
`include "included-file-2.svh"

//- @my_module defines/binding _
module my_module();

    initial begin
        //- @MyClassIncludedFile1 ref MyClassIncludedFile1
        //- @instance2 defines/binding Instance2
        static MyClassIncludedFile1 instance2 = new();

        //- @MyClassIncludedFile2 ref MyClassIncludedFile2
        //- @MyClassIncludedFile1 ref MyClassIncludedFile1
        //- @var5 ref Var5
        //- @var6 ref Var6
        MyClassIncludedFile2::var5 = MyClassIncludedFile1::var6;
        
        //- @instance2 ref Instance2
        //- @my_fun1 ref MyFun1
        //- @my_fun1 ref/call MyFun1
        $display(instance2.my_fun1());

        //- @MyClassIncludedFile1 ref MyClassIncludedFile1
        //- @my_fun1 ref MyFun1
        //- @my_fun1 ref/call MyFun1
        $display(MyClassIncludedFile1.my_fun1());
    end

endmodule
