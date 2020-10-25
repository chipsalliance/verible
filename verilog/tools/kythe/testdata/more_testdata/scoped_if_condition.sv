//- @m2 defines/binding _
module m2;
    //- @x defines/binding X
    //- @y defines/binding Y
    int x, y;

    //- @#0 x ref X
    if (x) begin : gen_x
        //- @temp defines/binding Temp0
        int temp;
        //- @temp ref Temp0
        assign temp = 1;
    end

    //- @#0y ref Y
    if (y) begin : gen_y
        //- @temp defines/binding Temp1
        int temp;
        //- @temp ref Temp1
        assign temp = 1;
    end

    //- @x ref X
    if (x) begin
        //- @temp defines/binding Temp2
        int temp;
        //- @temp ref Temp2
        assign temp = 1;
    end

endmodule
