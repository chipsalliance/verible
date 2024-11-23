//- @S defines/binding S
//- @T defines/binding T
class S #(parameter type T);
    //- @T ref T
    //- @foo defines/binding _
    T::member_type foo;

    //- @some_type defines/binding SomeType
    //- SomeType.node/kind talias
    typedef int some_type;
endclass

//- @S ref S
//- @some_type ref SomeType
//- @bar defines/binding _
S#(logic)::some_type bar;

//- @m1 defines/binding _
module m1();
    //- @S ref S
    //- @some_type ref SomeType
    //- @bar defines/binding _
    S#(logic)::some_type bar;
endmodule
