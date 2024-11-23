
//- @A defines/binding _
interface A #(
    //- @DATA_WIDTH defines/binding DATA_WIDTH2
    parameter DATA_WIDTH = 32
) ();

  //- @instr_id defines/binding _
  //- @DATA_WIDTH ref DATA_WIDTH2
  logic [DATA_WIDTH-1:0] instr_id;
endinterface
