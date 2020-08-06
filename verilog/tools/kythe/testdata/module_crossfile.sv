// Copyright 2020 Google LLC.
// SPDX-License-Identifier: Apache-2.0

//- _FileNode.node/kind file
// Module references package defined in package.sv.
module module_crossfile;
//- @my_pkg1 ref MyPkg1
  import my_pkg1::*;
//- @my_integer ref MyInteger
  initial $display(my_integer);
endmodule


