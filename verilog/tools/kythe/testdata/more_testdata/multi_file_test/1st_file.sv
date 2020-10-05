package pkg1;
    parameter int PARAM = 1;
    class my_class;
        my_class instance1 = new();

        static int x;

        static function int my_fun();
            return x;
        endfunction
    endclass
endpackage
