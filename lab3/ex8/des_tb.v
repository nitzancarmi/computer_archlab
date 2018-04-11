module main;
   reg in, clk, reset;
   reg [7:0] buff;
   wire [7:0] out;

   des uut (clk, in, reset, out);

   always #5 clk = ~clk;

   // Fill Here
endmodule
