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
  //- @external_struct defines/binding ExternalStruct
  my_struct external_struct = '{1, 2, 100};

  //- @local_struct ref LocalStruct
  //- @cc ref CC
  assign local_struct.cc = 50;

  //- @local_struct ref LocalStruct
  //- @external_struct ref ExternalStruct
  //- @bb ref BB
  //- @xx ref XX
  assign local_struct.bb = external_struct.xx;

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
