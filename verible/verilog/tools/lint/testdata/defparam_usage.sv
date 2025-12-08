module defparam_usage;
  // defparam shouldn't be used, this should trigger the forbid-defparam rule
  defparam p0.MY_PARAM = 1;
endmodule
