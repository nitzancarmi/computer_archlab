#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

#include "llsim.h"

#define sp_printf(a...)						\
	do {							\
		llsim_printf("sp: clock %d: ", llsim->clock);	\
		llsim_printf(a);				\
	} while (0)

int nr_simulated_instructions = 0;
FILE *inst_trace_fp = NULL, *cycle_trace_fp = NULL;

typedef struct sp_registers_s {
	// 6 32 bit registers (r[0], r[1] don't exist)
	int r[8];

	// 16 bit program counter
	int pc;

	// 32 bit instruction
	int inst;

	// 5 bit opcode
	int opcode;

	// 3 bit destination register index
	int dst;

	// 3 bit source #0 register index
	int src0;

	// 3 bit source #1 register index
	int src1;

	// 32 bit alu #0 operand
	int alu0;

	// 32 bit alu #1 operand
	int alu1;

	// 32 bit alu output
	int aluout;

	// 32 bit immediate field (original 16 bit sign extended)
	int immediate;

	// 32 bit cycle counter
	int cycle_counter;

	// 3 bit control state machine state register
	int ctl_state;

	// DMA registers
	int dma_raddr;
	int dma_waddr;
	int dma_reg;
	int dma_cnt;
	int dma_state;

	// control states
	#define CTL_STATE_IDLE		0
	#define CTL_STATE_FETCH0	1
	#define CTL_STATE_FETCH1	2
	#define CTL_STATE_DEC0		3
	#define CTL_STATE_DEC1		4
	#define CTL_STATE_EXEC0		5
	#define CTL_STATE_EXEC1		6

	#define DMA_STATE_IDLE		0
	#define DMA_STATE_READ		1
	#define DMA_STATE_SAMPLE	2
	#define DMA_STATE_WRITE		3
} sp_registers_t;

/*
 * Master structure
 */
typedef struct sp_s {
	// local sram
#define SP_SRAM_HEIGHT	64 * 1024
	llsim_memory_t *sram;

	unsigned int memory_image[SP_SRAM_HEIGHT];
	int memory_image_size;

	sp_registers_t *spro, *sprn;
	
	int start;
} sp_t;

static void sp_reset(sp_t *sp)
{
	sp_registers_t *sprn = sp->sprn;

	memset(sprn, 0, sizeof(*sprn));
}

/*
 * opcodes
 */
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
#define JLT 16
#define JLE 17
#define JEQ 18
#define JNE 19
#define JIN 20
#define HLT 24

int dma_enable(sp_registers_t *spro) {
	return !(spro->ctl_state == CTL_STATE_FETCH0 ||
			 (spro->ctl_state == CTL_STATE_EXEC0 && spro->opcode == LD) ||
			 (spro->ctl_state == CTL_STATE_EXEC1 && spro->opcode == ST));
}

static void dump_sram(sp_t *sp)
{
	FILE *fp;
	int i;

	fp = fopen("sram_out.txt", "w");
	if (fp == NULL) {
                printf("couldn't open file sram_out.txt\n");
                exit(1);
	}
	for (i = 0; i < SP_SRAM_HEIGHT; i++)
		fprintf(fp, "%08x\n", llsim_mem_extract(sp->sram, i, 31, 0));
	fclose(fp);
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

	   case DMA: return "DMA";
	   case POL: return "POL";
	}

	return "UNKNOWN";
}

void dump_trace(sp_registers_t *spro, sp_registers_t *sprn)
{
	int inst_cnt = spro->cycle_counter / 6 - 1;
	FILE *inst_trace;

	inst_trace = fopen("inst_trace.txt", "a+");
	if (!inst_trace) {
		printf("couldn't open file inst_trace.txt\n");
		exit(1);
	}

	fprintf(inst_trace, "--- instruction %d (%04x) @ PC %d (%04x) -----------------------------------------------------------\n",
		inst_cnt, inst_cnt, spro->pc - 1, spro->pc - 1);

	fprintf(inst_trace, "pc = %04x, inst = %08x, opcode = %d (%s), dst = %d, src0 = %d, src1 = %d, immediate = %08x\n",
		spro->pc - 1, spro->inst, spro->opcode, opcode_tostring(spro->opcode), spro->dst, spro->src0, spro->src1, spro->immediate);

	fprintf(inst_trace, "r[0] = %08x r[1] = %08x r[2] = %08x r[3] = %08x\nr[4] = %08x r[5] = %08x r[6] = %08x r[7] = %08x\n",
		spro->r[0],spro->r[1],spro->r[2],spro->r[3],spro->r[4],spro->r[5],spro->r[6],spro->r[7]);


	if (spro->opcode == HLT) {
		fprintf(inst_trace, "\n>>>>EXEC: HALT at PC %04x <<<<\n",
			sprn->pc - 1);
		fprintf(inst_trace, "sim finished at pc %d, %d instructions",
			spro->pc - 1, inst_cnt + 1);
		return;
	}

	switch(spro->opcode) {
	case ADD:
	case SUB:
	case LSF:
	case RSF:
	case AND:
	case OR:
	case XOR:
	case LHI:
		fprintf(inst_trace, "\n>>>>EXEC: R[%d] = %d %s %d <<<<\n\n",
			sprn->dst, sprn->r[spro->dst], opcode_tostring(sprn->opcode), sprn->opcode);
		break;
	case LD:
		fprintf(inst_trace, "\n>>>>EXEC: R[%d] = MEM[%d] = %08x <<<<\n\n",
			sprn->dst, sprn->r[sprn->src1], sprn->r[spro->dst]);
		break;
	case ST:
		fprintf(inst_trace, "\n>>>>EXEC: MEM[%d] = %08x %s %d <<<<\n\n",
			sprn->alu1, sprn->alu0, "ST", sprn->opcode);
		break;

	case JLT:
	case JLE:
	case JEQ:
	case JNE:
	case JIN:
		fprintf(inst_trace, "\n>>>>EXEC: %s %d, %d, %d <<<<\n\n",
			opcode_tostring(sprn->opcode), sprn->alu0, sprn->alu1, sprn->pc);
		break;

	case POL:
		fprintf(inst_trace, "\n>>>>EXEC: R[%d] = %d %s %d <<<<\n\n",
			sprn->dst, sprn->dma_cnt, opcode_tostring(sprn->opcode), sprn->opcode);
		break;
	case DMA:
		fprintf(inst_trace, "\n>>>>EXEC: %s Read = %d, Write = %d, count = %d <<<<\n\n",
			opcode_tostring(sprn->opcode), sprn->dma_raddr, sprn->dma_waddr, sprn->dma_cnt);
		break;


	default:
		printf("Unknown opcode\n");
	}

	fclose(inst_trace);
}


static void sp_ctl(sp_t *sp)
{
	sp_registers_t *spro = sp->spro;
	sp_registers_t *sprn = sp->sprn;
	int i;

	// sp_ctl

	fprintf(cycle_trace_fp, "cycle %d\n", spro->cycle_counter);
	for (i = 2; i <= 7; i++)
		fprintf(cycle_trace_fp, "r%d %08x\n", i, spro->r[i]);
	fprintf(cycle_trace_fp, "pc %08x\n", spro->pc);
	fprintf(cycle_trace_fp, "inst %08x\n", spro->inst);
	fprintf(cycle_trace_fp, "opcode %08x\n", spro->opcode);
	fprintf(cycle_trace_fp, "dst %08x\n", spro->dst);
	fprintf(cycle_trace_fp, "src0 %08x\n", spro->src0);
	fprintf(cycle_trace_fp, "src1 %08x\n", spro->src1);
	fprintf(cycle_trace_fp, "immediate %08x\n", spro->immediate);
	fprintf(cycle_trace_fp, "alu0 %08x\n", spro->alu0);
	fprintf(cycle_trace_fp, "alu1 %08x\n", spro->alu1);
	fprintf(cycle_trace_fp, "aluout %08x\n", spro->aluout);
	fprintf(cycle_trace_fp, "cycle_counter %08x\n", spro->cycle_counter);
	fprintf(cycle_trace_fp, "ctl_state %08x\n", spro->ctl_state);

	fprintf(cycle_trace_fp, "dma_raddr %08x\n", spro->dma_raddr);
	fprintf(cycle_trace_fp, "dma_reg %08x\n", spro->dma_reg);
	fprintf(cycle_trace_fp, "dma_waddr %08x\n", spro->dma_waddr);
	fprintf(cycle_trace_fp, "dma_cnt %08x\n", spro->dma_cnt);
	fprintf(cycle_trace_fp, "dma_state %08x\n\n", spro->dma_state);

	sprn->cycle_counter = spro->cycle_counter + 1;

	switch (spro->ctl_state) {
	case CTL_STATE_IDLE:
		sprn->pc = 0;
		if (sp->start)
			sprn->ctl_state = CTL_STATE_FETCH0;
		break;

	case CTL_STATE_FETCH0:
		llsim_mem_read(sp->sram, spro->pc);
		sprn->ctl_state = CTL_STATE_FETCH1;
		break;

	case CTL_STATE_FETCH1:
		sprn->inst = llsim_mem_extract_dataout(sp->sram, 31, 0);
		sprn->ctl_state = CTL_STATE_DEC0;
		break;

	case CTL_STATE_DEC0:
		sprn->pc = spro->pc + 1;
		sprn->opcode = spro->inst >> 25;
		sprn->dst = (spro->inst >> 22) & 7;
		sprn->src0 = (spro->inst >> 19) & 7;
		sprn->src1 = (spro->inst >> 16) & 7;
		sprn->immediate = spro->inst & 0xffff;
		sprn->ctl_state = CTL_STATE_DEC1;
		break;

	case CTL_STATE_DEC1:
		sprn->r[1] = spro->immediate;
		sprn->alu0 = (spro->src0 == 1) ? spro->immediate : spro->r[spro->src0];
		sprn->alu1 = (spro->src1 == 1) ? spro->immediate : spro->r[spro->src1];
		if (spro->opcode == LHI)
			sprn->alu0 = spro->dst;
		sprn->ctl_state = CTL_STATE_EXEC0;
		break;

	case CTL_STATE_EXEC0:
		switch(spro->opcode) {
		case ADD:
			sprn->aluout = spro->alu0 + spro->alu1;
			break;
		case SUB:
			sprn->aluout = spro->alu0 - spro->alu1;
			break;
		case LSF:
			sprn->aluout = spro->alu0 << spro->alu1;
			break;
		case RSF:
			sprn->aluout = spro->alu0 >> spro->alu1;
			break;
		case AND:
			sprn->aluout = spro->alu0 & spro->alu1;
			break;
		case OR:
			sprn->aluout = spro->alu0 | spro->alu1;
			break;
		case XOR:
			sprn->aluout = spro->alu0 ^ spro->alu1;
			break;
		case LHI:
			sprn->aluout = (spro->immediate << 16) + (spro->alu0 && 0xffff);
			break;

		case LD:
			llsim_mem_read(sp->sram, spro->alu1);
			break;
		case ST:
			break;

		case JLT:
			sprn->aluout = (spro->alu0 < spro->alu1);
			break;
		case JLE:
			sprn->aluout = (spro->alu0 <= spro->alu1);
			break;
		case JEQ:
			sprn->aluout = (spro->alu0 == spro->alu1);
			break;
		case JNE:
			sprn->aluout = (spro->alu0 != spro->alu1);
			break;
		case JIN:
			sprn->aluout = 1;
			break;

		case DMA:
			if (spro->dma_state == DMA_STATE_IDLE) {
				sprn->dma_raddr = spro->alu0;
				sprn->dma_waddr = spro->alu1;
				sprn->dma_cnt = spro->immediate;
				sprn->dma_state = DMA_STATE_READ;
			}
			break;
		case POL:
			break;

		case HLT:
			break;
		default:
			printf("Unknown opcode %d\n", spro->opcode);
		}

		sprn->ctl_state = CTL_STATE_EXEC1;
		break;

	case CTL_STATE_EXEC1:
		switch(spro->opcode) {
		case ADD:
		case SUB:
		case LSF:
		case RSF:
		case AND:
		case OR:
		case XOR:
		case LHI:
			sprn->r[spro->dst] = sprn->aluout;
			break;

		case LD:
			sprn->r[spro->dst] = llsim_mem_extract_dataout(sp->sram, 31, 0);
			break;
		case ST:
			llsim_mem_set_datain(sp->sram, spro->alu0, 31, 0);
			llsim_mem_write(sp->sram, spro->alu1);
			break;

		case JLT:
		case JLE:
		case JEQ:
		case JNE:
		case JIN:
			if (spro->aluout) {
				sprn->r[7] = spro->pc - 1;
				sprn->pc = spro->immediate;
			}
			break;

		case DMA:
			break;
		case POL:
			sprn->r[spro->dst] = spro->dma_cnt;
		case HLT:
			break;
		default:
			printf("Unknown opcode %d\n", spro->opcode);
		}

		dump_trace(spro, sprn);
		if (spro->opcode == HLT) {
			dump_sram(sp);
			llsim_stop();
		}
		sprn->ctl_state = (spro->opcode == HLT) ? CTL_STATE_IDLE : CTL_STATE_FETCH0;
		break;
	}

	if (dma_enable(spro)) {
		switch (spro->dma_state) {
		case DMA_STATE_IDLE:
			break;
		case DMA_STATE_READ:
			if (dma_enable(sprn)) {
				llsim_mem_read(sp->sram, spro->dma_raddr);
				sprn->dma_state = DMA_STATE_SAMPLE;
			}
			break;
		case DMA_STATE_SAMPLE:
			sprn->dma_reg = llsim_mem_extract_dataout(sp->sram, 31, 0);
			sprn->dma_raddr = spro->dma_raddr + 1;
			sprn->dma_state = DMA_STATE_WRITE;
			break;
		case DMA_STATE_WRITE:
			llsim_mem_set_datain(sp->sram, spro->dma_reg, 31, 0);
			llsim_mem_write(sp->sram, spro->dma_waddr);
			sprn->dma_waddr = spro->dma_waddr + 1;
			sprn->dma_cnt = spro->dma_cnt - 1;
			sprn->dma_state = (spro->dma_cnt == 1) ? DMA_STATE_IDLE : DMA_STATE_READ;
			break;
		default:
			break;
		}
	}
}

static void sp_run(llsim_unit_t *unit)
{
	sp_t *sp = (sp_t *) unit->private;

	if (llsim->reset) {
		sp_reset(sp);
		return;
	}

	sp->sram->read = 0;
	sp->sram->write = 0;

	sp_ctl(sp);
}

static void sp_generate_sram_memory_image(sp_t *sp, char *program_name)
{
        FILE *fp;
        int addr, i;

        fp = fopen(program_name, "r");
        if (fp == NULL) {
                printf("couldn't open file %s\n", program_name);
                exit(1);
        }
        addr = 0;
        while (addr < SP_SRAM_HEIGHT) {
                fscanf(fp, "%08x\n", &sp->memory_image[addr]);
                addr++;
                if (feof(fp))
                        break;
        }
	sp->memory_image_size = addr;

	for (i = 0; i < sp->memory_image_size; i++)
		llsim_mem_inject(sp->sram, i, sp->memory_image[i], 31, 0);
}

static void sp_register_all_registers(sp_t *sp)
{
	sp_registers_t *spro = sp->spro, *sprn = sp->sprn;

	// registers
	llsim_register_register("sp", "r_0", 32, 0, &spro->r[0], &sprn->r[0]);
	llsim_register_register("sp", "r_1", 32, 0, &spro->r[1], &sprn->r[1]);
	llsim_register_register("sp", "r_2", 32, 0, &spro->r[2], &sprn->r[2]);
	llsim_register_register("sp", "r_3", 32, 0, &spro->r[3], &sprn->r[3]);
	llsim_register_register("sp", "r_4", 32, 0, &spro->r[4], &sprn->r[4]);
	llsim_register_register("sp", "r_5", 32, 0, &spro->r[5], &sprn->r[5]);
	llsim_register_register("sp", "r_6", 32, 0, &spro->r[6], &sprn->r[6]);
	llsim_register_register("sp", "r_7", 32, 0, &spro->r[7], &sprn->r[7]);

	llsim_register_register("sp", "pc", 16, 0, &spro->pc, &sprn->pc);
	llsim_register_register("sp", "inst", 32, 0, &spro->inst, &sprn->inst);
	llsim_register_register("sp", "opcode", 5, 0, &spro->opcode, &sprn->opcode);
	llsim_register_register("sp", "dst", 3, 0, &spro->dst, &sprn->dst);
	llsim_register_register("sp", "src0", 3, 0, &spro->src0, &sprn->src0);
	llsim_register_register("sp", "src1", 3, 0, &spro->src1, &sprn->src1);
	llsim_register_register("sp", "alu0", 32, 0, &spro->alu0, &sprn->alu0);
	llsim_register_register("sp", "alu1", 32, 0, &spro->alu1, &sprn->alu1);
	llsim_register_register("sp", "aluout", 32, 0, &spro->aluout, &sprn->aluout);
	llsim_register_register("sp", "immediate", 32, 0, &spro->immediate, &sprn->immediate);
	llsim_register_register("sp", "cycle_counter", 32, 0, &spro->cycle_counter, &sprn->cycle_counter);
	llsim_register_register("sp", "ctl_state", 3, 0, &spro->ctl_state, &sprn->ctl_state);
}

void sp_init(char *program_name)
{
	llsim_unit_t *llsim_sp_unit;
	llsim_unit_registers_t *llsim_ur;
	sp_t *sp;

	llsim_printf("initializing sp unit\n");

	inst_trace_fp = fopen("inst_trace.txt", "w");
	if (inst_trace_fp == NULL) {
		printf("couldn't open file inst_trace.txt\n");
		exit(1);
	}

	cycle_trace_fp = fopen("cycle_trace.txt", "w");
	if (cycle_trace_fp == NULL) {
		printf("couldn't open file cycle_trace.txt\n");
		exit(1);
	}

	llsim_sp_unit = llsim_register_unit("sp", sp_run);
	llsim_ur = llsim_allocate_registers(llsim_sp_unit, "sp_registers", sizeof(sp_registers_t));
	sp = llsim_malloc(sizeof(sp_t));
	llsim_sp_unit->private = sp;
	sp->spro = llsim_ur->old;
	sp->sprn = llsim_ur->new;

	sp->sram = llsim_allocate_memory(llsim_sp_unit, "sram", 32, SP_SRAM_HEIGHT, 0);
	sp_generate_sram_memory_image(sp, program_name);

	sp->start = 1;

	sp_register_all_registers(sp);
}
