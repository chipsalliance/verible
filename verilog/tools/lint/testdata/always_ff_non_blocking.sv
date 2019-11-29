module always_ff_non_blocking;
  always_ff @(posedge c)
    a = b; // [Style: sequential-logic] [always-ff-non-blocking]
endmodule
