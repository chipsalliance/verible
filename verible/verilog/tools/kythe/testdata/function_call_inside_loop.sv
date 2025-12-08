//- @f1 defines/binding F1
//- @k defines/binding K
function int f1(int k);
    //- @k ref K
    return k;
endfunction

//- @m1 defines/binding _
module m1;
    //- @y defines/binding Y
    //- @x defines/binding X
    int y[10], x[10];

    //- @#0i defines/binding I
    //- @#1i ref I
    //- @#2i ref I
    for (genvar i=0; i<32; i++) begin
        //- @#1i ref I
        //- @#2i ref I
        //- @y ref Y
        //- @x ref X
        //- @f1 ref F1
        assign y[i] = f1(x[i]);
    end

    initial begin
        //- @#1i defines/binding I1
        //- @#2i ref I1
        //- @#3i ref I1
        for (int i = 0; i < 50; i++) begin
            //- @#1i ref I1
            //- @#2i ref I1
            //- @y ref Y
            //- @x ref X
            //- @f1 ref F1
            assign y[i] = f1(x[i]);
        end   
    end

  endmodule
