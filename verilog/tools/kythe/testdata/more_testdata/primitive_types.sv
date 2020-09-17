//- _FileNode.node/kind file

//- @my_pkg1 defines/binding MyPkg1
//- MyPkg1.node/kind package
package my_pkg1;
    //- @int1 defines/binding Int1Def
    //- Int1Def.node/kind variable
    //- Int1Def.complete definition
    //- Int1Def childof MyPkg1
    //- @int2 defines/binding Int2Def
    //- Int2Def.node/kind variable
    //- Int2Def.complete definition
    //- Int2Def childof MyPkg1
    int int1, int2;

    //- @var1 defines/binding Var1Def
    //- Var1Def.node/kind variable
    //- Var1Def.complete definition
    //- Var1Def childof MyPkg1
    int var1 = 1;

    //- @var2 defines/binding Var2Def
    //- Var2Def.node/kind variable
    //- Var2Def.complete definition
    //- Var2Def childof MyPkg1
    //- @var3 defines/binding Var3Def
    //- Var3Def.node/kind variable
    //- Var3Def.complete definition
    //- Var3Def childof MyPkg1
    int var2 = 2, var3 = 3;

    //- @logic1 defines/binding Logic1Def
    //- Logic1Def.node/kind variable
    //- Logic1Def.complete definition
    //- Logic1Def childof MyPkg1
    //- @logic2 defines/binding Logic2Def
    //- Logic2Def.node/kind variable
    //- Logic2Def.complete definition
    //- Logic2Def childof MyPkg1
    logic logic1, logic2;

    //- @bit1 defines/binding Bit1Def
    //- Bit1Def.node/kind variable
    //- Bit1Def.complete definition
    //- Bit1Def childof MyPkg1
    //- @bit2 defines/binding Bit2Def
    //- Bit2Def.node/kind variable
    //- Bit2Def.complete definition
    //- Bit2Def childof MyPkg1
    bit bit1, bit2;

    //- @string1 defines/binding String1Def
    //- String1Def.node/kind variable
    //- String1Def.complete definition
    //- String1Def childof MyPkg1
    //- @string2 defines/binding String2Def
    //- String2Def.node/kind variable
    //- String2Def.complete definition
    //- String2Def childof MyPkg1
    string string1, string2;

    //- @my_function defines/binding MyFunction
    //- MyFunction.node/kind function
    //- MyFunction.complete definition
    //- MyFunction childof MyPkg1
    //- @my_arg1 defines/binding MyArg1
    //- MyArg1.node/kind variable
    //- MyArg1.complete definition
    //- MyArg1 childof MyFunction
    //- @my_arg2 defines/binding MyArg2
    //- MyArg2.node/kind variable
    //- MyArg2.complete definition
    //- MyArg2 childof MyFunction
    function int my_function(int my_arg1, int my_arg2);

        //- @var1_fun defines/binding Var1FunDef
        //- Var1FunDef.node/kind variable
        //- Var1FunDef.complete definition
        //- Var1FunDef childof MyFunction
        int var1_fun = 1;

        //- @var2_fun defines/binding Var2FunDef
        //- Var2FunDef.node/kind variable
        //- Var2FunDef.complete definition
        //- Var2FunDef childof MyFunction
        //- @var3_fun defines/binding Var3FunDef
        //- Var3FunDef.node/kind variable
        //- Var3FunDef.complete definition
        //- Var3FunDef childof MyFunction
        int var2_fun = 2, var3_fun = 3;

        //- @x defines/binding XDef
        //- XDef.node/kind variable
        //- XDef.complete definition
        //- XDef childof MyFunction
        //- @y defines/binding YDef
        //- YDef.node/kind variable
        //- YDef.complete definition
        //- YDef childof MyFunction
        int x, y;

        //- @x ref XDef
        //- @my_arg1 ref MyArg1
        x = my_arg1;

        //- @y ref YDef
      return y;
    endfunction
  
    //- @my_pkg1 ref MyPkg1
  endpackage : my_pkg1
