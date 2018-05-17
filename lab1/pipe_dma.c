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
#define DMA 10
#define POL 11
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
	last_addr = 1300;


	for (addr = 0; addr < 30; addr++)
		mem[1000 + addr] = addr;

	asm_cmd(ADD, 2, 0, 1, 1000); // 0: R[2] = 1000
	asm_cmd(ADD, 3, 0, 1, 1200); // 1: R[3] = 1200
	asm_cmd(DMA, 0, 2, 3, 30); // 2: DMA from addr 1000 to 1200 of 30 words

	asm_cmd(ADD, 3, 0, 1, 1100); // 3: R[3] = 1100 (main write addr to copy to)
	asm_cmd(ADD, 4, 0, 1, 10); // 4: R[4] = 10 (main #words to copy)
//LOOP:
	asm_cmd(JEQ, 0, 4, 0, 14); // 5: if R4 == 0 --> goto END
	asm_cmd(LD, 5, 0, 2, 0); // 6: R[5] = MEM[R[2]]
	asm_cmd(ADD, 5, 5, 1, 100); // 7: R[5] += 100
	asm_cmd(ST, 0, 5, 3, 0); // 8: MEM[R[3]] = R[5]
	asm_cmd(ADD, 2, 2, 1, 1); // 9: R[2]++
	asm_cmd(ADD, 3, 3, 1, 1); // 10: R[3]++
	asm_cmd(SUB, 4, 4, 1, 1); // 11: R[4]--
	asm_cmd(POL, 6, 0, 0, 0); // 12: Poll DMA
	asm_cmd(JIN, 0, 1, 0, 5); // 13: goto LOOP

//END:
	asm_cmd(POL, 6, 0, 0, 0); // 14: Poll DMA
	asm_cmd(ADD, 2, 0, 0, 0); // 15: R[2] = 0
	asm_cmd(JNE, 0, 6, 0, 18); // 16: if DMA finished --> goto ELSE
	asm_cmd(ADD, 2, 0, 1, 1); // 17: R[2] = 1
//ELSE:
	asm_cmd(HLT, 0, 0, 0, 0); // 18: HALT



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
