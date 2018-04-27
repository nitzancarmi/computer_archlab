`include "defines.vh"

/***********************************
 * CTL module
 **********************************/
module CTL(
	   clk,
	   reset,
	   start,
	   sram_ADDR,
	   sram_DI,
	   sram_EN,
	   sram_WE,
	   sram_DO,
	   opcode,
	   alu0,
	   alu1,
	   aluout_wire
	   );

   // inputs
   input clk;
   input reset;
   input start;
   input [31:0] sram_DO;
   input [31:0] aluout_wire;

   // outputs
   output [15:0] sram_ADDR;
   output [31:0] sram_DI;
   output 	 sram_EN;
   output 	 sram_WE;
   output [31:0] alu0;
   output [31:0] alu1;
   output [4:0]  opcode;

   // registers
   reg [31:0] 	 r[0:7];
//   reg [31:0] 	 r2;
//   reg [31:0] 	 r3;
//   reg [31:0] 	 r4;
//   reg [31:0] 	 r5;
//   reg [31:0] 	 r6;
//   reg [31:0] 	 r7;
   reg [15:0] 	 pc;
   reg [31:0] 	 inst;
   reg [4:0] 	 opcode;
   reg [2:0] 	 dst;
   reg [2:0] 	 src0;
   reg [2:0] 	 src1;
   reg [31:0] 	 alu0;
   reg [31:0] 	 alu1;
   reg [31:0] 	 aluout;
   reg [31:0] 	 immediate;
   reg [31:0] 	 cycle_counter;
   reg [2:0] 	 ctl_state;

   reg [15:0]    _sram_ADDR = 0;
   reg [31:0]    _sram_DI = 0;

   integer 	 verilog_trace_fp, rc;

   initial
     begin
	verilog_trace_fp = $fopen("verilog_trace.txt", "w");
     end

     assign sram_EN = (ctl_state == `CTL_STATE_FETCH0) | ((ctl_state == `CTL_STATE_EXEC0) & (opcode == `LD)) | 
	              ((ctl_state == `CTL_STATE_EXEC1) & (opcode == `ST));
     assign sram_WE = ((ctl_state == `CTL_STATE_EXEC1) & (opcode == `ST));
     assign sram_ADDR = _sram_ADDR;
     assign sram_DI = _sram_DI;
   /***********************************
    * set up sram inputs (outputs from sp)
    * 
    * TODO: fill here
    **********************************/

   // synchronous instructions
   always@(posedge clk)
     begin
	if (reset) begin
	   // registers reset
	   r[0] <= 0;
	   r[1] <= 0;
	   r[2] <= 0;
	   r[3] <= 0;
	   r[4] <= 0;
	   r[5] <= 0;
	   r[6] <= 0;
	   r[7] <= 0;
	   pc <= 0;
	   inst <= 0;
	   opcode <= 0;
	   dst <= 0;
	   src0 <= 0;
	   src1 <= 0;
	   alu0 <= 0;
	   alu1 <= 0;
	   aluout <= 0;
	   immediate <= 0;
	   cycle_counter <= 0;
	   ctl_state <= 0;
	   
	end else begin
	   // generate cycle trace
	   $fdisplay(verilog_trace_fp, "cycle %0d", cycle_counter);
	   $fdisplay(verilog_trace_fp, "r2 %08x", r[2]);
	   $fdisplay(verilog_trace_fp, "r3 %08x", r[3]);
	   $fdisplay(verilog_trace_fp, "r4 %08x", r[4]);
	   $fdisplay(verilog_trace_fp, "r5 %08x", r[5]);
	   $fdisplay(verilog_trace_fp, "r6 %08x", r[6]);
	   $fdisplay(verilog_trace_fp, "r7 %08x", r[7]);
	   $fdisplay(verilog_trace_fp, "pc %08x", pc);
	   $fdisplay(verilog_trace_fp, "inst %08x", inst);
	   $fdisplay(verilog_trace_fp, "opcode %08x", opcode);
	   $fdisplay(verilog_trace_fp, "dst %08x", dst);
	   $fdisplay(verilog_trace_fp, "src0 %08x", src0);
	   $fdisplay(verilog_trace_fp, "src1 %08x", src1);
	   $fdisplay(verilog_trace_fp, "immediate %08x", immediate);
	   $fdisplay(verilog_trace_fp, "alu0 %08x", alu0);
	   $fdisplay(verilog_trace_fp, "alu1 %08x", alu1);
	   $fdisplay(verilog_trace_fp, "aluout %08x", aluout);
	   $fdisplay(verilog_trace_fp, "cycle_counter %08x", cycle_counter);
	   $fdisplay(verilog_trace_fp, "ctl_state %08x\n", ctl_state);

	   cycle_counter <= cycle_counter + 1;
	   case (ctl_state)
	     `CTL_STATE_IDLE: begin
                pc <= 0;
	        _sram_WE <= 0;
                if (start)
                  ctl_state <= `CTL_STATE_FETCH0;
             end
	     `CTL_STATE_FETCH0: begin
	        _sram_WE <= 0;
		_sram_ADDR <= pc;
                ctl_state <= `CTL_STATE_FETCH1;
             end
	     `CTL_STATE_FETCH1: begin
		inst <= sram_DO;
                ctl_state <= `CTL_STATE_DEC0;
             end
	     `CTL_STATE_DEC0: begin
		pc <= pc + 1;
		opcode <= inst >> 25;
		dst <= (inst >> 22) & 7;
		src0 <= (inst >> 19) & 7;
		src1 <= (inst >> 16) & 7;
		immediate <= inst & {16{1'b1}};
                ctl_state <= `CTL_STATE_DEC1;
             end
	     `CTL_STATE_DEC1: begin
		alu0 <= (src0 == 1) ? immediate : r[src0];
		alu1 <= (src1 == 1) ? immediate : r[src1];
		if (opcode == `LHI)
			alu0 <= dst;
                ctl_state <= `CTL_STATE_EXEC0;
             end
	     `CTL_STATE_EXEC0: begin
	   	if (ctl_state == `LD) begin
		   _sram_ADDR <= alu1;
		end
                ctl_state <= `CTL_STATE_EXEC1;
             end
	     `CTL_STATE_EXEC1: begin
	        case (opcode)
	          `ADD: r[dst] <= aluout_wire;  
	          `SUB: r[dst] <= aluout_wire;  
	          `LSF: r[dst] <= aluout_wire; 
	          `RSF: r[dst] <= aluout_wire; 
	          `AND: r[dst] <= aluout_wire; 
	          `OR:  r[dst] <= aluout_wire; 
	          `XOR: r[dst] <= aluout_wire; 
	          `LHI: r[dst] <= aluout_wire; 

	          `LD:  begin
	        	  r[dst] <= sram_DO;
	        	end
	          `ST:  begin
	        	  _sram_DI <= alu0;
	        	  _sram_ADDR <= alu1;
	        	  _sram_WE <= 1;
	                end
	          `JLT: begin 
	          	  if (aluout_wire == 1) begin
                             r[7] <= pc - 1;
	        	     pc <= immediate;
	        	  end
	        	end
	          `JLE: begin 
	          	  if (aluout_wire == 1) begin
                             r[7] <= pc - 1;
	        	     pc <= immediate;
	        	  end
	        	end
	          `JEQ: begin 
	          	  if (aluout_wire == 1) begin
                             r[7] <= pc - 1;
	        	     pc <= immediate;
	        	  end
	        	end
	          `JNE: begin 
	          	  if (aluout_wire == 1) begin
                             r[7] <= pc - 1;
	        	     pc <= immediate;
	        	  end
	        	end
	          `JIN: begin 
	          	  if (aluout_wire == 1) begin
                             r[7] <= pc - 1;
	        	     pc <= immediate;
	        	  end
	        	end
	        endcase
                ctl_state <= (opcode == `HLT) ? `CTL_STATE_IDLE : `CTL_STATE_FETCH0;
             end
	   endcase
	   if (opcode == `HLT) begin
	      $fclose(verilog_trace_fp);
	      $writememh("verilog_sram_out.txt", top.SP.SRAM.mem);
	      $finish;
	   end
	end // !reset
     end // @posedge(clk)
endmodule // CTL
