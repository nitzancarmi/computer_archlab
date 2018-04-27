
/***********************************
 * SP module
 **********************************/
module SP(clk, reset, start);
   input clk;
   input reset;
   input start;

   wire [15:0] sram_ADDR;
   wire [31:0] sram_DI;
   wire        sram_EN;
   wire        sram_WE;
   wire [31:0] sram_DO;
   wire [4:0]  opcode;
   wire [31:0] alu0;
   wire [31:0] alu1;
   wire [31:0] aluout_wire;

   // instantiate ALU, CTL and SRAM modules
   //SRAM SRAM(clk, sram_ADDR, sram_DI, sram_EN, sram_WE, sram_DO);
   SRAM SRAM (.clk(clk),
		.addr(sram_ADDR),
		.di(sram_DI),
		.en(sram_EN),
		.we(sram_WE),
		.do(sram_DO));

   ALU ALU(.opcode(opcode),
           .alu0(alu0),
           .alu1(alu1),
           .aluout(aluout_wire));

   CTL CTL(.clk(clk),
           .reset(reset),
	   .start(start),
	   .sram_ADDR(sram_ADDR),
	   .sram_DI(sram_DI),
	   .sram_EN(sram_EN),
	   .sram_WE(sram_WE),
	   .sram_DO(sram_DO),
	   .opcode(opcode),
	   .alu0(alu0),
	   .alu1(alu1),
	   .aluout_wire(aluout_wire));
   
   
endmodule  
