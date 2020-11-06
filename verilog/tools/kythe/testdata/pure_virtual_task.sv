//- @BaseClass defines/binding BaseClass
virtual class BaseClass;
    //- @data defines/binding Data
    //- Data childof BaseClass
    int data;

    //- @getData defines/binding GetData1
    //- GetData1 childof BaseClass
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
