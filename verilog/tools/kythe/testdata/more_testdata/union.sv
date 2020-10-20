//- @pkg1 defines/binding Pkg1
package pkg1;
    union {
        //- @var4 defines/binding Var4
        int var4;
    //- @my_var defines/binding MyVar
    } my_var;

    //- @f2 defines/binding _
    function int f2();
        //- @my_var ref MyVar
        //- @var4 ref Var4
        return my_var.var4;
    endfunction
endpackage

union {
    //- @var6 defines/binding Var6
    //- Var6 childof MyVar2
    int var6;
//- @my_var2 defines/binding MyVar2
} my_var2;

typedef union packed {
    //- @a1 defines/binding A1
    logic [3:0] a1;
    //- @b1 defines/binding B1
    logic [1:0][1:0] b1;
//- @union_type defines/binding UnionType
//- UnionType.node/kind record
} union_type;

//- @union_type ref UnionType
//- @my_union defines/binding MyUnion
union_type my_union;

//- @my_module defines/binding _
module my_module
    (
        //- @out1 defines/binding Out1
        output out1,
        //- @out2 defines/binding Out2
        out2,
        //- @clk defines/binding Clk
        input clk,
        //- @in1 defines/binding In1
        input in1
    );

    union {
        //- @var7 defines/binding Var7
        //- Var7 childof MyVar3
        int var7;
    //- @my_var3 defines/binding MyVar3
    } my_var3;
    
    //- @my_var ref MyVar
    //- @var4 ref Var4
    //- @pkg1 ref Pkg1
    initial $display(pkg1::my_var.var4);

    //- @clk ref Clk
    always@(posedge clk) begin
        //- @my_union ref MyUnion
        //- @a1 ref A1
        //- @in1 ref In1
        my_union.a1 <= in1;
    end

    //@ clk ref Clk
    always@(posedge clk) begin
        //- @out1 ref Out1
        //- @my_union ref MyUnion
        //- @b1 ref B1
        out1 <= my_union.b1[0];
        //- @out2 ref Out2
        //- @my_union ref MyUnion
        //- @b1 ref B1
        out2 <= my_union.b1[1];
    end
endmodule
