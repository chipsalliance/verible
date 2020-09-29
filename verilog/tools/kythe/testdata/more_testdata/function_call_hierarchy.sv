// Copyright 2020 Google LLC.
// SPDX-License-Identifier: Apache-2.0

//- _FileNode.node/kind file

//- @top_pkg defines/binding TopPkg
package top_pkg;
  //- @f1 defines/binding F1
  //- F1.node/kind function
  //- F1 childof TopPkg
  function automatic int f1(int arg1);
    int temp1 = f2(arg1);
    int temp2 = f2(arg1);
    //- @f3 ref/call F3
    //- @f3 ref F3
    return f3(arg1);
  endfunction

  //- @f2 defines/binding F2
  //- F2.node/kind function
  //- F2 childof TopPkg
  function automatic int f2(int arg1);
    //- @f3 ref F3
    //- @f3 ref/call F3
    return f3(arg1);
  endfunction

  //- @f3 defines/binding F3
  //- F3.node/kind function
  //- F3 childof TopPkg
  function automatic int f3(int arg1);
    return arg1;
  endfunction
endpackage
