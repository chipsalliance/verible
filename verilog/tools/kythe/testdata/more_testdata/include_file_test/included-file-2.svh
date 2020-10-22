// Copyright 2020 Google LLC.
// SPDX-License-Identifier: Apache-2.0

//- @"\"included-file-1.svh\"" ref/includes IncludedFile1Svh
//- IncludedFile1Svh.node/kind file
`include "included-file-1.svh"

//- @MyClassIncludedFile2 defines/binding MyClassIncludedFile2
class MyClassIncludedFile2;
    //- @var5 defines/binding Var5
    static int var5;

    //- @my_fun3 defines/binding _
    function int my_fun3();
        //- @var6 ref Var6
        //- @MyClassIncludedFile1 ref MyClassIncludedFile1
        return MyClassIncludedFile1::var6;
    endfunction
endclass
