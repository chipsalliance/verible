module mismatched_labels(
  input clk_i
);

always_ff @(posedge clk_i)
  begin : foo
  end : bar // This mismatched label should cause an error

endmodule;
