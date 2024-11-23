// Copyright 2020 Google LLC.
// SPDX-License-Identifier: Apache-2.0

//- _FileNode.node/kind file
//- @my_pkg defines/binding MyPkg
package my_pkg;

    //- @my_class1 defines/binding MyClass1
    //- MyClass1.node/kind record
    //- MyClass1 childof MyPkg
    class my_class1;
        //- @my_var defines/binding MyVar
        int my_var;

        //- @my_function defines/binding MyFunction
        virtual function int my_function();
            //- @my_var ref MyVar
            return my_var;
        endfunction

        //- @my_task defines/binding MyTask
        //- MyTask.node/kind function
        //- @my_arg2 defines/binding MyArg2
        task my_task(int my_arg2);
        //- @my_arg2 ref MyArg2
        $display(my_arg2);
        endtask

        //- @nested_class defines/binding NestedClass
        //- NestedClass.node/kind record
        //- NestedClass childof MyClass1
        class nested_class;
        //- @nested_function defines/binding NestedFunction
        //- NestedFunction childof NestedClass
        function int nested_function();
            return 1;
        endfunction
        endclass

    endclass

    //- @my_class2 defines/binding MyClass2
    //- MyClass2.node/kind record
    //- MyClass2 extends MyClass1
    //- MyClass2 childof MyPkg
    //- @my_class1 ref MyClass1
    class my_class2 extends my_class1;

        //- @my_static_var defines/binding MyStaticVar
        static int my_static_var;

        //- @my_function defines/binding MyFunctionOverride
        //- MyFunctionOverride overrides MyFunction
        function int my_function();
             return 1;
        endfunction

    endclass

    class my_class3 extends my_class2;
        //- @my_function defines/binding MyFunctionOverride2
        //- MyFunctionOverride2 overrides MyFunctionOverride
        function int my_function();
        return 1;
        endfunction
    endclass
endpackage

module my_module;
    import my_pkg::*;
    initial begin
        //- @my_class1 ref MyClass1
        //- @handle1 defines/binding Handle1
        static my_class1 handle1 = new();

        //- @my_class2 ref MyClass2
        //- @handle2 defines/binding Handle2
        static my_class2 handle2 = new();

        //- @handle1 ref Handle1
        //- @my_var ref MyVar
        handle1.my_var = 10;

        //- @handle1 ref Handle1
        //- @my_function ref MyFunction
        //- @my_function ref/call MyFunction
        $display(handle1.my_function());

        //- @handle1 ref Handle1
        //- @my_task ref/call MyTask
        handle1.my_task(2);

        //- @handle2 ref Handle2
        //- @my_function ref MyFunctionOverride
        //- @my_function ref/call MyFunctionOverride
        $display(handle2.my_function());

        //- @my_class2 ref MyClass2
        //- @my_static_var ref MyStaticVar
        $display(my_class2::my_static_var);

    end
endmodule
