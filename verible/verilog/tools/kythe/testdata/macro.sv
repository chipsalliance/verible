//- _FileNode.node/kind file

//- @PRINT_STRING defines/binding PRINT_STRING
//- PRINT_STRING.node/kind macro
//- @#0str1 defines/binding Sr1_PRINT_STRING
//- Sr1_PRINT_STRING.node/kind variable
//- Sr1_PRINT_STRING.complete definition
//- Sr1_PRINT_STRING childof PRINT_STRING
`define PRINT_STRING(str1) $display("%s\n", str1)

//- @PRINT_3_STRINGS defines/binding PRINT_3_STRINGS
//- PRINT_3_STRINGS.node/kind macro
//- @str1 defines/binding Sr1_PRINT_3_STRINGS
//- Sr1_PRINT_3_STRINGS.node/kind variable
//- Sr1_PRINT_3_STRINGS.complete definition
//- Sr1_PRINT_3_STRINGS childof PRINT_3_STRINGS
//- @str2 defines/binding Sr2_PRINT_3_STRINGS
//- Sr2_PRINT_3_STRINGS.node/kind variable
//- Sr2_PRINT_3_STRINGS.complete definition
//- Sr2_PRINT_3_STRINGS childof PRINT_3_STRINGS
//- @str3 defines/binding Sr3_PRINT_3_STRINGS
//- Sr3_PRINT_3_STRINGS.node/kind variable
//- Sr3_PRINT_3_STRINGS.complete definition
//- Sr3_PRINT_3_STRINGS childof PRINT_3_STRINGS
`define PRINT_3_STRINGS(str1, str2, str3) \
    `PRINT_STRING(str1); \
    `PRINT_STRING(str2); \
    `PRINT_STRING(str3);

//- @PRINT_INT defines/binding PRINT_INT
//- PRINT_INT.node/kind macro
//- @#0int1 defines/binding Int1_PRINT_INT
//- Int1_PRINT_INT.node/kind variable
//- Int1_PRINT_INT.complete definition
//- Int1_PRINT_INT childof PRINT_INT
`define PRINT_INT(int1) $display("%d\n", int1)

//- @PRINT_3_INTS defines/binding PRINT_3_INTS
//- PRINT_3_INTS.node/kind macro
//- @int1 defines/binding Int1_PRINT_3_INTS
//- Int1_PRINT_3_INTS.node/kind variable
//- Int1_PRINT_3_INTS.complete definition
//- Int1_PRINT_3_INTS childof PRINT_3_INTS
//- @int2 defines/binding Int2_PRINT_3_INTS
//- Int2_PRINT_3_INTS.node/kind variable
//- Int2_PRINT_3_INTS.complete definition
//- Int2_PRINT_3_INTS childof PRINT_3_INTS
//- @int3 defines/binding Int3_PRINT_3_INTS
//- Int3_PRINT_3_INTS.node/kind variable
//- Int3_PRINT_3_INTS.complete definition
//- Int3_PRINT_3_INTS childof PRINT_3_INTS
`define PRINT_3_INTS(int1, int2, int3) \
    `PRINT_INT(int1); \
    `PRINT_INT(int2); \
    `PRINT_INT(int3);

//- @TEN defines/binding TEN
//- TEN.node/kind macro
`define TEN 10

//- @NUM defines/binding NUM
//- NUM.node/kind macro
//- @#0int1 defines/binding Int1
//- Int1.node/kind variable
//- Int1.complete definition
//- Int1 childof NUM
`define NUM(int1) int1

package my_pkg;
  function automatic my_function(int arg1, int arg2);
    //- @MacroFunction defines/binding MacroFunction
    //- MacroFunction.node/kind macro
    `define MacroFunction 10
    return arg1 + arg2;
  endfunction

  //- @MacroPackage defines/binding MacroPackage
  //- MacroPackage.node/kind macro
  `define MacroPackage 10

  task my_task();
    //- @MacroTask defines/binding MacroTask
    //- MacroTask.node/kind macro
    `define MacroTask 10
  endtask

  class my_class;
    //- @MacroClass defines/binding MacroClass
    //- MacroClass.node/kind macro
    `define MacroClass 10
  endclass
endpackage

module macro;
  //- @MacroModule defines/binding MacroModule
  //- MacroModule.node/kind macro
  `define MacroModule 10

  //- @"TEN" ref/expands TEN
  parameter int x = `TEN;
  initial begin

    //- @"PRINT_3_STRINGS" ref/expands PRINT_3_STRINGS
    `PRINT_3_STRINGS("Grand", "Tour", "S4");
    //- @"PRINT_3_INTS" ref/expands PRINT_3_INTS
    `PRINT_3_INTS(1, 2, 3);
    //- @"TEN" ref/expands TEN
    $display("%d\n", `TEN);
    //- @"MacroClass" ref/expands MacroClass
    $display("%d\n", `MacroClass);
    //- @"NUM" ref/expands NUM
    $display("%d\n", `NUM(1));
    //- @"TEN" ref/expands TEN
    //- @"NUM" ref/expands NUM
    $display("%d\n", `NUM(`TEN));

  end
endmodule
