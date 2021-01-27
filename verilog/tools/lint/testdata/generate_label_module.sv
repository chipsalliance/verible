module generate_label_module;
  // verilog_lint: waive legacy-generate-region
  generate if (foo) begin
    baz bam;
  end
  endgenerate
endmodule
