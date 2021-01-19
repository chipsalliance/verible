module disable_statement;
  initial begin
    fork
      begin
        #6;
      end
    join_any
    disable fork_invalid;
  end
endmodule
