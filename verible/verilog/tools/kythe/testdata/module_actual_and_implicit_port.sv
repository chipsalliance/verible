//- _FileNode.node/kind file

//- @my_module defines/binding MyModule
//- MyModule.node/kind record
//- MyModule.subkind module
//- MyModule.complete definition
module my_module (
    //- @in1 defines/binding In1Def
    //- In1Def.node/kind variable
    //- In1Def.complete definition
    //- In1Def childof MyModule
    input in1,
    //- @in2 defines/binding In2Def
    //- In2Def.node/kind variable
    //- In2Def.complete definition
    //- In2Def childof MyModule
    input wire in2,
    //- @z defines/binding ZDef
    //- ZDef.node/kind variable
    //- ZDef.complete definition
    //- ZDef childof MyModule
    output z
);

endmodule

//- @my_module2 defines/binding MyModule2
//- MyModule2.node/kind record
//- MyModule2.subkind module
//- MyModule2.complete definition
module my_module2 (
    //- @x defines/binding XDef
    //- XDef.node/kind variable
    //- XDef.complete definition
    //- XDef childof MyModule2
    input x,
    //- @y defines/binding YDef
    //- YDef.node/kind variable
    //- YDef.complete definition
    //- YDef childof MyModule2
    input wire y,
    //- @z defines/binding ZDef2
    //- ZDef2.node/kind variable
    //- ZDef2.complete definition
    //- ZDef2 childof MyModule2
    output z
);

    //- @my_module ref MyModule
    my_module
        //- @instance1 defines/binding Instance1Def
        //- Instance1Def.node/kind variable
        //- Instance1Def.complete definition
        //- Instance1Def childof MyModule2
        instance1(
            //- @x ref XDef
            //- @in1 ref In1Def
            .in1(x),
            //- @y ref YDef
            //- @in2 ref In2Def
            .in2(y),
            //- @z ref ZDef
            //- @z ref ZDef2
            .z
        );

endmodule

//- @my_module3 defines/binding MyModule3
//- MyModule3.node/kind record
//- MyModule3.subkind module
//- MyModule3.complete definition
module my_module3 (
);

    //- @x defines/binding X1Def
    //- X1Def.node/kind variable
    //- X1Def.complete definition
    //- X1Def childof MyModule3
    input x;
    //- @y defines/binding Y1Def
    //- Y1Def.node/kind variable
    //- Y1Def.complete definition
    //- Y1Def childof MyModule3
    wire y;
    //- @z defines/binding ZDef3
    //- ZDef3.node/kind variable
    //- ZDef3.complete definition
    //- ZDef3 childof MyModule3
    wire z;

    //- @my_module ref MyModule
    my_module
        //- @instance1 defines/binding Instance2Def
        //- Instance2Def.node/kind variable
        //- Instance2Def.complete definition
        //- Instance2Def childof MyModule3
        instance1(
            //- @x ref X1Def
            //- @in1 ref In1Def
            .in1(x),
            //- @y ref Y1Def
            //- @in2 ref In2Def
            .in2(y),
            //- @z ref ZDef
            //- @z ref ZDef3
            .z
        );

endmodule
