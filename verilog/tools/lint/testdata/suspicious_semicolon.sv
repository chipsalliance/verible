module suspicious_semicolon ();
  initial begin
    if (x);
      $display("Hi");
  end
endmodule
