//- @f1 defines/binding F1
//- @A defines/binding A
//- @B defines/binding B
function void f1(int A, int B);
endfunction

//- @g1 defines/binding _
//- @C defines/binding C
//- @D defines/binding D
function void g1(int C, int D);
    //- @f1 ref F1
    //- @A ref A
    //- @B ref B
    //- @C ref C
    //- @D ref D
    f1(.A(C), .B(D));
endfunction
