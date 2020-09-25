//- _FileNode.node/kind file

//- @Different_Port_Type defines/binding DifferentPort
//- DifferentPort.node/kind record
//- DifferentPort.subkind module
//- DifferentPort.complete definition
module Different_Port_Type (
    ///- @input1 defines/binding InDef
    //- InDef.node/kind variable
    //- InDef.complete definition
    //- InDef childof DifferentPort
    input input1,
    //- @wire1 defines/binding WDef
    //- WDef.node/kind variable
    //- WDef.complete definition
    //- WDef childof DifferentPort
    wire wire1,
    //- @bit1 defines/binding BDef
    //- BDef.node/kind variable
    //- BDef.complete definition
    //- BDef childof DifferentPort
    bit bit1
);

  //- @Different_Port_Type ref DifferentPort
endmodule : Different_Port_Type

//- @Non_ANSI defines/binding NON_ANSI
//- NON_ANSI.node/kind record
//- NON_ANSI.subkind module
//- NON_ANSI.complete definition
module Non_ANSI (
    //- @in1 ref In1Def
    in1,
    //- @in2 ref In2Def
    in2,
    // - @out1 ref Out1Def
    out1
);

  //- @in1 defines/binding In1Def
  //- In1Def.node/kind variable
  //- In1Def.complete definition
  //- In1Def childof NON_ANSI
  //- @in2 defines/binding In2Def
  //- In2Def.node/kind variable
  //- In2Def.complete definition
  //- In2Def childof NON_ANSI
  input in1, in2;

  //- @out1 defines/binding Out1Def
  //- Out1Def.node/kind variable
  //- Out1Def.complete definition
  //- Out1Def childof NON_ANSI
  output out1;

  //- @Non_ANSI ref NON_ANSI
endmodule : Non_ANSI

//- @my_module defines/binding MyModule
//- MyModule.node/kind record
//- MyModule.subkind module
//- MyModule.complete definition
module my_module (
    //- @x defines/binding XDef
    //- XDef.node/kind variable
    //- XDef.complete definition
    //- XDef childof MyModule
    input x,
    //- @y defines/binding YDef
    //- YDef.node/kind variable
    //- YDef.complete definition
    //- YDef childof MyModule
    input wire y,
    //- @z defines/binding ZDef
    //- ZDef.node/kind variable
    //- ZDef.complete definition
    //- ZDef childof MyModule
    output z
);

  //- @w1 defines/binding W1Def
  //- W1Def.node/kind variable
  //- W1Def.complete definition
  //- W1Def childof MyModule
  //- @w2 defines/binding W2Def
  //- W2Def.node/kind variable
  //- W2Def.complete definition
  //- W2Def childof MyModule
  //- @w3 defines/binding W3Def
  //- W3Def.node/kind variable
  //- W3Def.complete definition
  //- W3Def childof MyModule
  wire w1, w2, w3;

  //- @Non_ANSI ref NON_ANSI  
  Non_ANSI
      //- @instance1 defines/binding Instance1Def
      //- Instance1Def.node/kind variable
      //- Instance1Def.complete definition
      //- Instance1Def childof MyModule
      instance1 (
          //- @x ref XDef
          x,
          //- @y ref YDef
          y,
          //- @z ref ZDef
          z
      ),
      //- @instance2 defines/binding Instance2Def
      //- Instance2Def.node/kind variable
      //- Instance2Def.complete definition
      //- Instance2Def childof MyModule
      instance2 (
          //- @w1 ref W1Def
          w1,
          //- @w2 ref W2Def
          w2,
          //- @w3 ref W3Def
          w3
      );

  //- @my_module ref MyModule
endmodule : my_module

//- @int1 defines/binding Int1Def
//- Int1Def.node/kind variable
//- Int1Def.complete definition
//- Int1Def childof MyPkg1
//- @int2 defines/binding Int2Def
//- Int2Def.node/kind variable
//- Int2Def.complete definition
//- Int2Def childof MyPkg1
int int1, int2;

//- @my_module2 defines/binding MyModule2
//- MyModule2.node/kind record
//- MyModule2.subkind module
//- MyModule2.complete definition
module my_module2 (
    //- @x defines/binding XDef1
    //- XDef1.node/kind variable
    //- XDef1.complete definition
    //- XDef1 childof MyModule2
    //- @#0int1 ref LDef
    //- @#1int1 ref LDef
    //- @#0int2 ref RDef
    //- @#1int2 ref RDef
    input [int1:int2] x[int1:int2],
    //- @y defines/binding YDef2
    //- YDef2.node/kind variable
    //- YDef2.complete definition
    //- YDef2 childof MyModule2
    //- @#0int1 ref LDef
    //- @#1int1 ref LDef
    //- @#0int2 ref RDef
    //- @#1int2 ref RDef
    input wire [int1:int2] y[int1:int2]
);

  //- @my_module2 ref MyModule2
endmodule : my_module2

