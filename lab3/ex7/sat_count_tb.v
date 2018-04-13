module main;
   reg clk, reset, branch, taken;
   wire prediction;
   reg correct;

   sat_count uut(.clk(clk),
                 .reset(reset),
                 .branch(branch),
                 .taken(taken),
                 .prediction(prediction));

   always #5 clk = ~clk;

   initial
     begin
        $dumpfile("waves.vcd");
        $dumpvars;
        correct = 1;
        clk = 0;
        reset = 1;
        branch = 0;
        taken = 0;
        //prediction = 0;

	//reset really reset
        #10;
	reset = 0;
        correct = correct & (~prediction);
        if (correct)
		$display("reset works");

	//~branch -> no update
        #10;
        taken = 1;
        #20;
        correct = correct & (~prediction);
        if (correct)
		$display("!branch blocks");

	//branch -> update
        #10;
        branch = 1;
        taken = 1;
        #10;
        correct = correct & (~prediction);
        #10;
        correct = correct & (prediction);
        #10;
        correct = correct & (prediction);
        #20; //no overflow!
        correct = correct & (prediction);
        if (correct)
		$display("add to taken works");

        //subtraction
        #10;
        branch = 1;
        taken = 0;
        #10;
        correct = correct & (prediction);
        #10;
        correct = correct & (~prediction);
        #10;
        correct = correct & (~prediction);
        #20; //no underflow!
        correct = correct & (~prediction);
        if (correct)
		$display("sub to ~taken works");

        
	if (correct)
	  $display("all tests PASSED!");
        $finish;
     end // initial begin
endmodule
