module main;

reg a, b, ci, correct, loop_skipped;
wire sum, co;
integer ai, bi, cii;
fulladder uut(a, b, ci, sum, co);

initial begin
correct = 1; loop_skipped= 1;
$dumpfile("waves.vcd");
$dumpvars;
#1
for( ai=0; ai<=1; ai=ai+1 ) begin
  for( bi=0; bi<=1; bi=bi+1 ) begin
    for( cii=0; cii<=1; cii=cii+1 ) begin
      loop_skipped= 0;
      a = ai[0]; b = bi[0]; ci = cii[0];
      #5 correct = correct & (a + b + ci == {co,sum});
    end
  end
end

if (correct && ~loop_skipped)
  $display("Test Passed -%m");
else
  $display("Test Failed -%m");
$finish;
end

endmodule
