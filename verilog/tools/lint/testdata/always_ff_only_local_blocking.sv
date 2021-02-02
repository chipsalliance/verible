module always_ff_only_local_blocking;
  always_ff @(posedge c)
    a = b; // [Style: sequential-logic] [always-ff-only-local-blocking]
endmodule
