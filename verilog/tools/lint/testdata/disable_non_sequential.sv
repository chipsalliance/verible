module disable_non_sequential;
  initial begin
    fork : foo
      begin
        #6;
      end
    join_any
    disable foo;
  end
endmodule
