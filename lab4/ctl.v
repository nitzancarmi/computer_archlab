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
   reg [15:0] sram_ADDR;
   reg [31:0] sram_DI;

//DMA registers
   reg [15:0] dma_raddr;
   reg [15:0] dma_waddr;
   reg [15:0] dma_reg;
   reg [15:0] dma_cnt;
   reg [1:0]  dma_state;
   wire       dma_enable;
   reg        dma_enable_o;


   integer 	 verilog_trace_fp, rc;

   initial
     begin
	verilog_trace_fp = $fopen("verilog_trace.txt", "w");
     end

     assign sram_EN = (ctl_state == `CTL_STATE_FETCH0) | ((ctl_state == `CTL_STATE_EXEC0) & (opcode == `LD)) | 
	              ((ctl_state == `CTL_STATE_EXEC1) & (opcode == `ST));
     assign sram_WE = ((ctl_state == `CTL_STATE_EXEC1) & (opcode == `ST));

     assign dma_enable = ~((ctl_state == `CTL_STATE_FETCH0) |
			   (ctl_state == `CTL_STATE_EXEC0 & opcode == `LD) |
			   (ctl_state == `CTL_STATE_EXEC1 & opcode == `ST));

     always @(ctl_state, opcode, pc, alu1, alu0)
       begin
          if (ctl_state == `CTL_STATE_FETCH0)
		  sram_ADDR <= pc;
	  if ((ctl_state == `CTL_STATE_EXEC0) & (opcode == `LD))
		  sram_ADDR <= alu1;
	  if ((ctl_state == `CTL_STATE_EXEC1) & (opcode == `ST))
	  begin
		  sram_ADDR <= alu1;
	          sram_DI <= alu0;
	  end

          if (dma_enable)
          begin
		case(dma_state)
		    `DMA_STATE_READ: sram_ADDR <= dma_raddr;
		    `DMA_STATE_WRITE:
                    begin
		        sram_ADDR <= dma_waddr;
	                sram_DI <= dma_reg;
                    end
		endcase
	  end
       end

   // synchronous instructions
   always@(posedge clk)
     begin
        dma_enable_o <= dma_enable;

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

	   $fdisplay(verilog_trace_fp, "dma_raddr %08x\n", dma_raddr);
	   $fdisplay(verilog_trace_fp, "dma_reg %08x\n", dma_reg);
	   $fdisplay(verilog_trace_fp, "dma_waddr %08x\n", dma_waddr);
	   $fdisplay(verilog_trace_fp, "dma_cnt %08x\n", dma_cnt);
	   $fdisplay(verilog_trace_fp, "dma_state %08x\n\n", dma_state);

	   cycle_counter <= cycle_counter + 1;
	   case (ctl_state)
	     `CTL_STATE_IDLE: begin
                pc <= 0;
                if (start)
                  ctl_state <= `CTL_STATE_FETCH0;
             end
	     `CTL_STATE_FETCH0: begin
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
	        case (opcode)
	          `ADD: aluout <= aluout_wire; 
	          `SUB: aluout <= aluout_wire; 
	          `LSF: aluout <= aluout_wire;
	          `RSF: aluout <= aluout_wire;
	          `AND: aluout <= aluout_wire;
	          `OR:  aluout <= aluout_wire;
	          `XOR: aluout <= aluout_wire;
	          `LHI: aluout <= aluout_wire;

	          `JLT: aluout <= aluout_wire; 
	          `JLE: aluout <= aluout_wire; 
	          `JEQ: aluout <= aluout_wire; 
	          `JNE: aluout <= aluout_wire; 
	          `JIN: aluout <= aluout_wire; 
		  `DMA: begin
			    if (dma_state == `DMA_STATE_IDLE)
                            begin
			    	dma_raddr = alu0;
			    	dma_waddr = alu1;
			    	dma_cnt = immediate;
			    	dma_state = `DMA_STATE_READ;
			    end
			end
	        endcase
                ctl_state <= `CTL_STATE_EXEC1;
             end
	     `CTL_STATE_EXEC1: begin
	        case (opcode)
	          `ADD: r[dst] <= aluout;  
	          `SUB: r[dst] <= aluout;  
	          `LSF: r[dst] <= aluout; 
	          `RSF: r[dst] <= aluout; 
	          `AND: r[dst] <= aluout; 
	          `OR:  r[dst] <= aluout; 
	          `XOR: r[dst] <= aluout; 
	          `LHI: r[dst] <= aluout; 

	          `LD:  begin
	        	  r[dst] <= sram_DO;
	        	end
	          `JLT: begin 
	          	  if (aluout == 1) begin
                             r[7] <= pc - 1;
	        	     pc <= immediate;
	        	  end
	        	end
	          `JLE: begin 
	          	  if (aluout == 1) begin
                             r[7] <= pc - 1;
	        	     pc <= immediate;
	        	  end
	        	end
	          `JEQ: begin 
	          	  if (aluout == 1) begin
                             r[7] <= pc - 1;
	        	     pc <= immediate;
	        	  end
	        	end
	          `JNE: begin 
	          	  if (aluout == 1) begin
                             r[7] <= pc - 1;
	        	     pc <= immediate;
	        	  end
	        	end
	          `JIN: begin 
	          	  if (aluout == 1) begin
                             r[7] <= pc - 1;
	        	     pc <= immediate;
	        	  end
	        	end
	          `POL: r[dst] <= dma_cnt; 
	        endcase
                ctl_state <= (opcode == `HLT) ? `CTL_STATE_IDLE : `CTL_STATE_FETCH0;
	   	if (opcode == `HLT) begin
	   	   $fclose(verilog_trace_fp);
	   	   $writememh("verilog_sram_out.txt", top.SP.SRAM.mem);
	   	   $finish;
	   	end
             end
	   endcase //ctl_state

           if (dma_enable_o)
           begin
          	case(dma_state)
          	    `DMA_STATE_READ:
                        begin
          	    	if (dma_enable)
          	    		dma_state = `DMA_STATE_SAMPLE;
                        end
          	    `DMA_STATE_SAMPLE:
                        begin
          	    	dma_reg <= sram_DO;
          	    	dma_raddr <= dma_raddr + 1;
          	    	dma_state <= `DMA_STATE_WRITE;
                        end
          	    `DMA_STATE_WRITE:
                        begin
          	    	dma_waddr <= dma_waddr + 1;
          	    	dma_cnt <= dma_cnt - 1;
          	    	dma_state <= (dma_cnt == 1) ? `DMA_STATE_IDLE : `DMA_STATE_READ;
                        end
          	endcase
           end

	end // !reset
     end // @posedge(clk)
endmodule // CTL
