//- _FileNode.node/kind file

//- @top defines/binding TopModule
//- TopModule.node/kind record
//- TopModule.subkind module
//- TopModule.complete definition
module top;
endmodule

//- @top_top defines/binding TopTopModule
//- TopTopModule.node/kind record
//- TopTopModule.subkind module
//- TopTopModule.complete definition
module top_top;

  //- @#0top ref TopModule
  //- @top_instance defines/binding TopInstanceDef
  //- TopInstanceDef.node/kind variable
  //- TopInstanceDef.complete definition
  //- TopInstanceDef childof TopTopModule
  top top_instance ();
endmodule
