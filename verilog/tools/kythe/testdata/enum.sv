//- _FileNode.node/kind file
package my_pkg;
//- @AA defines/binding _AA
//- @BB defines/binding BB
//- @my_enum_var1 defines/binding MyEnumVar1
  enum {AA, BB} my_enum_var1;

//- @CC defines/binding _CC
//- @DD defines/binding DD
//- @my_enum2 defines/binding MyEnum2
  typedef enum {CC, DD} my_enum2;
endpackage

module my_module;
initial begin
//- @my_enum2 ref MyEnum2
//- @my_enum_var2 defines/binding _
//- @DD ref DD
  automatic my_pkg::my_enum2 my_enum_var2 = my_pkg::DD;

//- @my_enum_var1 ref MyEnumVar1
//- @BB ref BB
  my_pkg::my_enum_var1 = my_pkg::BB;
end
endmodule

