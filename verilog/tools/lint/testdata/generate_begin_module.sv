module generate_begin_module;
  generate
    begin : gen_block1
      always @(posedge clk) foo <= bar;
    end
  endgenerate
endmodule
