//- @my_module defines/binding MyModule
module my_module#(
    //- @ADDR_WIDTH defines/binding ADDR_WIDTH
    parameter ADDR_WIDTH=8
    )
    //-@in1 defines/binding _
    (input in1);

    //- @pkg1 ref/imports Pkg1
    import pkg1::*;

    //- @my_class ref MyClass
    //- @instance2 defines/binding Instance2
    my_class instance2 = new();

    //- @PARAM ref Param
    initial $display(PARAM);

    //- @instance2 ref Instance2
    //- @var1 ref Var1
    initial $display(instance2::var1);

    //- @instance2 ref Instance2
    //- @my_fun ref MyFun
    //- @my_fun ref/call MyFun
    initial $display(instance2.my_fun());

    //- @instance2 ref Instance2
    //- @instance1 ref Instance1
    //- @var1 ref Var1
    initial $display(instance2::instance1::var1);
endmodule
