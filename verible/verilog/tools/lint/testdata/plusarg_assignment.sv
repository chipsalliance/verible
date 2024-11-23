class foo;
  function bar;
    // The use of $test$plusargs() is not allowed.
    if ($test$plusargs("baz")) begin
      return 1;
    end
  endfunction
endclass
