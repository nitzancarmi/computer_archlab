module sat_count(clk, reset, branch, taken, prediction);
   parameter N=2;
   parameter counter_o[N-1:0];
   parameter counter_n[N-1:0];

   input clk, reset, branch, taken;
   output prediction;

   addsub add1(.result(counter_n),
	       .operand_a(counter_o),
	       .operand_b(1),
               .mode(~taken));

   always @(posedge clk)
     begin
	if (reset)
	  counter <= 0;
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
