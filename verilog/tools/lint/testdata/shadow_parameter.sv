class jumbo_packet;
    const int max_size = 9 * 1024;
    byte payload [];
    function new( int size );
        payload = new[ size > max_size ? max_size : size ];
        int jumbo_packet = 0;
    endfunction
endclass
