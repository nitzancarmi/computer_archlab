module fulladder(a, b, ci, sum, co);
  input   a, b, ci;
  output  sum, co;

  assign sum = a^b^ci;
  assign co = (a&b) | (a&ci) | (b&ci);
endmodule
