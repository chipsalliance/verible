//- @m1 defines/binding _
module m1;
    //- @temp defines/binding Temp0
    int temp;

    initial begin
        //- @temp ref Temp0
        repeat (temp) begin
            //- @temp defines/binding Temp8
            int temp;
            //- @temp ref Temp8
            assign temp = 1;
        end
    end
    
    //- @temp ref Temp0
    assign temp = 1;

  endmodule
