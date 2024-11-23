//- @pkg defines/binding Pkg
package pkg;
    //- @my_class defines/binding MyClass
    //- @P1 defines/binding P1
    class my_class#(parameter P1 = 1);
        //- @var3 defines/binding Var3
        int var3;
    endclass
endpackage

//- @my_module defines/binding _
module my_module();
    //- @var4 defines/binding Var4
    int var4;
    //- @pkg ref Pkg
    //- @my_class ref MyClass
    //- @var1 defines/binding Var1
    //- @P1 ref P1
    //- @var4 ref Var4
    pkg::my_class#(.P1(var4)) var1;
    //- @pkg ref Pkg
    //- @my_class ref MyClass
    //- @var2 defines/binding Var2
    //- @P1 ref P1
    //- @var4 ref Var4
    pkg::my_class#(.P1(var4)) var2 = new();
    
    initial begin
         //- @var1 ref Var1
        var1 = new();

        //- @var1 ref Var1
        //- @var3 ref Var3
        $display(var1.var3);
        //- @var2 ref Var2
        //- @var3 ref Var3
        $display(var2.var3);
    end
endmodule

