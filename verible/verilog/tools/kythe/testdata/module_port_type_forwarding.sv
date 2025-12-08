//- _FileNode.node/kind file

//- @my_module defines/binding MyModule
//- MyModule.node/kind record
//- MyModule.subkind module
//- MyModule.complete definition
module my_module (
    //- @n1 ref NDef
    n1,
    //- @b1 defines/binding B1Def
    //- B1Def.node/kind variable
    //- B1Def.complete definition
    //- B1Def childof MyModule
    bit b1,
    //- @b2 defines/binding B2Def
    //- B2Def.node/kind variable
    //- B2Def.complete definition
    //- B2Def childof MyModule
    b2,
    //- @x defines/binding XDef
    //- XDef.node/kind variable
    //- XDef.complete definition
    //- XDef childof MyModule
    input x,
    //- @x2 defines/binding X2Def
    //- X2Def.node/kind variable
    //- X2Def.complete definition
    //- X2Def childof MyModule
    x2,
    //- @y defines/binding YDef
    //- YDef.node/kind variable
    //- YDef.complete definition
    //- YDef childof MyModule
    output wire y,
    //- @z defines/binding ZDef
    //- ZDef.node/kind variable
    //- ZDef.complete definition
    //- ZDef childof MyModule
    z
);

    //- @n1 defines/binding NDef
    //- NDef.node/kind variable
    //- NDef.complete definition
    //- NDef childof MyModule
    input n1;

endmodule
