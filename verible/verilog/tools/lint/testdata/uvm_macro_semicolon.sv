// UVM macro calls should not be followed by ';'
class c;
  function f();
    `uvm_info("msg_id", "message", UVM_LOW);
  endfunction
endclass
