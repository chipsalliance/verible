module object_creation_name;
  initial begin
    // This is good
    cool_driver_h = cool_driver::type_id::create("cool_driver_h", this);
    // This is bad
    cool_driver_h = cool_driver::type_id::create("not_cool_driver_h", this);
  end
endmodule
