`include "../ex5/addsub.v"

module sat_count(clk, reset, branch, taken, prediction);
   parameter N=2;
   reg [3:0] max = {N{1'b1}};
   reg [3:0] counter_o;
   wire [3:0] counter_n;

   input clk, reset, branch, taken;
   output prediction;

   addsub add0(.result(counter_n),
	       .operand_a(counter_o),
	       .operand_b(4'b1),
               .mode(~taken));

   always @(posedge clk)
     begin
	if (reset)
	  counter_o <= 0;
	else begin
          if (~branch)
            counter_o <= counter_o;
          else
	    case (taken)
               0: counter_o <= (counter_o == 0) ? 0 : counter_n;
               1: counter_o <= (counter_o == max) ? max : counter_n;
               default: counter_o <= counter_n;
	    endcase
        end
     end

    assign prediction = counter_o[N-1];
endmodule
