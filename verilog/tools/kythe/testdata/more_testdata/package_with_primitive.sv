// Copyright 2020 Google LLC.
// SPDX-License-Identifier: Apache-2.0

//- _FileNode.node/kind file
//- @my_pkg1 defines/binding MyPkg1
package my_pkg1;
  //- @my_integer defines/binding MyInteger
  integer my_integer = 10;
endpackage

// Second package also defines my_integer.
//- @my_pkg2 defines/binding MyPkg2
package my_pkg2;
  //- @my_integer defines/binding MyInteger2
  integer my_integer = 10;
endpackage

// First imported reference to the first package.
module my_module1;
  //- @my_pkg1 ref/imports MyPkg1
  import my_pkg1::*;
  //- @my_integer ref MyInteger
  initial $display(my_integer);
endmodule

// Second reference to the second package.
module my_module2;
  //- @my_pkg2 ref MyPkg2
  //- @my_integer ref MyInteger2
  initial $display(my_pkg2::my_integer);
endmodule
