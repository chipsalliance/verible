class constraints_check;
  logic max;
  // This line should trigger the constraint naming convention rule check
  constraint MyMax {  max >= 64; }
endclass
