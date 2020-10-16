//- @Bus defines/binding Bus
//- Bus.node/kind interface
interface Bus;
  //- @Addr defines/binding Addr
  //- @Data defines/binding Data
  logic [7:0] Addr, Data;
  //- @RWn defines/binding Rwn
  logic RWn;
endinterface

//- @TestRAM defines/binding _
module TestRAM;
  x y;
  //- @#0Bus ref Bus
  //- @TheBus defines/binding TheBus
  Bus TheBus();                   
  
  //- @mem defines/binding Mem0
  logic[7:0] mem[0:7];

  //- @#0RAM ref Ram 
  //- @TheRAM defines/binding _
  //- @MemBus ref MemBus
  //- @TheBus ref TheBus
  RAM TheRAM (.MemBus(TheBus));   

  initial
  begin
      //- @TheBus ref TheBus
      //- @RWn ref Rwn
    TheBus.RWn = 0;         

    //- @TheBus ref TheBus
    //- @Addr ref Addr
    TheBus.Addr = 0;

    //- @#0I defines/binding I
    //- @#1I ref I
    //- @#2I ref I
    for (int I=0; I<7; I++)
    //- @#0TheBus ref TheBus
    //- @#0Addr ref Addr
    //- @#1TheBus ref TheBus
    //- @#1Addr ref Addr
      TheBus.Addr = TheBus.Addr + 1;
    
      //- @TheBus ref TheBus
    //- @RWn ref Rwn
    TheBus.RWn = 1;
    
    //- @TheBus ref TheBus
    //- @Data ref Data
    //- @mem ref Mem0
    TheBus.Data  = mem[0];
  end
endmodule

//- @RAM defines/binding Ram
//- @#0Bus ref Bus
//- @MemBus defines/binding MemBus
module RAM (Bus MemBus);
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
endmodule
