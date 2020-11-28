//- @var1 defines/binding Var1
//- @var2 defines/binding Var2
//- @var3 defines/binding Var3
//- @var4 defines/binding Var4
int var1, var2, var3, var4;  // TODO(#604): Technically, this should be a localparam.

//- @f4 defines/binding _
//- @var1 ref Var1
//- @var2 ref Var2
//- @var3 ref Var3
//- @var4 ref Var4
//- @t1 defines/binding T1
function f4(int [var1:var2] t1 [var3:var4]);
    //- @x defines/binding X
    //- @y defines/binding Y
    //- @data defines/binding Data
    struct { int x; int y; } data;
    //- @sum defines/binding _
    //- @#0data ref Data
    //- @#1data ref Data
    //- @x ref X
    //- @y ref Y
    //- @t1 ref T1
    int sum = data.x + data.y + t1;
endfunction
