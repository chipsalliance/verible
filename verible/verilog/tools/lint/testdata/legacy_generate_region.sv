// verilog_syntax: parse-as-module-body
generate
  for (genvar k = 0; k < FooParam; k++) begin : gen_loop
    // code
  end
endgenerate
