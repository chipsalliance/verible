//- @t1 defines/binding _
//- @y defines/binding Y
task t1(int y);
    //- @x defines/binding X
    int x[10];
    //- @x ref X
    //- @y ref Y
    @x[y];
endtask
