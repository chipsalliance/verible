module forbid_consecutive_null_statements;
  always_ff @(posedge foo)
    ;; // [Style: consecutive-null-statements] [forbid-consecutive-null-statements]
endmodule
