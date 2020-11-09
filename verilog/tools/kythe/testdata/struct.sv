// Copyright 2020 Google LLC.
// SPDX-License-Identifier: Apache-2.0

//- @pkg defines/binding _
package pkg;
  struct {
    //- @aa defines/binding AA1
    //- @dd defines/binding _
    byte aa, dd;
    //- @bb defines/binding _
    reg bb;
    //- @cc defines/binding CC1
    shortint unsigned cc;
  }
  //- @local_struct defines/binding LocalStruct1
  //- @other_struct defines/binding OtherStruct
  local_struct = '{1, 2, 100}, other_struct;

  typedef struct {
    //- @xx defines/binding _
    byte xx;
    //- @yy defines/binding _
    reg yy;
    //- @zz defines/binding ZZ1
    shortint unsigned zz;
  }
  //- @my_struct1 defines/binding MyStruct1
  my_struct1;

  //- @my_struct1 ref MyStruct1
  //- @struct_instance defines/binding StructInstnace
  my_struct1 struct_instance;

  //- @f1 defines/binding _
  function int f1();
    //- @struct_instance ref StructInstnace
    //- @zz ref ZZ1
    //- @other_struct ref OtherStruct
    //- @cc ref CC1
    //- @local_struct ref LocalStruct1
    //- @aa ref AA1
    return struct_instance.zz + other_struct.cc + local_struct.aa;
  endfunction

endpackage

typedef struct {
  //- @xx defines/binding XX
  byte xx;
  //- @yy defines/binding YY
  reg yy;
  //- @zz defines/binding ZZ
  shortint unsigned zz;
}
//- @my_struct defines/binding MyStruct
my_struct;

//- @module_with_struct defines/binding _
module module_with_struct;
  struct {
    //- @aa defines/binding AA
    byte aa;
    //- @bb defines/binding BB
    reg bb;
    //- @cc defines/binding CC
    shortint unsigned cc;
  }
  //- @local_struct defines/binding LocalStruct
  local_struct = '{1, 2, 100};

  //- @my_struct ref MyStruct
  //- @#0external_struct defines/binding ExternalStruct
  //- @external_struct2 defines/binding ExternalStruct2
  my_struct external_struct = '{1, 2, 100}, external_struct2;

  //- @local_struct ref LocalStruct
  //- @cc ref CC
  assign local_struct.cc = 50;

  //- @local_struct ref LocalStruct
  //- @#0external_struct ref ExternalStruct
  //- @bb ref BB
  //- @xx ref XX
  //- @external_struct2 ref ExternalStruct2
  //- @yy ref YY
  assign local_struct.bb = external_struct.xx + external_struct2.yy;

  //- @zz ref ZZ
  //- @cc ref CC
  assign local_struct.cc = external_struct.zz;

  //- @xx ref XX
  //- @external_struct ref ExternalStruct
  assign external_struct.xx = 0;

  //- @aa ref AA
  //- @xx ref XX
  assign local_struct.aa = external_struct.xx;

  //- @yy ref YY
  //- @bb ref BB
  assign external_struct.yy = local_struct.bb;
endmodule
