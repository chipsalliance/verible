//- @p_pkg defines/binding PPkg
package p_pkg;
    //- @A defines/binding A
    class A;
        //- @var1 defines/binding Var1
        int var1;
    endclass

    //- @X defines/binding X
    class X;
        //- @Y defines/binding Y
        class Y;
            //- @var2 defines/binding Var2
            int var2;
        endclass
    endclass
endpackage

//- @C defines/binding C
//- @p_pkg ref PPkg
//- @A ref A
//- C extends A
class C extends p_pkg::A;
    //- @f1 defines/binding _
    function int f1();
        //- @var1 ref Var1
        return var1;
    endfunction
endclass

//- @H defines/binding H
//- @p_pkg ref PPkg
//- @X ref X
//- @Y ref Y
//- H extends Y
class H extends p_pkg::X::Y;
    //- @f2 defines/binding _
    function int f2();
        //- @var2 ref Var2
        return var2;
    endfunction
endclass
