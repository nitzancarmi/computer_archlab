module main;
   reg in, clk, reset, correct;
   reg [7:0] buff;
   wire [7:0] out;

   des #(8) uut (clk, in, reset, out);

   always #5 clk = ~clk;

   initial
     begin
        #1;
        $dumpfile("waves.vcd");
        $dumpvars;
        correct = 1;
        clk = 0;
        reset = 0;

        in = 1;
	#40;
        in = 0;
	#40;
        #10;
	//$display("out %d", out);
        correct = correct & (out == 8'b01111000);
        if (correct)
		$display("simple test works");
        in = 1;

        //#2
	#10;
	//$display("out %d", out);
	#10;
	//$display("out %d", out);
	#10;
	reset = 1;
	//$display("out %d", out);
	#10;
        correct = correct & (out == 8'b00000000);
        if (correct)
		$display("reset works");
        reset = 0;

	//$display("out %d", out);
	#10;
	//$display("out %d", out);
	#10;
	//$display("out %d", out);
	#10;
	//$display("out %d", out);
	#10;
	//$display("out %d", out);
	#10;
        correct = correct & (out == 8'b00111111);
        if (correct)
		$display("after reset works");
	//$display("out %d", out);


	if (correct)
	  $display("all tests PASSED!");
        $finish;
     end // initial begin
endmodule
