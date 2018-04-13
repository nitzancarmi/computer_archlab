`include "../ex5/addsub.v"

module sat_count(clk, reset, branch, taken, prediction);
   parameter N=2;
   reg [N-1:0] counter_o = 0;
   wire [N-1:0] counter_n;
   wire [3:0] counter_n4 = { {(4-N){counter_n[N-1]}}, counter_n[N-1:0] };

   input clk, reset, branch, taken;
   output prediction;

   addsub add0(.result(counter_n4),
	       .operand_a({ {(4-N){counter_o[N-1]}}, counter_o[N-1:0] }),
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
               1: counter_o <= (counter_o == -1) ? -1 : counter_n;
               default: counter_o <= counter_n;
	    endcase
        end
     end

    assign prediction = counter_o[N-1];
endmodule
