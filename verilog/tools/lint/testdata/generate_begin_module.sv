module generate_begin_module;
  // verilog_lint: waive legacy-generate-region
  generate
    begin : gen_block1
      always @(posedge clk) foo <= bar;
    end
  endgenerate
endmodule
