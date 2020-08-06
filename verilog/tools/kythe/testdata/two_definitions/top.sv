// Copyright 2020 Google LLC.
// SPDX-License-Identifier: Apache-2.0

// Definition of in1 depends on an ifdef block

module top;

  `ifdef USE_PARAMETER
//- @in1 defines/binding DEFINED
    parameter int in1 = 10;
  `else
//- @in1 defines/binding NOTDEFINED
    logic in1 = 1'b1;
  `endif

  final begin
//- @in1 ref DEFINED
//- @in1 ref NOTDEFINED
    $display(in1);
    $display("TEST PASSED");
  end

endmodule : top
