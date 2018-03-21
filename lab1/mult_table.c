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
	mem[1999] = 2100;
	last_addr = 2500;


	asm_cmd(ADD, 2, 0, 1, 10); // 0: i = 10
//FORI:
	asm_cmd(JEQ, 0, 2, 0, 24); // 1: if i == 0 jump HALT
	asm_cmd(ST, 0, 2, 1, 1998); // 2: store i in MEM[1998]
	asm_cmd(ADD, 3, 0, 1, 10); // 3: j = 10
//FORJ:
	asm_cmd(JEQ, 0, 3, 0, 21); // 4: if j == 0 jump ENDI
	asm_cmd(LD, 5, 0, 1, 1998); // 5: R5 = i
	asm_cmd(ADD, 6, 3, 0, 0); // 6: R6 = j
	asm_cmd(ADD, 2, 0, 0, 0); // 7: R2 = result
//LOOP:
	asm_cmd(JEQ, 0, 6, 0, 15);// 8: if b == 0 jump ENDJ
	asm_cmd(AND, 4, 6, 1, 1); // 9: R4 = b & 1
	asm_cmd(JEQ, 4, 4, 0, 12); // 10: if bit=0, jump ELSE
	asm_cmd(ADD, 2, 2, 5, 0); // 11: res += "a"
//ELSE:
	asm_cmd(LSF, 5, 5, 1, 1); // 12: a<<1
	asm_cmd(RSF, 6, 6, 1, 1); // 13: b>>1
	asm_cmd(JEQ, 0, 0, 0, 8); // 14:jump LOOP
//ENDJ:
	asm_cmd(LD, 4, 0, 1, 1999); // 15: LD addr_ptr to write
	asm_cmd(SUB, 4, 4, 1, 1); // 16:  addr_ptr--
	asm_cmd(ST, 0, 2, 4, 0); //17: store result in MEM[addr_ptr]
	asm_cmd(ST, 0, 4, 1, 1999); //18: store back addr_ptr

	asm_cmd(SUB, 3, 3, 1, 1); // 19: j--
	asm_cmd(JIN, 0, 0, 0, 4); // 20: jump FORJ
//ENDI:
	asm_cmd(LD, 2, 0, 1, 1998); // 21: load back i from memory
	asm_cmd(SUB, 2, 2, 1, 1); // 22: i--
	asm_cmd(JIN, 0, 0, 0, 1); // 23: jump FORI
//HALT:
	asm_cmd(HLT, 0, 0, 0, 0); // 24: halt
	
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
