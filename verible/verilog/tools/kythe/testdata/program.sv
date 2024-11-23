//- @mem_intf defines/binding MemIntf
interface mem_intf;
    //- @clk defines/binding CLK
    logic      clk;
endinterface
   
//- @testcase defines/binding TestCase
//- TestCase.node/kind record
//- TestCase.subkind program
program testcase (
    //- @intf defines/binding Intf
    interface  intf
    );
    //- @#1env defines/binding Env
    environment env;
    initial begin
        //- @env ref Env
        //- @intf ref Intf
        env = new(intf);
    end
//- @testcase ref TestCase
endprogram: testcase
   
//- @tbench_top defines/binding _
module tbench_top;
    //- @mem_intf ref MemIntf
    //- @#1intf defines/binding Intf1
    mem_intf intf();

    //- @memory ref Memory
    //- @dut defines/binding _
    memory dut(
        //- @#0clk ref Clk2
        //- @intf ref Intf1
        //- @#1clk ref CLK
        .clk(intf.clk),
        );

    //- @testcase ref TestCase
    //- @#1test defines/binding _
    //- @intf ref Intf1
    testcase test(intf);
endmodule

//- @memory defines/binding Memory
module memory ( 
    //- @clk ref Clk2
        clk
    );
    //- @clk defines/binding Clk2
    input        clk;
endmodule
