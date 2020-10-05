class my_class;
endclass

package pkg2;
    class my_class2;

        my_class instance1 = new();
        static int x;

        static function int my_fun();
            return x;
        endfunction
    endclass
endpackage
