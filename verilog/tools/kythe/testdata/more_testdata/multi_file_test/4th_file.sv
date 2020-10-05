module my_module#(
    parameter ADDR_WIDTH=8
    ) 
    (input x);

    import pkg1::*;

    my_class instance1 = new();

    initial $display(PARAM);
    initial $display(instance1::x);
    initial $display(instance1.my_fun());
    initial $display(instance1::instance1::x);
endmodule
