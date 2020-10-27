//- _FileNode.node/kind file

//- @param_name defines/binding ParamName
//- ParamName.node/kind variable
//- ParamName.complete definition
parameter int param_name = 3;
 
//- @my_module2 defines/binding MyModule2
//- MyModule2.node/kind record
//- MyModule2.subkind module
//- MyModule2.complete definition
module  my_module2 ();
    //- @var1 defines/binding Var1
    parameter int var1 = 3;

    //- @my_module3 ref MyModule3
    my_module3 #(
        //- @var1 ref Var1
        //- @DATA_WIDTH ref DATA_WIDTH2
       .DATA_WIDTH(var1), 
        //- @ADDR_WIDTH ref ADDR_WIDTH
       .ADDR_WIDTH(8), 
        //- @RAM_DEPTH ref RAM_DEPTH
       .RAM_DEPTH(256)) 
        //- @instance1 defines/binding Instance1Def
        //- Instance1Def.node/kind variable
        //- Instance1Def.complete definition
        //- Instance1Def childof MyModule2
       instance1();
endmodule

//- @my_module3 defines/binding MyModule3
//- MyModule3.node/kind record
//- MyModule3.subkind module
//- MyModule3.complete definition
module  my_module3 (); 
    //- @DATA_WIDTH defines/binding DATA_WIDTH2
    //- DATA_WIDTH2 childof MyModule3
    parameter DATA_WIDTH = 8 ;

    //- @ADDR_WIDTH defines/binding ADDR_WIDTH
    //- ADDR_WIDTH childof MyModule3
    parameter ADDR_WIDTH = 8 ;

    //- @RAM_DEPTH defines/binding RAM_DEPTH
    //- RAM_DEPTH childof MyModule3
    //- @ADDR_WIDTH ref ADDR_WIDTH
    parameter RAM_DEPTH = 1 << ADDR_WIDTH;
endmodule

//- @my_module defines/binding MyModule
//- MyModule.node/kind record
//- MyModule.subkind module
//- MyModule.complete definition
module my_module 
  #(
    //- @ADDR_WIDTH defines/binding ADDR_WIDTH2
    //- ADDR_WIDTH2 childof MyModule
    parameter ADDR_WIDTH=8,
    //- @DATA_WIDTH defines/binding DATA_WIDTH
    //- DATA_WIDTH childof MyModule
    parameter DATA_WIDTH=32
  )
  ();
  
  //- @ADDR_WIDTH ref ADDR_WIDTH2
  //- @addr defines/binding _
  input  [ADDR_WIDTH-1:0] addr;
  //- @DATA_WIDTH ref DATA_WIDTH
  //- @data_output defines/binding _
  output [DATA_WIDTH-1:0] data_output;
  initial begin
      //- @ADDR_WIDTH ref ADDR_WIDTH2
    $display("My secret number is %d", ADDR_WIDTH);
  end

 endmodule

 //- @my_module4 defines/binding _
 module my_module4 #(
    //- @a_param defines/binding AParam
    parameter int a_param = 2,
    //- @a_param ref AParam
    //- @b_param defines/binding _
    localparam int b_param = a_param-1
  ) ();
  endmodule
