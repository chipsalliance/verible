//- @BaseClass defines/binding BaseClass
virtual class BaseClass;
    //- @data defines/binding Data
    //- Data childof BaseClass
    int data;

    //- @f1 defines/binding F1
    //- F1 childof BaseClass
    //- F1.complete incomplete
    //- @x1 defines/binding _
    extern task f1(int x1);

    //- @getData defines/binding GetData1
    //- GetData1 childof BaseClass
    //- GetData1.complete incomplete
    //- @x defines/binding X
    //- X childof GetData1
    //- @y defines/binding Y
    //- Y childof GetData1
    pure virtual task getData(int x, int y);
 endclass
 
 //- @ChildClass defines/binding ChildClass
 //- ChildClass extends BaseClass
 class ChildClass extends BaseClass;
    //- @getData defines/binding GetData
    //- GetData overrides GetData1
    //- GetData childof ChildClass
    //- GetData.complete definition
    //- @x defines/binding X1
    //- X1 childof GetData
    //- @y defines/binding Y1
    //- Y1 childof GetData
    virtual task getData(int x, int y);
        //- @data ref Data
       data = 15;
    endtask
 endclass
 
 //- @tb defines/binding TB
 module tb();
    //- @ChildClass ref ChildClass
    //- @child defines/binding Child
    //- Child childof TB
    ChildClass child;
    initial begin
        //- @child ref Child
        child = new(); 
        //- @getData ref GetData
        //- @child ref Child
        child.getData(1, 2);
    end
 endmodule
