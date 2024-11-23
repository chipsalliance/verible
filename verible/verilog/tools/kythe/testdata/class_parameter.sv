//- @Stack defines/binding Stack
//- @T defines/binding T
class Stack #(parameter type T=int);
    //- @T ref T
    //- @items defines/binding _
    //- @idx defines/binding _
    T items[64], idx=0;

    //- @T ref T
    //- @val defines/binding _
    function void push(input T val);
    endfunction
endclass

//- @Vector defines/binding Vector
//- @WIDTH defines/binding WIDTH
class Vector #(parameter WIDTH=1);
    //- @WIDTH ref WIDTH
    //- @data defines/binding _
    bit [WIDTH-1:0] data;
endclass

//- @my_module defines/binding _
module my_module();
    //- @Stack ref Stack
    //- @real_stack defines/binding _
    Stack #(real) real_stack = new();
    
    //- @#0Stack ref Stack
    //- @#1Stack ref Stack
    //- @instance1 defines/binding _
    Stack#(Stack#(int)) instance1 = new;

    //- @#0Vector ref Vector
    //- @#1Vector ref Vector
    //- @instance2 defines/binding _
    Vector#(Vector#(int)) instance2 = new;

    //- @Vector ref Vector
    //- @v1 defines/binding _
    Vector v1 = new();       
    //- @x1 defines/binding X1
    int x1;       
    //- @Vector ref Vector
    //- @v2 defines/binding _
    //- @x1 ref X1
    //- @WIDTH ref WIDTH
    Vector #(.WIDTH(x1)) v2 = new();  
endmodule

//- @uvm_sequence_item defines/binding UvmSequenceItem
class uvm_sequence_item;
endclass

//- @M defines/binding M
package M;
    //- @uvm_sequence defines/binding UvmSequence
    //- @T defines/binding T1
    class uvm_sequence #(parameter type T = int);
    endclass
endpackage

//- @core_base_seq defines/binding _
//- @#0REQ defines/binding REQ
//- @#0uvm_sequence_item ref UvmSequenceItem
//- @#1uvm_sequence_item ref UvmSequenceItem
//- @ Y defines/binding _
//- @M ref M
//- @#2uvm_sequence ref UvmSequence
//- @T ref T1
//- @#1REQ ref REQ
class core_base_seq #(type REQ = uvm_sequence_item, Y = uvm_sequence_item) extends M::uvm_sequence#(.T(REQ));
endclass

//- @foo_c defines/binding FooC
class foo_c #(
    //- @A defines/binding A
    parameter int A = 1,
    //- @B defines/binding B
    parameter int B = 0
);
    //- @bar_c defines/binding BarC
    class bar_c #(
        //- @X defines/binding X
        parameter int X = 1,
        //- @W defines/binding W
        parameter int W = 0
    );
        //- @get2 defines/binding Get2
        function static int get2();
        endfunction
    endclass
endclass

function void f1();
    //- @var1 defines/binding Var1
    //- @var2 defines/binding Var2
    int var1, var2;

    //- @foo_c ref FooC
    //- @bar_c ref BarC
    //- @X ref X
    //- @B ref B
    //- @A ref A
    //- @W ref W
    //- @#0var1 ref Var1
    //- @#0var2 ref Var2
    //- @#1var1 ref Var1
    //- @#1var2 ref Var2
    //- @get2 ref Get2
    foo_c#(.A(var1), .B(var2))::bar_c#(.X(var1), .W(var2))::get2();
endfunction
