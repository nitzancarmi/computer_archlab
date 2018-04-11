module parity(clk, in, reset, out);

   input clk, in, reset;
   output out;

   reg 	  state;

   parameter zero=0, one=1;

   always @(posedge clk)
     begin
	if (reset)
	  state <= zero;
	else
	  case (state)
          0: state <= in;
          1: state <= ~in;
          default: state <= 0;
	  endcase
     end

  assign out = state;

endmodule
