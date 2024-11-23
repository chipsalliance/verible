//- @Bus defines/binding Bus
//- Bus.node/kind interface
interface Bus;
    //- @Addr defines/binding Addr
    //- @Data defines/binding Data
    logic [7:0] Addr, Data;
    //- @RWn defines/binding Rwn
    logic RWn;
endinterface

//- @RAM defines/binding _
//- @#0Bus ref Bus
//- @#0MemBus defines/binding MemBus
//- @MemBus2 defines/binding MemBus2
module RAM (Bus MemBus, MemBus2);
    //- @mem defines/binding Mem
    logic [7:0] mem[0:255];

    always @*
    //- @MemBus ref MemBus
    //- @RWn ref Rwn
    if (MemBus.RWn)
        //- @#0MemBus ref MemBus
        //- @Data ref Data
        //- @#1MemBus ref MemBus
        //- @Addr ref Addr
        //- @mem ref Mem
        MemBus.Data = mem[MemBus.Addr];
    else
        //- @#0MemBus ref MemBus
        //- @Addr ref Addr
        //- @#1MemBus ref MemBus
        //- @Data ref Data
        //- @mem ref Mem
        mem[MemBus.Addr] = MemBus.Data;

    always @*
    //- @MemBus2 ref MemBus2
    //- @RWn ref Rwn
    if (MemBus2.RWn)
        //- @#0MemBus2 ref MemBus2
        //- @Data ref Data
        //- @#1MemBus2 ref MemBus2
        //- @Addr ref Addr
        //- @mem ref Mem
        MemBus2.Data = mem[MemBus2.Addr];
    else
        //- @#0MemBus2 ref MemBus2
        //- @Addr ref Addr
        //- @#1MemBus2 ref MemBus2
        //- @Data ref Data
        //- @mem ref Mem
        mem[MemBus2.Addr] = MemBus2.Data;
endmodule
  