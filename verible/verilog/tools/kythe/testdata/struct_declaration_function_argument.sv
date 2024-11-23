//- @f1 defines/binding _
//- @x defines/binding X
//- @y defines/binding Y
//- @data defines/binding Data
//- X childof Data
//- Y childof Data
function f1(struct { int x; int y; } data);
    //- @sum defines/binding _
    //- @#0data ref Data
    //- @#1data ref Data
    //- @x ref X
    //- @y ref Y
    int sum = data.x + data.y;
endfunction

//- @f2 defines/binding _
//- @x defines/binding X1
//- @y defines/binding Y1
//- @h defines/binding H1
//- @data defines/binding Data1
//- X1 childof Data1
//- Y1 childof Data1
//- H1  childof X1
function f2(struct { union {int h;} x; int y; } data);
    //- @sum defines/binding _
    //- @#0data ref Data1
    //- @#1data ref Data1
    //- @x ref X1
    //- @y ref Y1
    //- @h ref H1
    int sum = data.x.h + data.y;
endfunction

