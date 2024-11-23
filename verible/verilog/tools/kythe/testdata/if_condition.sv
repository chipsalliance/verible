//- @m2 defines/binding _
module m2;
    //- @x defines/binding X
    //- @y defines/binding Y
    int x, y;

    initial begin
        //- @#0 x ref X
        if (x) begin : gen_x
            //- @temp defines/binding Temp0
            int temp;
            //- @temp ref Temp0
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

        //- @#0y ref Y
        unique if (y) begin
            //- @temp defines/binding Temp1
            int temp;
            //- @temp ref Temp1
            assign temp = 1;
            //- @x ref X
        end else if(x) begin
            //- @temp defines/binding Temp5
            int temp;
            //- @temp ref Temp5
            assign temp = 1;
        end else begin
            //- @temp defines/binding Temp6
            int temp;
            //- @temp ref Temp6
            assign temp = 1;
        end

        //- @x ref X
        if (x) begin
            //- @temp defines/binding Temp2
            int temp;
            //- @temp ref Temp2
            assign temp = 1;
            //- @x ref X
        end else if(x) begin
            //- @temp defines/binding Temp11
            int temp;
            //- @temp ref Temp11
            assign temp = 1;
        end else begin
            //- @temp defines/binding Temp10
            int temp;
            //- @temp ref Temp10
            assign temp = 1;
        end

        //- @x ref X
        priority if (x) begin
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
    end

endmodule
