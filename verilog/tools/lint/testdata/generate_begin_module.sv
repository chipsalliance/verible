module generate_begin_module;
  generate
    begin : block1
      always @(posedge clk) foo <= bar;
    end
  endgenerate
endmodule
