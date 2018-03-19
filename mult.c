/*
 * SP ASM: Simple Processor assembler
 *
 * usage: asm
 */
#include <stdio.h>
#include <stdlib.h>

#define ADD 0
#define SUB 1
#define LSF 2
#define RSF 3
#define AND 4
#define OR  5
#define XOR 6
#define LHI 7
#define LD 8
#define ST 9
#define JLT 16
#define JLE 17
#define JEQ 18
#define JNE 19
#define JIN 20
#define HLT 24

#define MEM_SIZE_BITS	(16)
#define MEM_SIZE	(1 << MEM_SIZE_BITS)
#define MEM_MASK	(MEM_SIZE - 1)
unsigned int mem[MEM_SIZE];

int pc = 0;

static void asm_cmd(int opcode, int dst, int src0, int src1, int immediate)
{
	int inst;

	inst = ((opcode & 0x1f) << 25) | ((dst & 7) << 22) | ((src0 & 7) << 19) | ((src1 & 7) << 16) | (immediate & 0xffff);
	mem[pc++] = inst;
}

static void assemble_program(char *program_name)
{
	FILE *fp;
	int addr, last_addr;

	for (addr = 0; addr < MEM_SIZE; addr++)
		mem[addr] = 0;

	pc = 0;
	last_addr = 1100;


	asm_cmd(ADD, 2, 0, 1, 1000); // 0: R5 = MEM[1000] (= a)
	asm_cmd(ADD, 3, 0, 1, 1001); // 1: R5 = MEM[1000] (= a)
	asm_cmd(ST, 0, 1, 2, 63); // 2: R5 = MEM[1000] (= a)
	asm_cmd(ST, 0, 1, 3, 31); // 3: R6 = MEM[1001] (= b)
	/*
	 * Program starts here
	 */
	asm_cmd(LD, 5, 0, 1, 1000); // 4: R5 = MEM[1000] (= a)
	asm_cmd(LD, 6, 0, 1, 1001); // 5: R6 = MEM[1001] (= b)
	asm_cmd(ADD, 2, 0, 0, 0); // 6: R2 = result
	asm_cmd(JEQ, 0, 6, 0, 14);// 7: if b == 0 --> jump to end of iteration
//	{
	asm_cmd(AND, 4, 6, 1, 1); // 8: R4 = b & 1
	asm_cmd(JEQ, 4, 4, 0, 11); // 9: if bit=0, finish iteration
	asm_cmd(ADD, 2, 2, 5, 0); // 10: res += "a"
//	}
	asm_cmd(LSF, 5, 5, 1, 1); // 11: a<<1
	asm_cmd(RSF, 6, 6, 1, 1); // 12: b>>1
	asm_cmd(JEQ, 0, 0, 0, 7); // 13:jump back to Line 8
	asm_cmd(ST, 0, 2, 1, 1002); //14: R6 = MEM[1001] (= b)
	asm_cmd(HLT, 0, 0, 0, 0); // 15: halt
	
	/* 
	 * Constants are planted into the memory somewhere after the program code:
	 */
//	for (i = 0; i < 8; i++)
//		mem[15+i] = i;

	fp = fopen(program_name, "w");
	if (fp == NULL) {
		printf("couldn't open file %s\n", program_name);
		exit(1);
	}
	addr = 0;
	while (addr < last_addr) {
		fprintf(fp, "%08x\n", mem[addr]);
		addr++;
	}
}


int main(int argc, char *argv[])
{
	
	if (argc != 2){
		printf("usage: asm program_name\n");
		return -1;
	}else{
		assemble_program(argv[1]);
		printf("SP assembler generated machine code and saved it as %s\n", argv[1]);
		return 0;
	}
	
}
