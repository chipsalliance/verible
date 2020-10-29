//- @my_module2 defines/binding _
module my_module2(
    //- @in2 defines/binding In2
    input in2);

    //- @my_module ref MyModule
    my_module #(
            //- @ADDR_WIDTH ref ADDR_WIDTH
            //- @pkg1 ref Pkg1
            //- @PARAM ref Param
            .ADDR_WIDTH(pkg1::PARAM))
        //- @m defines/binding _
        //- @in2 ref In2
        m(in2);
endmodule
