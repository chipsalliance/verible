module generate_begin_module;
  // verilog_lint: waive legacy-generate-region
  generate
    begin : gen_block1
      always @(posedge clk) begin
        foo <= bar;
      end
    end
  endgenerate
endmodule
