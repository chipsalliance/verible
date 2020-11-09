//- @my_class defines/binding MyClass1
class my_class;
endclass

//- @pkg2 defines/binding _
package pkg2;
    //- @my_class2 defines/binding _
    class my_class2;

        //- @my_class ref MyClass1
        //- @instance1 defines/binding _
        my_class instance1 = new();

        //- @var2 defines/binding Var2
        static int var2;

        //- @my_fun defines/binding _
        static function int my_fun();
            //- @var2 ref Var2
            return var2;
        endfunction
    endclass
endpackage
