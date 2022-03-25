// Names: Chanik Bryan Lee, Yilei Bai
// CNET IDs: bryan22lee, yileib

// The above are group members for this project.

/*
 * CMSC 22200
 *
 * ARM pipeline timing simulator
 */

#ifndef _PIPE_H_
#define _PIPE_H_

#include "shell.h"
#include "stdbool.h"
#include <limits.h>

// Macros for opcodes
#define HLT_0    (0xD4400000)
#define ADD_I    (0x91000000)
#define ADD_reg  (0x8B000000)
#define ADDS_I   (0xB1000000)
#define ADDS_reg (0xAB000000)
#define AND      (0x8A000000)
#define CBNZ     (0xB5000000)
#define CBZ      (0xB4000000)
#define ANDS     (0xEA000000)
#define EOR      (0xCA000000)
#define ORR      (0xAA000000)
#define LDUR     (0xF8400000)
#define LDURB    (0x38400000)
#define LDURH    (0x78400000)
#define LSL      (0xD3400000)
#define LSR      (0xD3400000)
#define MOVZ     (0xD2800000)
#define STUR     (0xF8000000)
#define STURB    (0x38000000)
#define STURH    (0x78000000)
#define SUB_I    (0xD1000000)
#define SUB_reg  (0xCB000000)
#define SUBS_I   (0xF1000000)
#define SUBS_reg (0xEB000000)
#define MUL      (0x9B000000)
#define CMP_reg  (0xEB000000)
#define CMP_I    (0xF1000000)
#define BR       (0xD6000000)
#define B        (0x14000000)
#define BCOND    (0x54000000)
#define BL       (0x94000000)

// For testing purposes only
#define DEBUG (0) 

//keeps track of whether or not the predicted branch path was taken or not
typedef struct {
	uint16_t missed;
	uint16_t predict_taken;
	uint64_t predict_addr;
} predict_t;

typedef struct CPU_State {
	/* register file state */
	int64_t REGS[ARM_REGS];
	int FLAG_N;        /* flag N */
	int FLAG_Z;        /* flag Z */

	/* program counter in fetch stage */
	uint64_t PC;
	
} CPU_State;

int RUN_BIT;

/* global variable -- pipeline state */
extern CPU_State CURRENT_STATE;

//pipeline registers will be of type CPU_State so they can be a "snapshot" of 
//everything going on in CURRENT_STATE and essentially serve as temporary storage
//for us to use in the same vein as pipeline registers
CPU_State Pipe_Reg_IFtoDE;
CPU_State Pipe_Reg_DEtoEX;
CPU_State Pipe_Reg_EXtoMEM;
CPU_State Pipe_Reg_MEMtoWB;

typedef enum STAGE {
	STAGE_FETCH,   //0
	STAGE_DECODE,  //1
	STAGE_EXECUTE, //2
	STAGE_MEM,     //3
	STAGE_WB,      //4
	STAGE_INVALID  //5
} STAGE;

typedef enum INSTRUCTION_TYPE {
	INSTRUCTION_TYPE_B,
	INSTRUCTION_TYPE_CB, 
	INSTRUCTION_TYPE_I,
	INSTRUCTION_TYPE_R,
	INSTRUCTION_TYPE_D,
	INSTRUCTION_TYPE_IW,
	INSTRUCTION_TYPE_INVALID,
	INSTRUCTION_TYPE_HLT
} INSTRUCTION_TYPE;

//structs for splitting up each instruction formats' important parts
//into a format that is easier to work with
typedef struct INSTRUCTION_FORMAT_R {
    uint32_t opcode;
    uint32_t rm;
    uint32_t rn;
    uint32_t rd;
} INSTRUCTION_R;

typedef struct INSTRUCTION_FORMAT_I {
    uint32_t opcode;
    int32_t alu_immediate;
    uint32_t rn;
    uint32_t rd;
} INSTRUCTION_I;

typedef struct INSTRUCTION_FORMAT_D {
    uint32_t opcode;
    uint32_t dt_address;
    uint32_t rn;
    uint32_t rt;
} INSTRUCTION_D;

typedef struct INSTRUCTION_FORMAT_B {
    uint32_t opcode;
    int32_t br_address;
} INSTRUCTION_B;

typedef struct INSTRUCTION_FORMAT_CB {
    uint32_t opcode;
    int32_t cb_address;
    uint32_t rt;
    uint32_t negation_flag;
    uint32_t cond;
} INSTRUCTION_CB;

typedef struct INSTRUCTION_FORMAT_IW {
    uint32_t opcode;
    int32_t mov_immediate;
    uint32_t rd;
} INSTRUCTION_IW;

//struct for holding all the necessary parts required for pipelining an instruction
typedef struct PIPELINED_INSTRUCTION {

	//keep track of what current stage the pipelined instruction is undergoing
	//IF, ID, EX, MEM, WB
	STAGE curr_stage; 

	//know what type of instruction we are pipelining
	INSTRUCTION_TYPE instr_type;

	uint32_t instruction;

	//know what type of instruction it is and store the decoded format
	union {
		INSTRUCTION_B instruction_b;
		INSTRUCTION_CB instruction_cb;
		INSTRUCTION_D instruction_d;
		INSTRUCTION_I instruction_i;
		INSTRUCTION_IW instruction_iw;
		INSTRUCTION_R instruction_r;
	} decoded_instruction;

	//keep track of important values
	int src_reg0, src_reg1; //-1 = invalid, 0-31 = register number

	int dest_reg; //-1 = invalid, 0-31 = register number

	int needs_flags; //0 = doesn't need the flags, 1 = needs the flags
	int update_flags; //0 = don't update flags, 1 = update flags

	predict_t predict;

	uint64_t PC;

} PIPELINED_INSTRUCTION;

/* called during simulator startup */
void pipe_init();

/* this function calls the others */
void pipe_cycle();

/* each of these functions implements one stage of the pipeline */
void pipe_stage_fetch();
void pipe_stage_decode();
void pipe_stage_execute();
void pipe_stage_mem();
void pipe_stage_wb();

#endif
