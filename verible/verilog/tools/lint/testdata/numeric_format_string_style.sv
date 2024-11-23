module numeric_format_string_style;
  initial begin
    // [Style: number-formatting] [numeric-format-string-style]
    $display("%h", hex);
    $display("%x", hex);
    $display("%b", bin);
    $display("0x%d", dec);
  end
endmodule
