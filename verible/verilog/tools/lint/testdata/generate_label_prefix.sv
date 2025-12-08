module generate_label_prefix;
  // verilog_lint: waive legacy-genvar-declaration
  genvar i;
  for (i = 0; i < 5; ++i) begin : invalid_label
  end
endmodule
