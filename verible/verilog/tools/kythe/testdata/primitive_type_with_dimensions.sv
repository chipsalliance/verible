//- _FileNode.node/kind file

//- @foo_pkg defines/binding FooPkg
package foo_pkg;
  //- @DATA_WIDTH defines/binding DATA_WIDTH
  parameter int DATA_WIDTH = 1;
  //- @DATA_LENGTH defines/binding DATA_LENGTH
  parameter int DATA_LENGTH = 1;
endpackage

//- @baz_pkg defines/binding _
package baz_pkg;
  // //- @var5 defines/binding _
  // //- @#0foo_pkg ref FooPkg 
  // //- @#1foo_pkg ref FooPkg 
  // //- @#2foo_pkg ref FooPkg 
  // //- @#3foo_pkg ref FooPkg 
  // //- @#0DATA_WIDTH ref DATA_WIDTH
  // //- @#1DATA_WIDTH ref DATA_WIDTH
  // //- @#0DATA_LENGTH ref DATA_LENGTH
  // //- @#1DATA_LENGTH ref DATA_LENGTH
  int [foo_pkg::DATA_WIDTH-1:foo_pkg::DATA_LENGTH] var5 [foo_pkg::DATA_WIDTH-1:foo_pkg::DATA_LENGTH];

  //- @bar_c defines/binding _
  class bar_c;
    //- @var2 defines/binding _
    //- @#0foo_pkg ref FooPkg 
    //- @#1foo_pkg ref FooPkg 
    //- @#2foo_pkg ref FooPkg 
    //- @#3foo_pkg ref FooPkg 
    //- @#0DATA_WIDTH ref DATA_WIDTH
    //- @#1DATA_WIDTH ref DATA_WIDTH
    //- @#0DATA_LENGTH ref DATA_LENGTH
    //- @#1DATA_LENGTH ref DATA_LENGTH
    bit [foo_pkg::DATA_WIDTH-1:foo_pkg::DATA_LENGTH] var2 [foo_pkg::DATA_WIDTH-1:foo_pkg::DATA_LENGTH];
  endclass
endpackage
