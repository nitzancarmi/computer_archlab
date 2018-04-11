`include "../ex3/fulladder.v"

module add4(a, b, ci, sum, co);

  input   [3:0] a, b;
  input   ci;
  output  [3:0] sum;
  output  co;
  wire     [3:0] citmp;
  
  assign citmp[0] = ci;
  fulladder fa0(a[0], b[0], citmp[0], sum[0], citmp[1]);
  fulladder fa1(a[1], b[1], citmp[1], sum[1], citmp[2]);
  fulladder fa2(a[2], b[2], citmp[2], sum[2], citmp[3]);
  fulladder fa3(a[3], b[3], citmp[3], sum[3], co);

endmodule
