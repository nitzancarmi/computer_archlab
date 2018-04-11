`include "../ex4/add4.v"

module	addsub( result, operand_a, operand_b, mode );

	input	[3:0]	operand_a, operand_b;
	input	mode;
	output	[3:0]	result;
	wire co;
	wire	[3:0]	operand_b_modified;

	add4	m1(
		.sum(result),
		.co(co),
		.a(operand_a),	 
		.b((mode) ? ~operand_b : operand_b),	
		.ci(mode)
	);	
endmodule
