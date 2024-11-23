module forbid_line_continuations;

string bad_literal = "Humpty Dumpty sat on a wall. \
Humpty Dumpty had a great fall.";

string good_literal = {"Humpty Dumpty sat on a wall.",
"Humpty Dumpty had a great fall."};

endmodule
