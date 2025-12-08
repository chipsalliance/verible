//- _FileNode.node/kind file

//- @class1 defines/binding _
class class1;

    //- @my_function defines/binding _
    function int my_function();
        //- @instance1 ref Instance1
        //- @instance2 ref Instance2
        //- @instance3 ref Instance3
        //- @var1 ref Var1
        return instance1.instance2.instance3.var1;
    endfunction

    //- @class2 ref MyClass2
    //- @instance1 defines/binding Instance1
    static class2 instance1 = new();
endclass

//- @class2 defines/binding MyClass2
class class2;
    //- @class3 ref MyClass3
    //- @instance2 defines/binding Instance2
    static class3 instance2 = new();
endclass

//- @class3 defines/binding MyClass3
class class3;
    //- @class4 ref MyClass4
    //- @instance3 defines/binding Instance3
    static class4 instance3 = new();
endclass

//- @class4 defines/binding MyClass4
class class4;
    //- @var1 defines/binding Var1
    static int var1;
endclass

//- @top_module defines/binding _
module top_module;
    //- @my_module ref MyModule  
    //- @dut defines/binding DutDef
    my_module dut();
    //- @dut ref DutDef
    //- @instance5 ref Instance5
    //- @var2 ref Var2
    initial $display(dut.instance5.var2);
endmodule

//- @my_module defines/binding MyModule
module my_module;
    //- @pkg ref/imports MyPkg1
    import pkg::*;
    //- @pkg_class1 ref PkgClass
    //- @instance5 defines/binding Instance5
    pkg_class1 instance5 = new();
endmodule

//- @pkg defines/binding MyPkg1
package pkg;
    //- @pkg_class1 defines/binding PkgClass
    class pkg_class1;
        //- @var2 defines/binding Var2
        static int var2;
    endclass
endpackage


// Used to check whether the extractor will crash or not.
module top_module2;
    //- @my_module2 ref MyModule2 
    //- @dut2 defines/binding DutDef2
    my_module2 dut2();
    //- @dut2 ref DutDef2
    //- @instance6 ref Instance6
    initial $display(dut2::instance6::var2);
endmodule

//- @my_module2 defines/binding MyModule2
module my_module2;
    import pkg2::*;
    //- @instance6 defines/binding Instance6
    pkg_class1 instance6 = new();
endmodule
