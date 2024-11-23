module disable_statement;
  initial begin
    fork : foo
      begin
        #6;
      end
    join_any
    disable foo;
  end
endmodule
