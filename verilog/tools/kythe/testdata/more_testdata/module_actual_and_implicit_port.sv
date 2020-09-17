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
