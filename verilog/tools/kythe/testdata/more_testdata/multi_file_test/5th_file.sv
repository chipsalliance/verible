module my_module2(input x);
    
    my_module #(
            .ADDR_WIDTH(pkg1::PARAM))
        m(x);
    
endmodule
