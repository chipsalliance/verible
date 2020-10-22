// Copyright 2020 Google LLC.
// SPDX-License-Identifier: Apache-2.0

//- @MyInterface defines/binding MyInterface
//- MyInterface.node/kind interface
interface MyInterface();
  //- @reset defines/binding _
  wire reset;
  //- @clk defines/binding _
  wire clk;
  //- @in defines/binding _
  logic [7:0] in;

  //- @MyNestedInterface defines/binding MyNestedInterface
  //- MyNestedInterface childof MyInterface
  //- MyNestedInterface.node/kind interface
  interface MyNestedInterface();

    //- @MyNestedInterface ref MyNestedInterface
  endinterface: MyNestedInterface
  //- @MyInterface ref MyInterface
endinterface: MyInterface
