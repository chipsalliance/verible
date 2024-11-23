 //- @foo defines/binding Foo
 module foo #(
    //- @baz defines/binding Baz
    parameter int baz = 2
  ) ();
endmodule

//- @m1 defines/binding _
module m1();
    //- @baz defines/binding Baz1
    //- @x defines/binding X
    int baz, x;

    //- @foo ref Foo
    foo #(
        //- @#0baz ref Baz
        //- @#1baz ref Baz1
        .baz(baz))
    //- @bar defines/binding Bar0
    bar();

    //- @x ref X
    if (x) begin
        //- @foo ref Foo
        foo #(
            //- @#0baz ref Baz
            //- @#1baz ref Baz1
            .baz(baz)
        )
        //- @bar defines/binding Bar1
        bar();
        
        //- @bar ref Bar1
        //- @#0baz ref Baz
        //- @#1baz ref Baz1
        assign bar.baz = baz;
    end else if(x) begin
        //- @foo ref Foo
        foo #(
            //- @#0baz ref Baz
            //- @#1baz ref Baz1
            .baz(baz)
        )
        //- @bar defines/binding Bar2
        bar();

        //- @bar ref Bar2
        //- @#0baz ref Baz
        //- @#1baz ref Baz1
        assign bar.baz = baz;
    end else begin
       //- @foo ref Foo
        foo #(
            //- @#0baz ref Baz
            //- @#1baz ref Baz1
            .baz(baz)
        )
        //- @bar defines/binding Bar3
        bar();

        //- @bar ref Bar3
        //- @#0baz ref Baz
        //- @#1baz ref Baz1
        assign bar.baz = baz;
    end
    
    //- @bar ref Bar0
    //- @#0baz ref Baz
    //- @#1baz ref Baz1
    assign bar.baz = baz;

  endmodule
