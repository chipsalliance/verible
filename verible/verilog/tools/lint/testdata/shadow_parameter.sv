class jumbo_packet; // the shadowed instance
    const int max_size = 9 * 1024;
    byte payload [];
    function new( int size );
        payload = new[ size > max_size ? max_size : size ];
        int jumbo_packet = 0; // shadowing the class name, error
    endfunction
endclass
