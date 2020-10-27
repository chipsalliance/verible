// Copyright 2020 Google LLC.
// SPDX-License-Identifier: Apache-2.0

//- _FileNode.node/kind file

//- @my_module defines/binding MyModule
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
module top;
  //- @aa defines/binding AA
  //- @bb defines/binding BB
  //- @cc defines/binding CC
  bit aa, bb, cc;

  //- @my_module ref MyModule
  //- @dut defines/binding _
  //- @aa ref AA
  //- @bb ref BB
  //- @cc ref CC
  my_module dut(.in1(aa), .in2(bb), .out1(cc));

endmodule

//- @top_top defines/binding TopTop
module top_top;
  //- @#0top ref Top
  //- @top_module defines/binding TopInstance
  //- TopInstance childof TopTop
  top top_module();
endmodule
