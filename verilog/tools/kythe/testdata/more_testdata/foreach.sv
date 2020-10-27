//- @m1 defines/binding _
module m1;
    //- @y defines/binding Y
    //- @temp defines/binding Temp0
    int y[10], temp;

    initial begin
        //- @y ref Y
        foreach (y) begin
            //- @temp defines/binding Temp8
            int temp;
            //- @temp ref Temp8
            assign temp = 1;
        end
    end
    
    //- @temp ref Temp0
    assign temp = 1;

  endmodule
