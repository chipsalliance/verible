module disable_non_sequential;
  initial begin
    fork
      begin
        #6;
      end
    join_any
    disable fork_invalid;
  end
endmodule
