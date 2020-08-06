//- _FileNode.node/kind file

//- @my_module defines/binding MyModule
module my_module(
  //- @d defines/binding D
  input wire d
);

  //- @b defines/binding B
  wire b;

  //- @b ref B
  //- @d ref D
  assign b = d;
endmodule

//- @implicit_net defines/binding _
module implicit_net;
  //- @imp defines/binding Imp
  wire d;

  //- @my_module ref MyModule
  my_module inst(
    //- @imp ref Imp
    //- @d ref D
    .d
  );
endmodule
