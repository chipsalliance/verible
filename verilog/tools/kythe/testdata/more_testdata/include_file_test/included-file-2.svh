//- @"\"included-file-1.svh\"" ref/includes vname(_,_, _, "./verilog/tools/kythe/testdata/more_testdata/include_file_test/included-file-1.svh", _)
`include "included-file-1.svh"

//- @MyClassIncludedFile2 defines/binding MyClassIncludedFile2
class MyClassIncludedFile2;
    //- @var5 defines/binding Var5
    static int var5;

    //- @my_fun3 defines/binding _
    function int my_fun3();
        //- @var6 ref Var6
        //- @MyClassIncludedFile1 ref MyClassIncludedFile1
        return MyClassIncludedFile1::var6;
    endfunction
endclass
