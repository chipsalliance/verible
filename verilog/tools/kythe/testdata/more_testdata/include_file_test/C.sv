//- @"\"A.svh\"" ref/includes vname(_,_, _, "./verilog/tools/kythe/testdata/more_testdata/include_file_test/A.svh", _)
`include "A.svh"
//- @"\"B.svh\"" ref/includes vname(_,_, _, "./verilog/tools/kythe/testdata/more_testdata/include_file_test/B.svh", _)
`include "B.svh"

//- @C defines/binding _
//- @A ref A
//- @B ref B
localparam C = A + B;
