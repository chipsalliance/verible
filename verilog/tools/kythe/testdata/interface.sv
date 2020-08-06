//- @MyInterface defines/binding MyInterface
interface MyInterface();
  wire reset;
  wire clk;
  logic [7:0] in;

  //- @MyNestedInterface defines/binding MyNestedInterface
  //- MyNestedInterface childof MyInterface
  interface MyNestedInterface();

  endinterface;
endinterface
