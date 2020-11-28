//- @uint defines/binding Uint
typedef int uint;

//- @my_module defines/binding _
module my_module();
    //- @y defines/binding Y
    int y = 0;
    initial begin
        //- @uint ref Uint
        //- @#0x defines/binding X
        //- @#1x ref X
        //- @#2x ref X
        for (uint x = 1; x < 5; x++) begin
            //- @y ref Y
            y++;
        end
    end
endmodule
