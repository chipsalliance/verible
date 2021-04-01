// Copyright 2020 Google LLC.
// SPDX-License-Identifier: Apache-2.0

// File node require empty language & signature.
//- _FileNode=vname("", "", _, _, "").node/kind file

//- @array defines/binding _
module array (
    //- @arr1 defines/binding Arr1
    input bit [7:0] arr1
);
  //- @mock_arr defines/binding MockArr
  bit [7:0] mock_arr;
  //- @arr2 defines/binding Arr2
  int arr2[8][32];

  //- @arr1 ref Arr1
  //- @mock_arr ref MockArr
  assign mock_arr[0] = arr1[0];

  //- @arr2 ref Arr2
  assign arr2[0][31] = 42;
endmodule

//- @dynamic_arrays defines/binding _
module dynamic_arrays;
  //- @arr defines/binding Arr
  bit [7:0] arr[];

  initial begin
    //- @arr ref Arr
    arr = new[8];

    //- @arr ref Arr
    $display("Array size: %d", arr.size());

    //- @#0arr ref Arr
    //- @#1arr ref Arr
    arr = new[16] (arr);
  end

endmodule

//- @associative_arrays defines/binding _
module associative_arrays;
  //- @real_arr defines/binding RealArr
  int real_arr[int];

  initial begin
    //- @real_arr ref RealArr
    real_arr[80] = 81;
    //- @real_arr ref RealArr
    real_arr[0]  = 1;

    //- @var1 defines/binding Var1
    //- @var2 defines/binding Var2
    int var1 = 0, var2 = 0;

    //- @#1i defines/binding I
    //- @#3i ref I
    //- @#4i ref I
    //- @var1 ref Var1
    //- @#0var2 ref Var2
    //- @#1var2 ref Var2
    //- @j defines/binding _
    //- @m defines/binding _
    for (int i = 0, j = 0, bit [var1:var2] m = var2; i < 50; i++) begin
      //- @real_arr ref RealArr
      //- @#0i ref I
      //- @#1i ref I
      real_arr[i] = i;
    end
  end
endmodule
