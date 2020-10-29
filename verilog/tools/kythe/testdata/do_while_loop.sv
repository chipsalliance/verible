//- @m1 defines/binding _
module m1;
    //- @temp defines/binding Temp0
    //- @y defines/binding Y
    int temp, y;

    initial begin
        do begin
            //- @temp defines/binding Temp8
            int temp;
            //- @temp ref Temp8
            assign temp = 1;
            //- @y ref Y
        end while(y < 5);
    end
    
    //- @temp ref Temp0
    assign temp = 1;

  endmodule
