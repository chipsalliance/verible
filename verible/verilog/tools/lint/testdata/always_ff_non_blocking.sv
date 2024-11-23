module always_ff_non_blocking;
  always_ff @(posedge c) begin
    a = b; // [Style: sequential-logic] [always-ff-non-blocking]
  end
endmodule
