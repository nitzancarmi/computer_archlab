module main;
reg a,b;
wire sum, carry;
reg correct;
halfadder uut(a, b, sum, carry);

always@(sum or carry)
begin
$display("time=%d:%b + %b = %b, carry = %b\n", $time, a, b, sum, carry);
end

initial
begin
#1;
correct = 1;
$dumpfile("waves.vcd");
$dumpvars;
a = 0; b = 0;
#5 correct = correct & (a+b == {carry, sum});
a = 0; b = 1;
#5 correct = correct & (a+b == {carry, sum});
a = 1; b = 0;
#5 correct = correct & (a+b == {carry, sum});
a = 1; b = 1;
#5 correct = correct & (a+b == {carry, sum});
$finish;
end
endmodule
