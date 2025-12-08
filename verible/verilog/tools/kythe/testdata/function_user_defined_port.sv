//- @X defines/binding X
class X;
    //- @var1 defines/binding Var1
    int var1;
endclass

//- @f1 defines/binding _
//- @X ref X
//- @x1 defines/binding X1
function int f1(X x1);
    //- @x1 ref X1
    //- @var1 ref Var1
    return x1.var1;
endfunction

//- @f2 defines/binding _
//- @X ref X
//- @x1 defines/binding X2
task f2(X x1);
    //- @x1 ref X2
    //- @var1 ref Var1
    $display(x1.var1);
endtask

