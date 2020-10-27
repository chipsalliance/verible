//- @m1 defines/binding _
module m1;
    //- @y defines/binding Y
    //- @x defines/binding X
    //- @#1i defines/binding I0
    int y[10], x[10], i;

    //- @#0i defines/binding I
    //- @#1i ref I
    //- @#2i ref I
    for (genvar i=0; i<32; i++) begin
        //- @#1i ref I
        //- @#2i ref I
        //- @y ref Y
        //- @x ref X
        assign y[i] = x[i];

        //- @temp defines/binding Temp0
        int temp;
        //- @temp ref Temp0
        assign temp = 1;
    end

    //- @#0i defines/binding I2
    //- @#1i ref I2
    //- @#2i ref I2
    for (genvar i=0; i<32; i++) begin
        //- @#1i ref I2
        //- @#2i ref I2
        //- @y ref Y
        //- @x ref X
      assign y[i] = x[i];

      //- @temp defines/binding Temp2
      int temp;
      //- @temp ref Temp2
      assign temp = 1;
    end

    initial begin
        //- @#1i defines/binding I1
        //- @#2i ref I1
        //- @#3i ref I1
        for (int i = 0; i < 50; i++) begin
            //- @temp defines/binding Temp1
            int temp;
            //- @temp ref Temp1
            assign temp = 1;
        end   
    end
    
    //- @#1i ref I0
    assign i = 1;

  endmodule
