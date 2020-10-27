//- @m1 defines/binding _
module m1;
    //- @temp defines/binding Temp0
    //- @x defines/binding X
    int temp, x;

    //- @x ref X
    if (x) begin
        //- @temp defines/binding Temp7
        int temp;
        //- @temp ref Temp7
        assign temp = 1;
        //- @x ref X
    end else if(x) begin
        //- @temp defines/binding Temp8
        int temp;
        //- @temp ref Temp8
        assign temp = 1;
    end else begin
        //- @temp defines/binding Temp9
        int temp;
        //- @temp ref Temp9
        assign temp = 1;
    end

    //- @#0 x ref X
    if (x) begin 
        //- @temp defines/binding Temp1
        int temp;
        //- @temp ref Temp1
        assign temp = 1;
        //- @x ref X
    end else if(x) begin
        //- @temp defines/binding Temp3
        int temp;
        //- @temp ref Temp3
        assign temp = 1;
    end else begin
        //- @temp defines/binding Temp4
        int temp;
        //- @temp ref Temp4
        assign temp = 1;
    end
    
    //- @temp ref Temp0
    assign temp = 1;

  endmodule
