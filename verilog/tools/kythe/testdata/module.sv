//- _FileNode.node/kind file

//- @my_module defines/binding MyModule
//- MyModule.node/kind function
module my_module (
  //- @in1 defines/binding In1
  input bit in1,
  //- @in2 defines/binding In2
  input bit in2,
  //- @out1 defines/binding Out1
  output bit out1
);

  //- @in1 ref In1
  //- @in2 ref In2
  //- @out1 ref Out1
  assign out1 = in1 & in2;
endmodule

//- @top defines/binding Top
//- Top.node/kind function
module top;
  //- @aa defines/binding AA
  //- @bb defines/binding BB
  //- @cc defines/binding CC
  bit aa, bb, cc;

  //- @my_module ref MyModule
  //- @dut defines/binding _
  //- @dut ref MyModule
  //- @"my_module dut(.in1(aa), .in2(bb), .out1(cc));" ref/call MyModule
  //- @"my_module dut(.in1(aa), .in2(bb), .out1(cc));" childof Top
  //- @aa ref AA
  //- @bb ref BB
  //- @cc ref CC
  my_module dut(.in1(aa), .in2(bb), .out1(cc));

endmodule

//- @top_top defines/binding TopTop
module top_top;
  //- @"top top_module();" childof TopTop
  top top_module();
endmodule
