//- @c defines/binding C
//- @x defines/binding _
`define c(x)

//- @f1 defines/binding _
//- @j defines/binding J
function int f1(int j);

    //- @#0"c" ref/expands C
    //- @#1"c" ref/call C
    //- @#2"c" ref/expands C
    //- @#0j ref J
    //- @#1j ref J
   return `c(`c(j)(`c(j)));
endfunction
