/*
 * SP ASM: Simple Processor assembler
 *
 * usage: asm
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <linux/types.h>

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

#define TRACE_FILE "mult_table_trace.txt"
#define SRAM_OUT_FILE "mult_table_sram_out.txt"


#define MEM_SIZE_BITS	(16)
#define MEM_SIZE	(1 << MEM_SIZE_BITS)
#define MEM_MASK	(MEM_SIZE - 1)

unsigned int MEM[MEM_SIZE];
unsigned int n = 0;
unsigned int REG[8];

FILE *trace, *sram_out;

struct inst {
	uint32_t pc;
	uint32_t data;
	uint8_t opc;
	uint8_t dst;
	uint8_t src0;
	uint8_t src1;
	uint16_t imm;	
};

struct inst insts[MEM_SIZE];

void parse_instructions(FILE *fp)
{
	int i;
	unsigned int mem;
	struct inst* inst;

	while(fscanf(fp, "%x\n", &mem) != EOF)
		MEM[n++] = mem;

	for(i = 0; i < n; i++) {
		mem = MEM[i];
		inst = &insts[i];
		inst->data = mem;
		inst->pc = i;
		inst->opc = mem >> 25;
		inst->dst = (mem >> 22) & 7;
		inst->src0 = (mem >> 19) & 7;
		inst->src1 = (mem >> 16) & 7;
		inst->imm = mem & 0xffff;
		if (inst->opc == HLT)
			break;
	}
}

void dump_memory()
{
	int i;
	
	for(i = 0; i < n; i++)
		fprintf(sram_out, "%08x\n", MEM[i]);
}

const char* opcode_tostring(int opc) 
{
	switch (opc) 
	{
	   case ADD: return "ADD";
	   case SUB: return "SUB";
	   case LSF: return "LSF";
	   case RSF: return "RSF";
	   case AND: return "AND";
	   case OR: return "OR";
	   case XOR: return "XOR";
	   case LHI: return "LHI";
	   case LD: return "LD";
	   case ST: return "ST";
	   case JLT: return "JLT";
	   case JLE: return "JLE";
	   case JEQ: return "JEQ";
	   case JNE: return "JNE";
	   case JIN: return "JIN";
	   case HLT: return "HLT";
	}

	return "UNKNOWN";
}

void dump_trace(struct inst* inst, int inst_cnt)
{
	fprintf(trace, "--- instruction %d (%04x) @ PC %d (%04x) -----------------------------------------------------------\n",
		inst_cnt, inst_cnt, inst->pc, inst->pc);

	fprintf(trace, "pc = %04x, inst = %08x, opcode = %d (%s), dst = %d, src0 = %d, src1 = %d, immediate = %08x\n",
		inst->pc, inst->data, inst->opc, opcode_tostring(inst->opc), inst->dst, inst->src0, inst->src1, inst->imm);

	fprintf(trace, "r[0] = %08x r[1] = %08x r[2] = %08x r[3] = %08x\nr[4] = %08x r[5] = %08x r[6] = %08x r[7] = %08x\n",
		REG[0],REG[1],REG[2],REG[3],REG[4],REG[5],REG[6],REG[7]);
}

void execute_instructions() {
	int pc = 0, inst_cnt = 0;
	struct inst* inst;
	unsigned int a, b;

	while (true) {
		inst = &insts[pc++];
		REG[1] = inst->imm;
		dump_trace(inst, inst_cnt);
		a = REG[inst->src0];
		b = REG[inst->src1];
		switch(inst->opc) {
		case ADD:
			REG[inst->dst] = a + b;
			break;
		case SUB:
			REG[inst->dst] = a - b;
			break;
		case LSF:
			REG[inst->dst] = a << b;
			break;
		case RSF:
			REG[inst->dst] = a >> b;
			break;
		case AND:
			REG[inst->dst] = a & b;
			break;
		case OR:
			REG[inst->dst] = a | b;
			break;
		case XOR:
			REG[inst->dst] = a ^ b;
			break;
		case LHI:
			REG[inst->dst] = (inst->imm << 16) + (REG[inst->dst] && 0xffff);
			break;

		case LD:
			REG[inst->dst] = MEM[b];
			break;
		case ST:
			MEM[b] = a;
			break;

		case JLT:
			if (a < b) {
				REG[7] = pc - 1;
				pc = inst->imm;
			}
			break;
		case JLE:
			if (a <= b) {
				REG[7] = pc - 1;
				pc = inst->imm;
			}
			break;
		case JEQ:
			if (a == b) {
				REG[7] = pc - 1;
				pc = inst->imm;
			}
			break;
		case JNE:
			if (a != b) {
				REG[7] = pc - 1;
				pc = inst->imm;
			}
			break;
		case JIN:
			REG[7] = pc - 1;
			pc = inst->imm;
			break;
		case HLT:
			pc--;
			inst_cnt++;
			fprintf(trace, "\n>>>>EXEC: HALT at PC %04x <<<<\n",
				pc);
			fprintf(trace, "sim finished at pc %d, %d instructions",
				pc, inst_cnt);
			return;
		default:
			printf("Unknown opcode %d\n", inst->opc);
			continue;
		}
		
		switch(inst->opc) {
		case ADD:
		case SUB:
		case LSF:
		case RSF:
		case AND:
		case OR:
		case XOR:
		case LHI:
			fprintf(trace, "\n>>>>EXEC: R[%d] = %d %s %d <<<<\n\n",
				inst->dst, REG[inst->dst], opcode_tostring(inst->opc), inst->opc);
			break;
		case LD:
			fprintf(trace, "\n>>>>EXEC: R[%d] = MEM[%d] = %08x <<<<\n\n",
				inst->dst, b, MEM[b]);
			REG[inst->dst] = MEM[b];
			break;
		case ST:
			fprintf(trace, "\n>>>>EXEC: MEM[%d] = %08x %s %d <<<<\n\n",
				b, MEM[b], "ST", inst->opc);
			MEM[b] = a;
			break;

		case JLT:
		case JLE:
		case JEQ:
		case JNE:
		case JIN:
			fprintf(trace, "\n>>>>EXEC: %s %d, %d, %d <<<<\n\n",
				opcode_tostring(inst->opc), a, b, pc);
			break;
		default:
			printf("Unknown opcode\n");
			continue;
		}

		inst_cnt++;
	}
}

int main(int argc, char *argv[])
{
	FILE *fp;

	if (argc != 2){
		printf("usage: iss bin_file\n");
		return -1;
	}

	fp = fopen(argv[1], "r");
	if (fp == NULL) {
		printf("couldn't open file %s\n", argv[1]);
		exit(1);
	}

	trace = fopen(TRACE_FILE, "wb");
	if (!trace) {
		printf("couldn't open file %s\n", TRACE_FILE);
		exit(1);
	}

	sram_out = fopen(SRAM_OUT_FILE, "wb");
	if (!sram_out) {
		printf("couldn't open file %s\n", SRAM_OUT_FILE);
		exit(1);
	}

	parse_instructions(fp);
	execute_instructions();
	dump_memory();

	fclose(fp);
	fclose(trace);
	fclose(sram_out);

	return 0;
}
