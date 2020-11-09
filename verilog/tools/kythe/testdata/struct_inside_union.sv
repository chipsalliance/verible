typedef union packed {
    // @data defines/binding _
    logic [9:0] data;
    struct packed {
        //- @op2 defines/binding Op2
         bit [2:0] op2;
         //- @op5 defines/binding Op5
         bit [2:0] op5;
    //- @op_modes defines/binding OpModes
    } op_modes;
//- @union_type defines/binding UnionType
//- UnionType.node/kind record
} union_type;

//- @union_type ref UnionType
//- @my_union defines/binding MyUnion
union_type my_union;

//- @var5 defines/binding Var5
int var5;
//- @var6 defines/binding Var6
int var6;

typedef struct {
    //- @a1 defines/binding A1
    //- @var5 ref Var5
    //- @var6 ref Var6
    bit [var5:var6] a1;
    //- @a2 defines/binding A2
    bit a2;
//- @s_1 defines/binding S1
//- S1.node/kind record
} s_1;

typedef union {
    //- @b1 defines/binding B1
    logic [7:0] b1;
    //- @s_1 ref S1
    //- @b2 defines/binding B2
    s_1 b2;
//- @union_type2 defines/binding UnionType2
//- UnionType2.node/kind record
} union_type2;

//- @union_type2 ref UnionType2
//- @my_union2 defines/binding MyUnion2
union_type2 my_union2;

//- @my_module defines/binding _
module my_module
    (
        //- @my_mult defines/binding MyMult
        output my_mult,
        //- @out1 defines/binding Out1
        out1,
        //- @out2 defines/binding Out2
        out2,
        //- @clk defines/binding Clk
        input clk,
        //- @in1 defines/binding In1
        input in1
    );

    //- @clk ref Clk
    always@(posedge clk) begin
        //- @my_mult ref MyMult
        //- @#0my_union ref MyUnion
        //- @#0op_modes ref OpModes
        //- @op2 ref Op2
        //- @#1my_union ref MyUnion
        //- @#1op_modes ref OpModes
        //- @op5 ref Op5
        my_mult <= my_union.op_modes.op2 * my_union.op_modes.op5;
    end

    //- @clk ref Clk
    always@(posedge clk) begin
        //- @my_union2 ref MyUnion2
        //- @b1 ref B1
        //- @in1 ref In1
        my_union2.b1 <= in1;
        //- @out1 ref Out1
        //- @my_union2 ref MyUnion2
        //- @b2 ref B2
        //- @a1 ref A1
        out1 <= my_union2.b2.a1;
        //- @out2 ref Out2
        //- @my_union2 ref MyUnion2
        //- @b2 ref B2
        //- @a2 ref A2
        out2 <= my_union2.b2.a2;
    end
endmodule

