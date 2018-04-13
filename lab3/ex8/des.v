module des(clk, in, reset, out);

   parameter WIDTH=8;
   
   input clk, in, reset;
   output [WIDTH-1:0] out;
   reg [WIDTH-1:0] out_next = 0;
   reg [WIDTH-1:0] _out = 0;
   reg [WIDTH-2:0] timer = 1;

   always @(posedge clk)
     begin
        out_next <= (out_next << 1) | in;
//        $display("next out %b (in %b)", out_next, in);
        timer <= timer << 1;

	if (reset)
	  _out <= 0;
	else
           _out <= (timer == 0) ? out_next : out;

        if (timer == 0) begin
//	   $display("assign new out: %b", out_next);
           timer <= 1;
        end

     end

     assign out = _out;

endmodule
