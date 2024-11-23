//- _FileNode.node/kind file

//- @my_class defines/binding MyClass1
class my_class;
    //- @my_class ref MyClass1
    //- @instance1 defines/binding Instance1
    my_class instance1 = new();

    //- @var1 defines/binding Var1
    int var1;
endclass

//- @pkg1 defines/binding Pkg1
package pkg1;
    //- @my_class defines/binding MyClass2
    class my_class;
        //- @my_class ref MyClass2
        //- @instance2  defines/binding Instance2 
        static my_class instance2 = new();

        //- @var2 defines/binding Var2
        static int var2;
    endclass
endpackage

//- @my_module defines/binding _
module my_module();

    initial begin
        //- @my_class ref MyClass1
        //- @m_class defines/binding MClass
        my_class m_class = new();
        
        //- @m_class ref MClass
        //- @var1 ref Var1
        $display(m_class.var1);

        //- @pkg1 ref Pkg1
        //- @my_class ref MyClass2
        //- @instance2 ref Instance2
        //- @var2 ref Var2
        $display(pkg1::my_class::instance2.var2);

        //- @m_class ref MClass
        //- @#0instance1 ref Instance1
        //- @#1instance1 ref Instance1
        //- @var1 ref Var1
        $display(m_class.instance1.instance1.var1);

        //- @m_class ref MClass
        //- @instance1 ref Instance1
        //- @var1 ref Var1
        $display(m_class.instance1.var1);

        //- @#0m_class ref MClass
        //- @#1m_class ref MClass
        //- @#0var1 ref Var1
        //- @#1var1 ref Var1
        m_class.var1 <= m_class.var1;

    end

endmodule
