//- _FileNode.node/kind file

//- @pkg1 defines/binding Pkg1
package pkg1;
    //- @PARAM defines/binding Param
    parameter int PARAM = 1;

    //- @my_class defines/binding MyClass
    class my_class;
        //- @instance1 defines/binding Instance1
        my_class instance1 = new();

        //- @var1 defines/binding Var1
        static int var1;

        //- @my_fun defines/binding MyFun
        static function int my_fun();
            //- @var1 ref Var1
            return var1;
        endfunction
    endclass
endpackage
