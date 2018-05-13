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

int inst_cnt = 0;

typedef struct sp_registers_s {
	// 6 32 bit registers (r[0], r[1] don't exist)
	int r[8];
	int r_busy[8];

	int jtaken;

	// 32 bit cycle counter
	int cycle_counter;

	// fetch0
	int fetch0_active; // 1 bit
	int fetch0_pc; // 16 bits

	// fetch1
	int fetch1_active; // 1 bit
	int fetch1_pc; // 16 bits

	// dec0
	int dec0_active; // 1 bit
	int dec0_pc; // 16 bits
	int dec0_inst; // 32 bits

	// dec1
	int dec1_active; // 1 bit
	int dec1_pc; // 16 bits
	int dec1_inst; // 32 bits
	int dec1_opcode; // 5 bits
	int dec1_src0; // 3 bits
	int dec1_src1; // 3 bits
	int dec1_dst; // 3 bits
	int dec1_immediate; // 32 bits

	// exec0
	int exec0_active; // 1 bit
	int exec0_pc; // 16 bits
	int exec0_inst; // 32 bits
	int exec0_opcode; // 5 bits
	int exec0_src0; // 3 bits
	int exec0_src1; // 3 bits
	int exec0_dst; // 3 bits
	int exec0_immediate; // 32 bits
	int exec0_alu0; // 32 bits
	int exec0_alu1; // 32 bits

	// exec1
	int exec1_active; // 1 bit
	int exec1_pc; // 16 bits
	int exec1_inst; // 32 bits
	int exec1_opcode; // 5 bits
	int exec1_src0; // 3 bits
	int exec1_src1; // 3 bits
	int exec1_dst; // 3 bits
	int exec1_immediate; // 32 bits
	int exec1_alu0; // 32 bits
	int exec1_alu1; // 32 bits
	int exec1_aluout;
} sp_registers_t;

/*
 * Master structure
 */
typedef struct sp_s {
	// local srams
#define SP_SRAM_HEIGHT	64 * 1024
	llsim_memory_t *srami, *sramd;

	unsigned int memory_image[SP_SRAM_HEIGHT];
	int memory_image_size;

	int start;

	sp_registers_t *spro, *sprn;
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
#define JLT 16
#define JLE 17
#define JEQ 18
#define JNE 19
#define JIN 20
#define HLT 24

static char opcode_name[32][4] = {"ADD", "SUB", "LSF", "RSF", "AND", "OR", "XOR", "LHI",
				 "LD", "ST", "U", "U", "U", "U", "U", "U",
				 "JLT", "JLE", "JEQ", "JNE", "JIN", "U", "U", "U",
				 "HLT", "U", "U", "U", "U", "U", "U", "U"};

static void dump_sram(sp_t *sp, char *name, llsim_memory_t *sram)
{
	FILE *fp;
	int i;

	fp = fopen(name, "w");
	if (fp == NULL) {
                printf("couldn't open file %s\n", name);
                exit(1);
	}
	for (i = 0; i < SP_SRAM_HEIGHT; i++)
		fprintf(fp, "%08x\n", llsim_mem_extract(sram, i, 31, 0));
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

void dump_inst_trace_fp(sp_registers_t *spro, sp_registers_t *sprn)
{
	fprintf(inst_trace_fp, "--- instruction %d (%04x) @ PC %d (%04x) -----------------------------------------------------------\n",
		inst_cnt, inst_cnt, spro->exec1_pc - 1, spro->exec1_pc - 1);

	fprintf(inst_trace_fp, "pc = %04x, inst = %08x, opcode = %d (%s), dst = %d, src0 = %d, src1 = %d, immediate = %08x\n",
		spro->exec1_pc - 1, spro->exec1_inst, spro->exec1_opcode, opcode_tostring(spro->exec1_opcode), spro->exec1_dst, spro->exec1_src0, spro->exec1_src1, spro->exec1_immediate);

	fprintf(inst_trace_fp, "r[0] = %08x r[1] = %08x r[2] = %08x r[3] = %08x\nr[4] = %08x r[5] = %08x r[6] = %08x r[7] = %08x\n",
		spro->r[0],spro->r[1],spro->r[2],spro->r[3],spro->r[4],spro->r[5],spro->r[6],spro->r[7]);


	if (spro->exec1_opcode == HLT) {
		fprintf(inst_trace_fp, "\n>>>>EXEC: HALT at PC %04x <<<<\n",
			sprn->exec1_pc - 1);
		fprintf(inst_trace_fp, "sim finished at pc %d, %d instructions",
			spro->exec1_pc - 1, inst_cnt + 1);
		return;
	}

	switch(spro->exec1_opcode) {
	case ADD:
	case SUB:
	case LSF:
	case RSF:
	case AND:
	case OR:
	case XOR:
	case LHI:
		fprintf(inst_trace_fp, "\n>>>>EXEC: R[%d] = %d %s %d <<<<\n\n",
			sprn->exec1_dst, sprn->r[spro->exec1_dst], opcode_tostring(sprn->exec1_opcode), sprn->exec1_opcode);
		break;
	case LD:
		fprintf(inst_trace_fp, "\n>>>>EXEC: R[%d] = MEM[%d] = %08x <<<<\n\n",
			sprn->exec1_dst, sprn->r[sprn->exec1_src1], sprn->r[spro->exec1_dst]);
		break;
	case ST:
		fprintf(inst_trace_fp, "\n>>>>EXEC: MEM[%d] = %08x %s %d <<<<\n\n",
			sprn->exec1_alu1, sprn->exec1_alu0, "ST", sprn->exec1_opcode);
		break;

	case JLT:
	case JLE:
	case JEQ:
	case JNE:
	case JIN:
		fprintf(inst_trace_fp, "\n>>>>EXEC: %s %d, %d, %d <<<<\n\n",
			opcode_tostring(sprn->exec1_opcode), sprn->exec1_alu0, sprn->exec1_alu1, sprn->exec1_pc);
		break;
/*
	case POL:
		fprintf(inst_trace_fp, "\n>>>>EXEC: R[%d] = %d %s %d <<<<\n\n",
			sprn->dst, sprn->dma_cnt, opcode_tostring(sprn->opcode), sprn->opcode);
		break;
	case DMA:
		fprintf(inst_trace_fp, "\n>>>>EXEC: %s Read = %d, Write = %d, count = %d <<<<\n\n",
			opcode_tostring(sprn->opcode), sprn->dma_raddr, sprn->dma_waddr, sprn->dma_cnt);
		break;
*/

	default:
		printf("Unknown opcode\n");
	}
}

static int is_jump_inst(int opcode) {
	switch(opcode) {
	case JLT:
	case JLE:
	case JEQ:
	case JNE:
	case JIN:
		return true;
	default:
		return false;
	}
}

static void sp_ctl(sp_t *sp)
{
	sp_registers_t *spro = sp->spro;
	sp_registers_t *sprn = sp->sprn;
	int i;
	int opcode, dst, src0, src1;

	fprintf(cycle_trace_fp, "cycle %d\n", spro->cycle_counter);
	fprintf(cycle_trace_fp, "cycle_counter %08x\n", spro->cycle_counter);
	for (i = 2; i <= 7; i++)
		fprintf(cycle_trace_fp, "r%d %08x\n", i, spro->r[i]);

	fprintf(cycle_trace_fp, "fetch0_active %08x\n", spro->fetch0_active);
	fprintf(cycle_trace_fp, "fetch0_pc %08x\n", spro->fetch0_pc);

	fprintf(cycle_trace_fp, "fetch1_active %08x\n", spro->fetch1_active);
	fprintf(cycle_trace_fp, "fetch1_pc %08x\n", spro->fetch1_pc);

	fprintf(cycle_trace_fp, "dec0_active %08x\n", spro->dec0_active);
	fprintf(cycle_trace_fp, "dec0_pc %08x\n", spro->dec0_pc);
	fprintf(cycle_trace_fp, "dec0_inst %08x\n", spro->dec0_inst); // 32 bits

	fprintf(cycle_trace_fp, "dec1_active %08x\n", spro->dec1_active);
	fprintf(cycle_trace_fp, "dec1_pc %08x\n", spro->dec1_pc); // 16 bits
	fprintf(cycle_trace_fp, "dec1_inst %08x\n", spro->dec1_inst); // 32 bits
	fprintf(cycle_trace_fp, "dec1_opcode %08x\n", spro->dec1_opcode); // 5 bits
	fprintf(cycle_trace_fp, "dec1_src0 %08x\n", spro->dec1_src0); // 3 bits
	fprintf(cycle_trace_fp, "dec1_src1 %08x\n", spro->dec1_src1); // 3 bits
	fprintf(cycle_trace_fp, "dec1_dst %08x\n", spro->dec1_dst); // 3 bits
	fprintf(cycle_trace_fp, "dec1_immediate %08x\n", spro->dec1_immediate); // 32 bits

	fprintf(cycle_trace_fp, "exec0_active %08x\n", spro->exec0_active);
	fprintf(cycle_trace_fp, "exec0_pc %08x\n", spro->exec0_pc); // 16 bits
	fprintf(cycle_trace_fp, "exec0_inst %08x\n", spro->exec0_inst); // 32 bits
	fprintf(cycle_trace_fp, "exec0_opcode %08x\n", spro->exec0_opcode); // 5 bits
	fprintf(cycle_trace_fp, "exec0_src0 %08x\n", spro->exec0_src0); // 3 bits
	fprintf(cycle_trace_fp, "exec0_src1 %08x\n", spro->exec0_src1); // 3 bits
	fprintf(cycle_trace_fp, "exec0_dst %08x\n", spro->exec0_dst); // 3 bits
	fprintf(cycle_trace_fp, "exec0_immediate %08x\n", spro->exec0_immediate); // 32 bits
	fprintf(cycle_trace_fp, "exec0_alu0 %08x\n", spro->exec0_alu0); // 32 bits
	fprintf(cycle_trace_fp, "exec0_alu1 %08x\n", spro->exec0_alu1); // 32 bits

	fprintf(cycle_trace_fp, "exec1_active %08x\n", spro->exec1_active);
	fprintf(cycle_trace_fp, "exec1_pc %08x\n", spro->exec1_pc); // 16 bits
	fprintf(cycle_trace_fp, "exec1_inst %08x\n", spro->exec1_inst); // 32 bits
	fprintf(cycle_trace_fp, "exec1_opcode %08x\n", spro->exec1_opcode); // 5 bits
	fprintf(cycle_trace_fp, "exec1_src0 %08x\n", spro->exec1_src0); // 3 bits
	fprintf(cycle_trace_fp, "exec1_src1 %08x\n", spro->exec1_src1); // 3 bits
	fprintf(cycle_trace_fp, "exec1_dst %08x\n", spro->exec1_dst); // 3 bits
	fprintf(cycle_trace_fp, "exec1_immediate %08x\n", spro->exec1_immediate); // 32 bits
	fprintf(cycle_trace_fp, "exec1_alu0 %08x\n", spro->exec1_alu0); // 32 bits
	fprintf(cycle_trace_fp, "exec1_alu1 %08x\n", spro->exec1_alu1); // 32 bits
	fprintf(cycle_trace_fp, "exec1_aluout %08x\n", spro->exec1_aluout);

	fprintf(cycle_trace_fp, "\n");

	sp_printf("cycle_counter %08x\n", spro->cycle_counter);
	sp_printf("r2 %08x, r3 %08x\n", spro->r[2], spro->r[3]);
	sp_printf("r4 %08x, r5 %08x, r6 %08x, r7 %08x\n", spro->r[4], spro->r[5], spro->r[6], spro->r[7]);
	sp_printf("fetch0_active %d, fetch1_active %d, dec0_active %d, dec1_active %d, exec0_active %d, exec1_active %d\n",
		  spro->fetch0_active, spro->fetch1_active, spro->dec0_active, spro->dec1_active, spro->exec0_active, spro->exec1_active);
	sp_printf("fetch0_pc %d, fetch1_pc %d, dec0_pc %d, dec1_pc %d, exec0_pc %d, exec1_pc %d\n",
		  spro->fetch0_pc, spro->fetch1_pc, spro->dec0_pc, spro->dec1_pc, spro->exec0_pc, spro->exec1_pc);

	sprn->cycle_counter = spro->cycle_counter + 1;

	if (sp->start)
		sprn->fetch0_active = 1;

	// fetch0
	sprn->fetch1_active = 0;
	if (spro->fetch0_active) {
		llsim_mem_read(sp->srami, spro->pc);

		sprn->fetch1_active = 1;
		sprn->fetch1_pc = spro->fetch0_pc;
	}

	// fetch1
	sprn->dec0_active = 0;
	if (spro->fetch1_active) {
		sprn->dec0_inst = llsim_mem_extract_dataout(sp->srami, 31, 0);

		sprn->dec0_active = 1;
		sprn->dec0_pc = spro->fetch1_pc;
	}
	
	// dec0
	sprn->dec1_active = 0;
	if (spro->dec0_active) {
		opcode = spro->dec0_inst >> 25;
		dst = (spro->dec0_inst >> 22) & 7;
		src0 = (spro->dec0_inst >> 19) & 7;
		src1 = (spro->dec0_inst >> 16) & 7;

		sprn->dec1_pc = (is_jump_inst(opcode) & spro->jtaken) ? spro->dec0_immediate : spro->dec0_pc + 1;
		sprn->dec1_opcode = opcode;
		if (spro->r_busy[src0] || spro->r_busy[src1]) {
			sprn->fetch0_active = 0;
			sprn->fetch1_active = 0;
			sprn->dec0_active = 1;
		} else {
			sprn->dec1_dst = dst;
			sprn->dec1_src0 = src0;
			sprn->dec1_src1 = src1;
			sprn->dec1_immediate = spro->dec0_inst & 0xffff;
			sprn->r_busy[dst] = 1;

			sprn->fetch0_active = 1;
			sprn->fetch1_active = 1;
			sprn->dec0_active = 1;
			sprn->dec1_active = 1;

			sprn->dec1_pc = spro->dec0_pc;
			sprn->dec1_inst = spro->dec0_inst;
		}
	}
	
	// dec1
	sprn->exec0_active = 0;
	if (spro->dec1_active) {
		sprn->r[1] = spro->dec1_immediate;
		sprn->exec0_alu0 = (spro->dec1_src0 == 1) ? spro->dec1_immediate : spro->r[spro->dec1_src0];
		sprn->exec0_alu1 = (spro->dec1_src1 == 1) ? spro->dec1_immediate : spro->r[spro->dec1_src1];
		if (spro->dec1_opcode == LHI)
			sprn->exec0_alu0 = spro->dec1_dst;

		sprn->exec0_active = 1;
		sprn->exec0_pc = spro->dec1_pc;
		sprn->exec0_inst = spro->dec1_inst;
		sprn->exec0_opcode = spro->dec1_opcode;
		sprn->exec0_dst = spro->dec1_dst;
		sprn->exec0_src0 = spro->dec1_src0;
		sprn->exec0_src1 = spro->dec1_src1;
		sprn->exec0_immediate = spro->dec1_immediate;
	}
	
	// exec0
	sprn->exec1_active = 0;
	if (spro->exec0_active) {

		switch(spro->exec0_opcode) {
		case ADD:
			sprn->exec1_aluout = spro->exec0_alu0 + spro->exec0_alu1;
			break;
		case SUB:
			sprn->exec1_aluout = spro->exec0_alu0 - spro->exec0_alu1;
			break;
		case LSF:
			sprn->exec1_aluout = spro->exec0_alu0 << spro->exec0_alu1;
			break;
		case RSF:
			sprn->exec1_aluout = spro->exec0_alu0 >> spro->exec0_alu1;
			break;
		case AND:
			sprn->exec1_aluout = spro->exec0_alu0 & spro->exec0_alu1;
			break;
		case OR:
			sprn->exec1_aluout = spro->exec0_alu0 | spro->exec0_alu1;
			break;
		case XOR:
			sprn->exec1_aluout = spro->exec0_alu0 ^ spro->exec0_alu1;
			break;
		case LHI:
			sprn->exec1_aluout = (spro->exec0_immediate << 16) + (spro->exec0_alu0 && 0xffff);
			break;

		case LD:
			llsim_mem_read(sp->sramd, spro->exec0_alu1);
			break;
		case ST:
			llsim_mem_set_datain(sp->sramd, spro->exec0_alu0, 31, 0);
			llsim_mem_write(sp->sramd, spro->exec0_alu1);
			break;

		case JLT:
			sprn->exec1_aluout = (spro->exec0_alu0 < spro->exec0_alu1);
			break;
		case JLE:
			sprn->exec1_aluout = (spro->exec0_alu0 <= spro->exec0_alu1);
			break;
		case JEQ:
			sprn->exec1_aluout = (spro->exec0_alu0 == spro->exec0_alu1);
			break;
		case JNE:
			sprn->exec1_aluout = (spro->exec0_alu0 != spro->exec0_alu1);
			break;
		case JIN:
			sprn->exec1_aluout = 1;
			break;
/*
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
*/
		case HLT:
			break;
		default:
			printf("Unknown opcode %d\n", spro->opcode);
		}

		sprn->exec1_active = 1;
		sprn->exec1_pc = spro->exec0_pc;
		sprn->exec1_inst = spro->exec0_inst;
		sprn->exec1_opcode = spro->exec0_opcode;
		sprn->exec1_dst = spro->exec0_dst;
		sprn->exec1_src0 = spro->exec0_src0;
		sprn->exec1_src1 = spro->exec0_src1;
		sprn->exec1_immediate = spro->exec0_immediate;
		sprn->exec1_alu0 = spro->exec0_alu0;
		sprn->exec1_alu1 = spro->exec0_alu1;
	}
	
	// exec1
	if (spro->exec1_active) {

		switch(spro->opcode) {
		case ADD:
		case SUB:
		case LSF:
		case RSF:
		case AND:
		case OR:
		case XOR:
		case LHI:
			sprn->r[spro->exec1_dst] = spro->exec1_aluout;
			sprn->r_busy[spro->exec1_dst] = 0;
			break;

		case LD:
			sprn->r[spro->exec1_dst] = llsim_mem_extract_dataout(sp->sramd, 31, 0);
			sprn->r_busy[spro->exec1_dst] = 0;
			break;
		case ST:
			break;

		case JLT:
		case JLE:
		case JEQ:
		case JNE:
		case JIN:
			if (spro->exec1_aluout) {
				sprn->r[7] = spro->pc - 1;
			}
			if (spro->exec1_aluout != spro->jtaken) {
				sprn->fetch0_active = 1;
				sprn->fetch1_active = 0;
				sprn->dec0_active = 0;
				sprn->dec1_active = 0;
				sprn->exec0_active = 0;
				sprn->exec1_active = 0;
				sprn->pc = spro->exec1_aluout ? spro->immediate : spro->exec1_pc + 1;
			}

			sprn->jtaken = spro->exec1_aluout;
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

		dump_inst_trace_fp(spro, sprn);
		inst_cnt++;

		if (spro->exec1_opcode == HLT) {
			llsim_stop();
			sprn->fetch0_active = 0;
			sprn->fetch1_active = 0;
			sprn->dec0_active = 0;
			sprn->dec1_active = 0;
			sprn->exec0_active = 0;
			sprn->exec1_active = 0;
			dump_sram(sp, "srami_out.txt", sp->srami);
			dump_sram(sp, "sramd_out.txt", sp->sramd);
		}
	}
}

static void sp_run(llsim_unit_t *unit)
{
	sp_t *sp = (sp_t *) unit->private;
	//	sp_registers_t *spro = sp->spro;
	//	sp_registers_t *sprn = sp->sprn;

	//	llsim_printf("-------------------------\n");

	if (llsim->reset) {
		sp_reset(sp);
		return;
	}

	sp->srami->read = 0;
	sp->srami->write = 0;
	sp->sramd->read = 0;
	sp->sramd->write = 0;

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
                //              printf("addr %x: %08x\n", addr, sp->memory_image[addr]);
                addr++;
                if (feof(fp))
                        break;
        }
	sp->memory_image_size = addr;

        fprintf(inst_trace_fp, "program %s loaded, %d lines\n", program_name, addr);

	for (i = 0; i < sp->memory_image_size; i++) {
		llsim_mem_inject(sp->srami, i, sp->memory_image[i], 31, 0);
		llsim_mem_inject(sp->sramd, i, sp->memory_image[i], 31, 0);
	}
}

void sp_init(char *program_name)
{
	llsim_unit_t *llsim_sp_unit;
	llsim_unit_registers_t *llsim_ur;
	sp_t *sp;

	llsim_printf("initializing sp unit\n");

	inst_trace_fp = fopen("inst_trace.txt", "w");
	if (inst_trace_fp == NULL) {
		printf("couldn't open file inst_trace_fp.txt\n");
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

	sp->srami = llsim_allocate_memory(llsim_sp_unit, "srami", 32, SP_SRAM_HEIGHT, 0);
	sp->sramd = llsim_allocate_memory(llsim_sp_unit, "sramd", 32, SP_SRAM_HEIGHT, 0);
	sp_generate_sram_memory_image(sp, program_name);

	sp->start = 1;
	
	// c2v_translate_end
}
