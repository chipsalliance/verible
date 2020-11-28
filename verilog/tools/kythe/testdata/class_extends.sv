//- @my_class1 defines/binding MyClass1
class my_class1;
    //- @var1 defines/binding Var1
    //- Var1 childof MyClass1
    int var1;

    //- @my_function1 defines/binding Function1
    //- Function1 childof MyClass1
    function int my_function1();
        //- @var1 ref Var1
        return var1;
    endfunction
endclass 

//- @my_class2 defines/binding MyClass2
//- @my_class1 ref MyClass1
//- MyClass2 extends MyClass1
class my_class2 extends my_class1;
    //- @instance2 defines/binding Instance2
    //- Instance2 childof MyClass2
    my_class2 instance2 = new();

    //- @var2 defines/binding Var2
    //- Var2 childof MyClass2
    int var2;

    //- @my_function2 defines/binding Function2
    //- Function2 childof MyClass2
    function int my_function2();
        //- @var1 ref Var1
        //- @var2 ref Var2
        return var1 + var2;
    endfunction
endclass

//- @my_module defines/binding MyModule
module my_module();
    //- @my_class2 ref MyClass2
    //- @instance1 defines/binding Instance1
    //- Instance1 childof MyModule
    my_class2 instance1 = new();

    initial begin
        //- @instance1 ref Instance1
        //- @var1 ref Var1
       $display(instance1::var1);
        //- @instance1 ref Instance1
        //- @var2 ref Var2
       $display(instance1.var2);
       
        //- @instance1 ref Instance1
        //- @instance2 ref Instance2
        //- @var1 ref Var1
       $display(instance1::instance2::var1);
        //- @instance1 ref Instance1
        //- @instance2 ref Instance2
        //- @var2 ref Var2
       $display(instance1.instance2.var2);

        //- @instance1 ref Instance1
        //- @my_function1 ref Function1
        //- @my_function1 ref/call Function1
       $display(instance1.my_function1());
        //- @instance1 ref Instance1
        //- @my_function2 ref Function2
        //- @my_function2 ref/call Function2
       $display(instance1.my_function2());

        //- @instance1 ref Instance1
        //- @instance2 ref Instance2
        //- @my_function1 ref Function1
        //- @my_function1 ref/call Function1
       $display(instance1::instance2.my_function1());
        //- @instance1 ref Instance1
        //- @instance2 ref Instance2
        //- @my_function2 ref Function2
        //- @my_function2 ref/call Function2
       $display(instance1.instance2.my_function2());
    end

endmodule
