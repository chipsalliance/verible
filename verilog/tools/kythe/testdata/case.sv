//- @m1 defines/binding _
module m1;
    //- @temp defines/binding Temp0
    //- @x defines/binding X
    //- @y defines/binding Y
    int temp, x, y;

    //- @temp ref Temp0
    case (temp) 
        //- @x ref X
       x: begin
           //- @temp defines/binding Temp7
            int temp;
            //- @temp ref Temp7
            assign temp = 1;
       end 
       //- @y ref Y
       y: begin
           //- @temp defines/binding Temp8
        int temp;
        //- @temp ref Temp8
        assign temp = 1;
       end
    endcase
    
    //- @temp ref Temp0
    assign temp = 1;

  endmodule
