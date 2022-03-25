// Names: Chanik Bryan Lee, Yilei Bai
// CNET IDs: bryan22lee, yileib

// The above are group members for this project.

/*
 * CMSC 22200
 *
 * ARM pipeline timing simulator
 */

#include "pipe.h"
#include "shell.h"
#include "bp.h"
#include "cache.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

extern bp_t bp_data;

/* global pipeline state */
CPU_State CURRENT_STATE;

// 5 stage pipline queue
PIPELINED_INSTRUCTION queue[5];

//keep track of which is the last one
int tail = -1;

//determine if HLT has been reached
int reached_HLT = 0;

//determines if we stall or not
//1 = yes
int stall = 0;

//determines if we stall or not for a CACHE MISS
int instr_cache_stall = 0;
int data_cache_stall = 0;

//declare the two caches
cache_t *instr_cache = NULL;
cache_t *data_cache = NULL;

//mem write_back address
uint64_t mem_wb_addr = 0;

//stalled instruction addr
uint64_t stalled_instr_addr = 0;

void execute_b(INSTRUCTION_B instruction_b, predict_t *p_predict) {

    switch (instruction_b.opcode) {

        case (B) :
            //branch and multiply by 4
            Pipe_Reg_EXtoMEM.PC += instruction_b.br_address << 2;

            break;
        case (BL) :
            //branch and multiply by 4
            Pipe_Reg_EXtoMEM.PC += instruction_b.br_address << 2;

            Pipe_Reg_EXtoMEM.REGS[30] = Pipe_Reg_DEtoEX.PC + 4;
            
            //push the stack by adding the PC + 4 addr to it
            if (stack_top < (STACK_SIZE - 1)) {
                addr_stack[++stack_top] = Pipe_Reg_DEtoEX.PC + 4;
            } 

            break;
    }

    //if (Pipe_Reg_EXtoMEM.PC != Pipe_Reg_IFtoDE.PC)
    //    CURRENT_STATE.PC = Pipe_Reg_EXtoMEM.PC;

    if (DEBUG) printf("DEBUG: Pipe_Reg_EXtoMEM.PC = %lx\n", Pipe_Reg_EXtoMEM.PC);

    bp_update(Pipe_Reg_DEtoEX.PC, 1, 0, Pipe_Reg_EXtoMEM.PC, 1, 0);

    //if BTB miss
    //if branch is mis-predicted
    //if branch predicted taken but the PC is wrong

    if (p_predict->missed ||
        Pipe_Reg_EXtoMEM.PC != p_predict->predict_addr ||
        1 != p_predict->predict_taken) {
            //flush
            stall = 1;
            if (tail >= 0 && queue[tail].curr_stage == STAGE_FETCH) 
                tail--;
            CURRENT_STATE.PC = Pipe_Reg_EXtoMEM.PC;
    }
}

void execute_cb(INSTRUCTION_CB instruction_cb, predict_t *p_predict) {

    int taken = 0;

    switch (instruction_cb.opcode) {
        case (BCOND) : //branch based on some equality/inequality checks using the flags, after a CMP of two register values
            //check the condition flag and perform the correct action accordingly
            if (DEBUG) printf("DEBUG: b.cond cond = %x\n", instruction_cb.cond);

            switch (instruction_cb.cond) { //see which condition to check
                case (0x0) : //EQ or NE
                    if (instruction_cb.negation_flag == 0) { //check normal condition
                        if (Pipe_Reg_DEtoEX.FLAG_Z == 0) { //the subtraction of the two values is NONZERO
                            //AKA the two values are not equal
                            //don't branch because NE
                            Pipe_Reg_EXtoMEM.PC += 4;
                        } else { //Pipe_Reg_DEtoEX.FLAG_Z == 1, the subtraction of the two values is ZERO
                            //branch because EQ
                            Pipe_Reg_EXtoMEM.PC += instruction_cb.cb_address << 2;

                            taken = 1;

                            if (Pipe_Reg_EXtoMEM.PC != Pipe_Reg_IFtoDE.PC)
                                CURRENT_STATE.PC = Pipe_Reg_EXtoMEM.PC;

                            if (DEBUG) printf("DEBUG: Pipe_Reg_EXtoMEM.PC = %lx\n", Pipe_Reg_EXtoMEM.PC);
                        }
                    } else { //negation_flag = 1, check opposite condition
                        if (Pipe_Reg_DEtoEX.FLAG_Z == 1) { //the subtraction of the two values is ZERO
                            //AKA the two values are equal
                            //don't branch because EQ
                            Pipe_Reg_EXtoMEM.PC += 4;
                        } else { //Pipe_Reg_DEtoEX.FLAG_Z == 0, the subtraction of the two values is NONZERO
                            //AKA the two vaues are not equal
                            //branch because NE
                            Pipe_Reg_EXtoMEM.PC += instruction_cb.cb_address << 2;

                            taken = 1;

                            if (Pipe_Reg_EXtoMEM.PC != Pipe_Reg_IFtoDE.PC)
                                CURRENT_STATE.PC = Pipe_Reg_EXtoMEM.PC;

                            if (DEBUG) printf("DEBUG: Pipe_Reg_EXtoMEM.PC = %lx\n", Pipe_Reg_EXtoMEM.PC);
                        }
                    }
                    break;
                case (0x5) : //GE LT
                    if (instruction_cb.negation_flag == 0) { //check normal condition
                        if (Pipe_Reg_DEtoEX.FLAG_N == 0) { //the subtraction of the two values is 0 or positive
                            //AKA x >= y
                            //branch and multiply by 4 (word size)
                            Pipe_Reg_EXtoMEM.PC += instruction_cb.cb_address << 2;

                            taken = 1;

                            if (Pipe_Reg_EXtoMEM.PC != Pipe_Reg_IFtoDE.PC)
                                CURRENT_STATE.PC = Pipe_Reg_EXtoMEM.PC;

                            if (DEBUG) printf("DEBUG: Pipe_Reg_EXtoMEM.PC = %lx\n", Pipe_Reg_EXtoMEM.PC);

                        } else { //the subtraction of the two values is NEGATIVE
                            //AKA x < y
                            //don't branch
                            Pipe_Reg_EXtoMEM.PC += 4;
                        }
                    } else {//check opposite condition
                        if (Pipe_Reg_DEtoEX.FLAG_N == 0) { //the subtraction of the two values is 0 or positive
                            //AKA x >= y
                            //don't branch
                            Pipe_Reg_EXtoMEM.PC += 4;

                        } else { //the subtraction of the two values is NEGATIVE
                            //AKA x < y
                            //branch and multiply by 4 (word size)
                            Pipe_Reg_EXtoMEM.PC += instruction_cb.cb_address << 2;

                            taken = 1;

                            if (Pipe_Reg_EXtoMEM.PC != Pipe_Reg_IFtoDE.PC)
                                CURRENT_STATE.PC = Pipe_Reg_EXtoMEM.PC;

                            if (DEBUG) printf("DEBUG: Pipe_Reg_EXtoMEM.PC = %lx\n", Pipe_Reg_EXtoMEM.PC);
                        }
                    }
                    break;
                case (0x6) : //GT LE
                    if (instruction_cb.negation_flag == 0) { //check normal condition
                        if (Pipe_Reg_DEtoEX.FLAG_N == 0 && Pipe_Reg_DEtoEX.FLAG_Z == 0) { 
                            //the subtraction of the two values is NONZERO and POSITIVE
                            //AKA x > y
                            //branch and multiply by 4 (word size)
                            Pipe_Reg_EXtoMEM.PC += instruction_cb.cb_address << 2;

                            taken = 1;

                            if (Pipe_Reg_EXtoMEM.PC != Pipe_Reg_IFtoDE.PC)
                                CURRENT_STATE.PC = Pipe_Reg_EXtoMEM.PC;

                            if (DEBUG) printf("DEBUG: Pipe_Reg_EXtoMEM.PC = %lx\n", Pipe_Reg_EXtoMEM.PC);

                        } else { //the subtraction of the two values is either NEGATIVE OR ZERO
                            //AKA x <= y
                            //don't branch
                            Pipe_Reg_EXtoMEM.PC += 4;
                        }
                    } else { //check opposite condition
                        if (Pipe_Reg_DEtoEX.FLAG_N == 0 && Pipe_Reg_DEtoEX.FLAG_Z == 0) {
                            //the subtraction of the two values is NONZERO AND POSITIVE
                            //AKA x > y
                            //don't branch
                            Pipe_Reg_EXtoMEM.PC += 4;
                        } else { //the subtraction of the two values is either NEGATIVE OR ZERO
                            //AKA x <= y 
                            //branch and multiply by 4 (word size)
                            Pipe_Reg_EXtoMEM.PC += instruction_cb.cb_address << 2;

                            taken = 1;

                            if (Pipe_Reg_EXtoMEM.PC != Pipe_Reg_IFtoDE.PC)
                                CURRENT_STATE.PC = Pipe_Reg_EXtoMEM.PC;

                            if (DEBUG) printf("DEBUG: Pipe_Reg_EXtoMEM.PC = %lx\n", Pipe_Reg_EXtoMEM.PC);
                        }
                    }
                    break; //break GT or LE
            }
        break; //break BCOND
    case (CBNZ) : //branch to target if register value is NOT ZERO
        if (Pipe_Reg_DEtoEX.REGS[instruction_cb.rt] != 0) {
            //branch
            Pipe_Reg_EXtoMEM.PC += instruction_cb.cb_address << 2;

            taken = 1;

            if (Pipe_Reg_EXtoMEM.PC != Pipe_Reg_IFtoDE.PC)
                CURRENT_STATE.PC = Pipe_Reg_EXtoMEM.PC;

            if (DEBUG) printf("DEBUG: Pipe_Reg_EXtoMEM.PC = %lx\n", Pipe_Reg_EXtoMEM.PC);
        } else {
            Pipe_Reg_EXtoMEM.PC += 4;
        }
        break;
    case (CBZ) : //branch to target if register value IS ZERO
            if (Pipe_Reg_DEtoEX.REGS[instruction_cb.rt] == 0) {
            //branch
            Pipe_Reg_EXtoMEM.PC += instruction_cb.cb_address << 2;

            taken = 1;

            if (Pipe_Reg_EXtoMEM.PC != Pipe_Reg_IFtoDE.PC)
                CURRENT_STATE.PC = Pipe_Reg_EXtoMEM.PC;

            if (DEBUG) printf("DEBUG: Pipe_Reg_EXtoMEM.PC = %lx\n", Pipe_Reg_EXtoMEM.PC);
        } else {
            Pipe_Reg_EXtoMEM.PC += 4;
        }
        break;
    }
    uint64_t target = Pipe_Reg_DEtoEX.PC + (instruction_cb.cb_address << 2);
    uint64_t expect = (taken) ? target : Pipe_Reg_DEtoEX.PC + 4;

    bp_update(Pipe_Reg_DEtoEX.PC, 1, 1, target, taken, 0);

    //if BTB miss
    //if branch is mis-predicted
    //if branch predicted taken but the PC is wrong

    int flush = 0;
    if (p_predict->missed) {
        if (taken)
            flush = 1;
    } else {
        if (expect != p_predict->predict_addr ||
            taken != p_predict->predict_taken) {
                flush = 1;
        }
    }

    //if (p_predict->missed ||
    //    expect != p_predict->predict_addr ||
    //    taken != p_predict->predict_taken) {
    if (flush) {
        //flush
        stall = 1;

        if (tail >= 0 && queue[tail].curr_stage == STAGE_FETCH)
            tail--;
        CURRENT_STATE.PC = expect;
    }
}

//keep in mind the special case of CMP essentially being SUBS
//AKA it has the same opcode as SUBS, just with Rd field = 11111
// Immediate logic/arithmetic computation
void execute_i(INSTRUCTION_I instruction_i) {

    // uint32_t opcode;
    // int32_t alu_immediate;
    // uint32_t rn;
    // uint32_t rd;

    int64_t result;
    int32_t imms;
    int32_t immr;

    // ADD Immediate (ADDI)
    if (instruction_i.opcode == ADD_I) {    
        // R[Rd] = R[Rn] + ALUImm
        result = Pipe_Reg_DEtoEX.REGS[instruction_i.rn] + instruction_i.alu_immediate;

		if (DEBUG) printf("\nDEBUG: Result: %li\n", result);

		if (DEBUG) printf("\nDEBUG: ___PipeEXtoMEM destination: %i\n", instruction_i.rd);
        Pipe_Reg_EXtoMEM.REGS[instruction_i.rd] = result;
    }
    // ADDIS
    else if (instruction_i.opcode == ADDS_I) {
        if (DEBUG) printf("\nRn: %li\n", Pipe_Reg_DEtoEX.REGS[instruction_i.rn]);
        if (DEBUG) printf("ALUImm: %i\n\n", instruction_i.alu_immediate);
        // R[Rd], FLAGS = R[Rn] + ALUImm
        result = Pipe_Reg_DEtoEX.REGS[instruction_i.rn] + instruction_i.alu_immediate;
        Pipe_Reg_EXtoMEM.REGS[instruction_i.rd] = result;
        //update the flags
        Pipe_Reg_EXtoMEM.FLAG_N = (result < 0) ? 1 : 0;
        Pipe_Reg_EXtoMEM.FLAG_Z = result ? 0 : 1;
    }
    // LSL and LSR (Immediate)
    else if (instruction_i.opcode == LSL) { // same opcode as LSR
        // Only take care of 64 bit variant
        if (DEBUG) printf("DEBUG: alu_immediate = %x\n", instruction_i.alu_immediate);
        imms = 0xFC0 & instruction_i.alu_immediate;
        if (DEBUG) printf("DEBUG: imms = %x\n", imms);

        if (imms & 0x800) { //LSL
            // R[Rd] = R[Rn] << shamt

            uint8_t shamt = (imms >> 6) & 0x3F;
            int8_t shamt_8;
            shamt |= 0xc0;
            shamt_8 = (int8_t) shamt;
            shamt_8 = -shamt_8;
            if (DEBUG) printf("DEBUG: shamt_8 = %x\n", shamt_8);
            result = Pipe_Reg_DEtoEX.REGS[instruction_i.rn] << shamt_8;
            Pipe_Reg_EXtoMEM.REGS[instruction_i.rd] = result;
        } else { //LSR
            // R[Rd] = R[Rn] >> shamt
            // shamt is immr

            immr = 0x3F & (instruction_i.alu_immediate >> 6);
            result = Pipe_Reg_DEtoEX.REGS[instruction_i.rn] >> immr;
            Pipe_Reg_EXtoMEM.REGS[instruction_i.rd] = result;
        }
    }
    // SUBI (Immediate)
    else if (instruction_i.opcode == SUB_I) {
        // R[Rd] = R[Rn] - ALUImm
        // R[Rd] = R[Rn] + ALUImm
        result = Pipe_Reg_DEtoEX.REGS[instruction_i.rn] - instruction_i.alu_immediate;
        Pipe_Reg_EXtoMEM.REGS[instruction_i.rd] = result;
    }
    // SUBIS (Immediate)
    else if (instruction_i.opcode == SUBS_I) {
        // R[Rd], FLAGS = R[Rn] - ALUImm
        result = Pipe_Reg_DEtoEX.REGS[instruction_i.rn] - instruction_i.alu_immediate;

        if (instruction_i.rd != 31) {
            Pipe_Reg_EXtoMEM.REGS[instruction_i.rd] = result;
        }   
        
        //update the flags
        Pipe_Reg_EXtoMEM.FLAG_N = (result < 0) ? 1 : 0;
        Pipe_Reg_EXtoMEM.FLAG_Z = result ? 0 : 1;
    }
    // CMPI (Immediate)
    /*else if (instruction_i.opcode == CMP_I) {
        // FLAGS = R[Rn] - ALUImm
        result = Pipe_Reg_DEtoEX.REGS[instruction_i.rn] - instruction_i.alu_immediate;
        //update the flags
        Pipe_Reg_EXtoMEM.FLAG_N = (result < 0) ? 1 : 0;
        Pipe_Reg_EXtoMEM.FLAG_Z = result ? 0 : 1;
    }
    */
}

//watch out for CMP again
void execute_r(INSTRUCTION_R instruction_r, predict_t *p_predict) {

    int64_t result;

    switch (instruction_r.opcode) {
        case (ADD_reg) : //add values of Rn and Rm and store sum into Rd
            result = Pipe_Reg_DEtoEX.REGS[instruction_r.rn] + Pipe_Reg_DEtoEX.REGS[instruction_r.rm];
            Pipe_Reg_EXtoMEM.REGS[instruction_r.rd] = result;
            break;
        case (ADDS_reg) : //add values of Rn and Rm and store sum into Rd AND update flags
            result = Pipe_Reg_DEtoEX.REGS[instruction_r.rn] + Pipe_Reg_DEtoEX.REGS[instruction_r.rm];
            Pipe_Reg_EXtoMEM.REGS[instruction_r.rd] = result;

            //update the flags
            Pipe_Reg_EXtoMEM.FLAG_N = (result < 0) ? 1 : 0;
            Pipe_Reg_EXtoMEM.FLAG_Z = result ? 0 : 1;
            break;

        case (AND) : //Perform bit-wise & between values in Rn and Rm and store into Rd
            result = Pipe_Reg_DEtoEX.REGS[instruction_r.rn] & Pipe_Reg_DEtoEX.REGS[instruction_r.rm];
            Pipe_Reg_EXtoMEM.REGS[instruction_r.rd] = result;
            break;
            
        case (ANDS) : //perform bit-wise & between values in Rn and Rm and store into Rd, update flags
            result = Pipe_Reg_DEtoEX.REGS[instruction_r.rn] & Pipe_Reg_DEtoEX.REGS[instruction_r.rm];
            Pipe_Reg_EXtoMEM.REGS[instruction_r.rd] = result;

            //update the flags
            Pipe_Reg_EXtoMEM.FLAG_N = (result < 0) ? 1 : 0;
            Pipe_Reg_EXtoMEM.FLAG_Z = result ? 0 : 1;
            break;
        
        case (EOR) : //perform bit-wise XOR on values in Rn and Rm and store into Rd
            result = Pipe_Reg_DEtoEX.REGS[instruction_r.rn] ^ Pipe_Reg_DEtoEX.REGS[instruction_r.rm];
            Pipe_Reg_EXtoMEM.REGS[instruction_r.rd] = result;
            break;

        case (ORR) : //perform bit-wise OR on values in Rn and Rm and store into Rd
            result = Pipe_Reg_DEtoEX.REGS[instruction_r.rn] | Pipe_Reg_DEtoEX.REGS[instruction_r.rm];
            Pipe_Reg_EXtoMEM.REGS[instruction_r.rd] = result;
            break; 
        
        case (SUB_reg) : //Rd = Rn - Rm, store result into Rd

            if (DEBUG) printf("DEBUG: Entered SUB_reg\n");
            result = Pipe_Reg_DEtoEX.REGS[instruction_r.rn] - Pipe_Reg_DEtoEX.REGS[instruction_r.rm]; 
            Pipe_Reg_EXtoMEM.REGS[instruction_r.rd] = result;
            break;

        //CMP is basically the same thing as SUBS extended register so they're combined here
        case (SUBS_reg) : //Rd = Rn - Rm, update flags, store result into Rd IF IT IS NOT CMP

            if (DEBUG) printf("DEBUG: Entered execute_r with Rn = %x and Rm = %x\n", instruction_r.rn, instruction_r.rm);
            result = Pipe_Reg_DEtoEX.REGS[instruction_r.rn] - Pipe_Reg_DEtoEX.REGS[instruction_r.rm];
            if (DEBUG) printf("DEBUG: result = %lx\n", result);

            //update the flags
            Pipe_Reg_EXtoMEM.FLAG_N = (result < 0) ? 1 : 0;
            Pipe_Reg_EXtoMEM.FLAG_Z = result ? 0 : 1;

            //add check for Rd == 11111
            if (instruction_r.rd != 31) {
                //in this case Rd != 11111 so it's NOT CMP, so we DO STORE the result
                Pipe_Reg_EXtoMEM.REGS[instruction_r.rd] = result;
            }
            //if Rd == 11111 AKA 31, it is CMP so we do not store the result

            break;
        case (MUL) : //multiply the values of Rn and Rm and store in Rd
            result = Pipe_Reg_DEtoEX.REGS[instruction_r.rn] * Pipe_Reg_DEtoEX.REGS[instruction_r.rm];
            Pipe_Reg_EXtoMEM.REGS[instruction_r.rd] = result;
            break;        
        case (BR) : //unconditional branch to a address stored in a register
            if (DEBUG) printf("DEBUG: instruction_r.rn = %x\n", instruction_r.rn);

            Pipe_Reg_EXtoMEM.PC = Pipe_Reg_DEtoEX.REGS[instruction_r.rn];

            bp_update(Pipe_Reg_DEtoEX.PC, 1, 0, Pipe_Reg_EXtoMEM.PC, 1, 1);

            if (p_predict->missed ||
                Pipe_Reg_EXtoMEM.PC != p_predict->predict_addr ||
                1 != p_predict->predict_taken) {
                //flush
                stall = 1;
            
                if (tail >= 0 && queue[tail].curr_stage == STAGE_FETCH)
                    tail--;
                CURRENT_STATE.PC = Pipe_Reg_EXtoMEM.PC;
            }
            //pop this entry from stack because it has been used for prediction 
            //stack_top -1 = empty
            if (stack_top >= 0)
                stack_top--;

    }
}

void execute_d(INSTRUCTION_D instruction_d, int hit_or_miss) {
    uint32_t val32;
    if (DEBUG) printf("DEBUG: instruction_d.opcode == %x\n", instruction_d.opcode);
    switch (instruction_d.opcode) {
        // LDUR
        case (LDUR) :
            if (DEBUG) printf("_____Entered LDUR with addr = %lx\n", Pipe_Reg_EXtoMEM.REGS[instruction_d.rn] + instruction_d.dt_address + 4);

            // R[Rt] = M[R[Rn] + DTAddr]
            //Pipe_Reg_MEMtoWB.REGS[instruction_d.rt] = 
            //    mem_read_32(Pipe_Reg_EXtoMEM.REGS[instruction_d.rn] + instruction_d.dt_address + 4);
            //Pipe_Reg_MEMtoWB.REGS[instruction_d.rt] = Pipe_Reg_MEMtoWB.REGS[instruction_d.rt] << 32;
            //Pipe_Reg_MEMtoWB.REGS[instruction_d.rt] |= (uint32_t)
            //    mem_read_32(Pipe_Reg_EXtoMEM.REGS[instruction_d.rn] + instruction_d.dt_address);

            Pipe_Reg_MEMtoWB.REGS[instruction_d.rt] = 
                cache_read(data_cache, Pipe_Reg_EXtoMEM.REGS[instruction_d.rn] + instruction_d.dt_address + 4);
            Pipe_Reg_MEMtoWB.REGS[instruction_d.rt] = Pipe_Reg_MEMtoWB.REGS[instruction_d.rt] << 32;
            Pipe_Reg_MEMtoWB.REGS[instruction_d.rt] |= (uint32_t)
                cache_read(data_cache, Pipe_Reg_EXtoMEM.REGS[instruction_d.rn] + instruction_d.dt_address);
            
            break;
        // LDURB
        case (LDURB) :
            // R[Rt] = {56'b0, M[R[Rn] + DTAddr](7:0)}
            // typecast mem_read_32 value to int64_t
            //Pipe_Reg_MEMtoWB.REGS[instruction_d.rt] =
            //    mem_read_32(Pipe_Reg_EXtoMEM.REGS[instruction_d.rn] + instruction_d.dt_address) & 0xFF;

            Pipe_Reg_MEMtoWB.REGS[instruction_d.rt] =
                cache_read(data_cache, Pipe_Reg_EXtoMEM.REGS[instruction_d.rn] + instruction_d.dt_address) & 0xFF;    
            break;
        // LDURH
        case (LDURH) :
            // R[Rt] = {48'b0, M[R[Rn] + DTAddr](15:0)}
            // typecast mem_read_32 value to int64_t
            //Pipe_Reg_MEMtoWB.REGS[instruction_d.rt] =
            //    mem_read_32(Pipe_Reg_EXtoMEM.REGS[instruction_d.rn] + instruction_d.dt_address) & 0xFFFF;

            Pipe_Reg_MEMtoWB.REGS[instruction_d.rt] =
                cache_read(data_cache, Pipe_Reg_EXtoMEM.REGS[instruction_d.rn] + instruction_d.dt_address) & 0xFFFF;
            break;
        case (STUR) : //store 64 bits to memory
            
            if (hit_or_miss) {
                val32 = (0xFFFFFFFF00000000 & Pipe_Reg_EXtoMEM.REGS[instruction_d.rt]) >> 32;
                mem_write_32(Pipe_Reg_EXtoMEM.REGS[instruction_d.rn] + instruction_d.dt_address + 4, val32);
                val32 = 0xFFFFFFFF & Pipe_Reg_EXtoMEM.REGS[instruction_d.rt];
                mem_write_32(Pipe_Reg_EXtoMEM.REGS[instruction_d.rn] + instruction_d.dt_address, val32);
            }

            val32 = (0xFFFFFFFF00000000 & Pipe_Reg_EXtoMEM.REGS[instruction_d.rt]) >> 32;
            cache_write(data_cache, Pipe_Reg_EXtoMEM.REGS[instruction_d.rn] + instruction_d.dt_address + 4, val32);
            val32 = 0xFFFFFFFF & Pipe_Reg_EXtoMEM.REGS[instruction_d.rt];
            cache_write(data_cache, Pipe_Reg_EXtoMEM.REGS[instruction_d.rn] + instruction_d.dt_address, val32);

            if (DEBUG_CACHE) printf("_____Entered STUR with addr = %lx and val32 = %x\n",
                Pipe_Reg_EXtoMEM.REGS[instruction_d.rn] + instruction_d.dt_address + 4, val32);

            break;
        case (STURB) : //store 8 bits to memory
            
            //go to Rn's address + offset, store Rt's value
            //first get the entire word of memory (32 bits) from base, because we'll extract out the first 24 bits
            val32 = mem_read_32(Pipe_Reg_EXtoMEM.REGS[instruction_d.rn] + instruction_d.dt_address);

            if (DEBUG) printf("DEBUG: val32 (value from mem): %x\n", val32);
            if (DEBUG) printf("DEBUG: instruction_d.dt_address = %x\n", instruction_d.dt_address);

            //preserve the first 24 bits
            val32 = val32 & 0xFFFFFF00;

            //merge the 8 bits of info with the 24 bits
            val32 = val32 | (0x000000FF & Pipe_Reg_EXtoMEM.REGS[instruction_d.rt]);

            if (DEBUG_CACHE) printf("_____Entered STURB with addr = %lx and val32 = %x\n",
                Pipe_Reg_EXtoMEM.REGS[instruction_d.rn] + instruction_d.dt_address + 4, val32);

            if (DEBUG) printf("DEBUG: val32 to be written: %x\n", val32);

            if (DEBUG) printf("DEBUG: address to write to: %lx\n", Pipe_Reg_EXtoMEM.REGS[instruction_d.rn] + instruction_d.dt_address);
            
            //write in the merged data to the address
            if (hit_or_miss)
                mem_write_32(Pipe_Reg_EXtoMEM.REGS[instruction_d.rn] + instruction_d.dt_address, val32);

            cache_write(data_cache, Pipe_Reg_EXtoMEM.REGS[instruction_d.rn] + instruction_d.dt_address, val32);

            break;
        case (STURH) : //store 16 bits to memory
            //go to Rn's address + offset, store Rt's value
            //first get the entire word of memory (32 bits) from base, because we'll extract out the first 16 bits
            val32 = mem_read_32(Pipe_Reg_EXtoMEM.REGS[instruction_d.rn] + instruction_d.dt_address);
            uint32_t val32_2 = cache_read(data_cache, Pipe_Reg_EXtoMEM.REGS[instruction_d.rn] + instruction_d.dt_address);

            //preserve the first 16 bits
            val32 = val32 & 0xFFFF0000;
            val32_2 = val32_2 & 0xFFFF0000;

            //merge the 16 bits of info with the 24 bits
            val32 = val32 | (0x0000FFFF & Pipe_Reg_EXtoMEM.REGS[instruction_d.rt]);
            val32_2 = val32_2 | (0x0000FFFF & Pipe_Reg_EXtoMEM.REGS[instruction_d.rt]);

            if (DEBUG_CACHE) printf("_____Entered STURH with addr = %lx and val32 = %x\n",
                Pipe_Reg_EXtoMEM.REGS[instruction_d.rn] + instruction_d.dt_address + 4, val32);

            //write in the merged data to the address
            if (hit_or_miss)
                mem_write_32(Pipe_Reg_EXtoMEM.REGS[instruction_d.rn] + instruction_d.dt_address, val32);

            cache_write(data_cache, Pipe_Reg_EXtoMEM.REGS[instruction_d.rn] + instruction_d.dt_address, val32_2);

            break;
    }
}

void execute_iw(INSTRUCTION_IW instruction_iw) {

    if (DEBUG) printf("____________Entered EXECUTE_IW\n");

    //for the moment, the only IW type instruction is MOVZ
    if (instruction_iw.opcode == MOVZ) {
        Pipe_Reg_EXtoMEM.REGS[instruction_iw.rd] = instruction_iw.mov_immediate & 0x0000FFFF;
    }
}

//also return a number indicating which type of instruction it is, each type can be assigned a number
//in process_instruction we can then check which type of instruction the current instruction is, and then run the correct
//execute_(?) function based on that type
int decode(uint32_t curr_instr)
{
    //check if instruction type is HLT
    if (curr_instr == HLT_0)
        return INSTRUCTION_TYPE_HLT;

    //figure out what instruction format it is
    uint32_t opcode = 0xFC000000 & curr_instr;

    //then mask to check if type is B (opcode = 6 bits)

    if (opcode == B || opcode == BL) {
        if (DEBUG) printf("DEBUG: Instruction Type: B\n");
        if (DEBUG) printf("DEBUG: opcode = 0x%x\n", opcode);
        
        return INSTRUCTION_TYPE_B;
    }

    //then mask to check if type is CB (opcode = 8 bits)
    opcode = 0xFF000000 & curr_instr;

    if (opcode == CBNZ || opcode == CBZ || opcode == BCOND) {
        if (DEBUG) printf("DEBUG: Instruction Type: CB");
        if (DEBUG) printf("DEBUG: opcode = 0x%x\n", opcode);

        return INSTRUCTION_TYPE_CB;
    }

    //then mask to check if type is I (opcode = 10 bits)
    opcode = 0xFFC00000 & curr_instr;

    //keep in mind the special case of CMP essentially being SUBS
    //AKA it has the same opcode as SUBS, just with Rd field = 11111
    if (opcode == ADD_I || opcode == ADDS_I || opcode == SUB_I ||
        opcode == SUBS_I || opcode == LSL || opcode == LSR) { 
        if (DEBUG) printf("DEBUG: Instruction Type: I\n");
        if (DEBUG) printf("DEBUG: opcode = 0x%x\n", opcode);

        return INSTRUCTION_TYPE_I;
    }

    //then mask to check if type is R, D, or IW (each opcode = 11 bits)
    opcode = 0xFFE00000 & curr_instr;

    //check if type is R
    //watch out for CMP again
    if (opcode == ADD_reg || opcode == ADDS_reg || opcode == AND ||
        opcode == ANDS || opcode == EOR || opcode == ORR ||
        opcode == SUB_reg || opcode == SUBS_reg || opcode == MUL ||
        opcode == CMP_reg || opcode == BR) {
        if (DEBUG) printf("DEBUG: Instruction Type: R\n");
        if (DEBUG) printf("DEBUG: opcode = 0x%x\n", opcode);

        return INSTRUCTION_TYPE_R;
    }
    
    //then check if type is D
    if (opcode == LDUR || opcode == LDURB || opcode == LDURH ||
        opcode == STUR || opcode == STURB || opcode == STURH) {
        if (DEBUG) printf("DEBUG: Instruction Type: D\n");
        if (DEBUG) printf("DEBUG: opcode = 0x%x\n", opcode);
        
        return INSTRUCTION_TYPE_D;
    }

    //then check if type is IW
    //the only IW type instruction at the moment is MOVZ
    if (opcode == MOVZ) {
        if (DEBUG) printf("DEBUG: Instruction Type: IW\n");

        return INSTRUCTION_TYPE_IW;
    }
}

void decode_b(PIPELINED_INSTRUCTION *pipeline_instr) {
    INSTRUCTION_B instruction_b;
    uint32_t instruction = pipeline_instr->instruction;
    
    //assign B instruction's opcode
    instruction_b.opcode = 0xFC000000 & instruction;

    //mask to get ONLY the 26-bit immediate offset
    instruction_b.br_address = 0x03FFFFFF & instruction;

    //sign extend to account for negative br_address offsets
    if (instruction & 0x02000000) {
        instruction_b.br_address |= 0xFC000000;
    }

    ////////////////////////////////////////////

    pipeline_instr->curr_stage = STAGE_DECODE;

    pipeline_instr->instr_type = INSTRUCTION_TYPE_B;

    pipeline_instr->decoded_instruction.instruction_b = instruction_b;

    if (instruction_b.opcode == BL) {
        pipeline_instr->dest_reg = 30;
    }
    //doesn't use flags
}

void decode_cb(PIPELINED_INSTRUCTION *pipeline_instr) {
    INSTRUCTION_CB instruction_cb;
    uint32_t instruction = pipeline_instr->instruction;

    instruction_cb.opcode = 0xFF000000 & instruction;

    instruction_cb.cb_address = 0x00FFFFE0 & instruction;
    instruction_cb.cb_address = instruction_cb.cb_address >> 5;

    //sign extend cb_address to account for negative offset
    if (instruction_cb.cb_address & 0x00040000) {
        instruction_cb.cb_address |= 0xFFF80000;
    }

    instruction_cb.rt = 0x0000001F & instruction;

    instruction_cb.negation_flag = 0x00000001 & instruction;

    instruction_cb.cond = 0x0000000E & instruction;
    instruction_cb.cond = instruction_cb.cond >> 1;

    ///////////////////////////////////////////////////////

    pipeline_instr->curr_stage = STAGE_DECODE;

    pipeline_instr->instr_type = INSTRUCTION_TYPE_CB;

    pipeline_instr->decoded_instruction.instruction_cb = instruction_cb;

    pipeline_instr->dest_reg = instruction_cb.rt;

    //needs to use flags N and Z
    if (instruction_cb.opcode == CBNZ || instruction_cb.opcode == CBZ || instruction_cb.opcode == BCOND)
        pipeline_instr->needs_flags = 1;
}

void decode_i(PIPELINED_INSTRUCTION *pipeline_instr) {
    INSTRUCTION_I instruction_i;
	uint32_t instruction = pipeline_instr->instruction;

    instruction_i.opcode = 0xFFC00000 & instruction;

    instruction_i.alu_immediate = 0x003FFC00 & instruction;
    instruction_i.alu_immediate = instruction_i.alu_immediate >> 10;

    //sign extend alu_immediate to account for negatives
    if (instruction_i.alu_immediate & 0x00000800) {
        instruction_i.alu_immediate |= 0xFFFFF000;
    }
    
    instruction_i.rn = 0x000003E0 & instruction;
    instruction_i.rn = instruction_i.rn >> 5;

    instruction_i.rd = 0x0000001F & instruction;

	if (DEBUG) printf("DEBUG: >>>decode_i Rd = %i\n", instruction_i.rd);

	///////////////////////////////////////////////

	pipeline_instr->curr_stage = STAGE_DECODE;

	pipeline_instr->instr_type = INSTRUCTION_TYPE_I;

	pipeline_instr->decoded_instruction.instruction_i = instruction_i;

	pipeline_instr->src_reg1 = instruction_i.rn;

	pipeline_instr->dest_reg = instruction_i.rd;

	if (instruction_i.opcode == ADDS_I || instruction_i.opcode == SUBS_I)
		pipeline_instr->update_flags = 1;

}

void decode_r(PIPELINED_INSTRUCTION *pipeline_instr) {
    INSTRUCTION_R instruction_r;
	uint32_t instruction = pipeline_instr->instruction;

	instruction_r.opcode = 0xFFE00000 & instruction;

    instruction_r.rm = 0x001F0000 & instruction;
    instruction_r.rm = instruction_r.rm >> 16;

    instruction_r.rn = 0x000003E0 & instruction;
    instruction_r.rn = instruction_r.rn >> 5;

    instruction_r.rd = 0x0000001F & instruction;

	///////////////////////////////////////////////

	pipeline_instr->curr_stage = STAGE_DECODE;

	pipeline_instr->instr_type = INSTRUCTION_TYPE_R;

	pipeline_instr->decoded_instruction.instruction_r = instruction_r;

	pipeline_instr->src_reg0 = instruction_r.rm;
	pipeline_instr->src_reg1 = instruction_r.rn;

	pipeline_instr->dest_reg = instruction_r.rd;

	if (instruction_r.opcode == ADDS_reg || instruction_r.opcode == ANDS ||
		instruction_r.opcode == SUBS_reg) {
		pipeline_instr->update_flags = 1;
	}

	if (instruction_r.opcode == CMP_reg)
		pipeline_instr->needs_flags = 1;
	
}

void decode_d(PIPELINED_INSTRUCTION *pipeline_instr) {
    INSTRUCTION_D instruction_d;
    uint32_t instruction = pipeline_instr->instruction;

    instruction_d.opcode = 0xFFE00000 & instruction;
    
    instruction_d.dt_address = 0x001FF000 & instruction;
    instruction_d.dt_address = instruction_d.dt_address >> 12;

    //sign extend to account for negative dt_address values
    if (instruction_d.dt_address & 0x00000800) {
        instruction_d.dt_address |= 0xFFFFF000;
    }

    instruction_d.rn = 0x000003E0 & instruction;
    instruction_d.rn = instruction_d.rn >> 5;
    
    instruction_d.rt = 0x0000001F & instruction;

    ///////////////////////////////////////////////////

    pipeline_instr->curr_stage = STAGE_DECODE;

    pipeline_instr->instr_type = INSTRUCTION_TYPE_D;

    pipeline_instr->decoded_instruction.instruction_d = instruction_d;

    pipeline_instr->src_reg0 = instruction_d.rn;

    if (instruction_d.opcode == LDUR || instruction_d.opcode == LDURB || instruction_d.opcode == LDURH) {
        //write to destination register as usual
        pipeline_instr->dest_reg = instruction_d.rt;
    } else if (instruction_d.opcode == STUR || instruction_d.opcode == STURB || instruction_d.opcode == STURH) {
        //special case of STUR-type instructions using Rt not as a destination
        //instead, Rt holds what will be written into memory
        pipeline_instr->src_reg1 = instruction_d.rt;
    }

    //doesn't need to use or update flags
}

void decode_iw(PIPELINED_INSTRUCTION *pipeline_instr) {
    INSTRUCTION_IW instruction_iw;
    uint32_t instruction = pipeline_instr->instruction;

    instruction_iw.opcode = 0xFFE00000 & instruction;
    
    instruction_iw.mov_immediate = 0x001FFFE0 & instruction;
    instruction_iw.mov_immediate = instruction_iw.mov_immediate >> 5;

    //sign extend to account for negative mov_immediate value
    //0000 0000 0001 0000 0000 0000 0000 0000
    //0000 0000 0000 0000 1000 0000 0000 0000
    if (instruction_iw.mov_immediate & 0x00008000) {
        instruction_iw.mov_immediate |= 0xFFFF0000;
    }

    instruction_iw.rd = 0x0000001F & instruction;

    /////////////////////////////////////////////////////

    pipeline_instr->curr_stage = STAGE_DECODE;

    pipeline_instr->instr_type = INSTRUCTION_TYPE_IW;

    pipeline_instr->decoded_instruction.instruction_iw = instruction_iw;

    pipeline_instr->dest_reg = instruction_iw.rd;

    if (DEBUG) printf("@@@@ Decode_IW dest_reg: %d\n", instruction_iw.rd);

    //doesn't need to use or update flags
}

void pipe_init()
{
    memset(&CURRENT_STATE, 0, sizeof(CPU_State));
    CURRENT_STATE.PC = 0x00400000;

    memset(&bp_data, 0, sizeof(bp_data));

    //initialize instruction cache (64 sets, 4-way set-associative, 32 byte-size blocks)
    instr_cache = cache_new(64, 4, 32);

    //initialize data cache (256 sets, 8-way set-associative, 32 byte-size blocks)
    data_cache = cache_new(256, 8, 32);
}

void pipe_cycle()
{
    
    stall = 0;

    //(queue[tail].instr_type == INSTRUCTION_TYPE_B ||
    //     queue[tail].instr_type == INSTRUCTION_TYPE_CB ||
    //     (queue[tail].instr_type == INSTRUCTION_TYPE_R && queue[tail].decoded_instruction.instruction_r.opcode == BR)
    //    )

    if (instr_cache_stall == 0 && data_cache_stall == 2) {
        stall = 1;
    }

    if (data_cache_stall > 0) {
        data_cache_stall--;

        if (DEBUG_CACHE) printf("______________________________DATA_CACHE_STALL = %d\n", data_cache_stall);

        if (data_cache_stall == 0) {
            cache_write_block(data_cache, mem_wb_addr);
        }

        if (instr_cache_stall > 0) {
            instr_cache_stall--;
            if (DEBUG_CACHE) printf("______________________________INSTR_CACHE_STALL = %d\n", instr_cache_stall);

            if (instr_cache_stall == 0)
                cache_update(instr_cache, CURRENT_STATE.PC);
        } else if (instr_cache_stall == 0) {
            if (tail >= 0 && queue[tail].curr_stage != STAGE_FETCH) {
                pipe_stage_fetch();
            }
        }
        return;
    }
    pipe_stage_wb();
	pipe_stage_mem();
	pipe_stage_execute();
	pipe_stage_decode();

    if (instr_cache_stall > 0) {
        instr_cache_stall--;
        if (DEBUG_CACHE) printf("______________________________INSTR_CACHE_STALL = %d\n", instr_cache_stall);

        if (instr_cache_stall > 0)
            return;

        cache_update(instr_cache, CURRENT_STATE.PC);
    }

	pipe_stage_fetch();
}

//checks if current instruction (arg 0) is depended on by future instruction (arg 1)
//if so, return 1 and update the appropriate pipeline register. return 0 otherwise and do nothing
int has_dependency(PIPELINED_INSTRUCTION curr_instr, PIPELINED_INSTRUCTION potential_dependent)
{
    //if there is only one instruction, a dependency is not possible
    if (tail == 0)
        return -1;
        
    //make sure all registers involved are actually valid and not = -1
    if (curr_instr.dest_reg != -1 && 
        (potential_dependent.src_reg0 != -1 ||
        potential_dependent.src_reg1 != -1)) {
        
        //check for any dependencies
        if (curr_instr.dest_reg == potential_dependent.src_reg0 ||
            curr_instr.dest_reg == potential_dependent.src_reg1) {

            if (DEBUG) printf("______Depedency found at REGISTER %d\n", curr_instr.dest_reg);

            //not sure yet

            return 1;
        }
    }
    return 0;
}

int will_stall(PIPELINED_INSTRUCTION curr_instr, PIPELINED_INSTRUCTION potential_dependent)
{
	//do nothing for a cycle

    //case: LDUR followed by a dependency in the next instruction
    //LDUR finished EX stage, next instruction finished DE stage, so we know where the dependency is now
    //stall next_instr for one cycle before it can start EX stage so that LDUR can complete the MEM stage
    //and have the value to pass back

    //skip passing along the pipeline registers for one cycle

    //will be called at the end of execute, so the next instruction should have just finished fetch and is preparing to 
    //decode, due to the nature of the order of how the pipe_stage_X() functions are called
    if (has_dependency(curr_instr, potential_dependent) && curr_instr.decoded_instruction.instruction_d.opcode == LDUR &&
        potential_dependent.curr_stage == STAGE_FETCH) {
        return 1;
    }

    return 0;
}

void pipe_stage_wb()
{
	if (tail < 0)
		return;

	if (DEBUG) printf("---CURRENT STAGE: WB---\n");

	if (DEBUG) printf("\nDEBUG: Dest_reg = %i\n", queue[tail].dest_reg);
	if (DEBUG) printf("\nDEBUG: value in Pipe_Reg = %li\n", Pipe_Reg_MEMtoWB.REGS[queue[tail].dest_reg]);

    if (queue[0].curr_stage != STAGE_MEM)
        return;
    stat_inst_retire++;

    if (queue[0].instruction == HLT_0) { //first check if the instruction is an HLT 0 instruction
        RUN_BIT = 0;
        return;
    }

    queue[0].curr_stage = STAGE_WB;

    if (queue[0].update_flags) {
        CURRENT_STATE.FLAG_N = Pipe_Reg_MEMtoWB.FLAG_N;
        CURRENT_STATE.FLAG_Z = Pipe_Reg_MEMtoWB.FLAG_Z;
    }

    if (queue[0].dest_reg != -1) {
        
        if (DEBUG) printf("______WB REWRITING REGISTER %d\n", queue[0].dest_reg);

        if (tail >= 1 && (queue[0].dest_reg != queue[1].dest_reg))
            Pipe_Reg_EXtoMEM.REGS[queue[0].dest_reg] = Pipe_Reg_MEMtoWB.REGS[queue[0].dest_reg];

        Pipe_Reg_DEtoEX.REGS[queue[0].dest_reg] = Pipe_Reg_MEMtoWB.REGS[queue[0].dest_reg];
        Pipe_Reg_IFtoDE.REGS[queue[0].dest_reg] = Pipe_Reg_MEMtoWB.REGS[queue[0].dest_reg];

        CURRENT_STATE.REGS[queue[0].dest_reg] = Pipe_Reg_MEMtoWB.REGS[queue[0].dest_reg];
        
    }
    
    //clear out the finished instruction from the queue
    for (int i = 0; i < tail; i++) {
        queue[i] = queue[i+1];
    }
    tail--;
}

void pipe_stage_mem()
{
	if (tail < 0)
		return;

	if (DEBUG) printf("---CURRENT STAGE: MEMORY---\n");
	Pipe_Reg_MEMtoWB = Pipe_Reg_EXtoMEM;

	for (int i = 0; i <= tail; i++) {
		if (queue[i].curr_stage != STAGE_EXECUTE)
			continue;

        if (DEBUG) {
            if (queue[i].src_reg0 != -1) {
                printf("__Pipe_EXECUTE: SRC_REG 0 = %x & SRC_REG 0 val = %lx\n", queue[i].src_reg0, Pipe_Reg_MEMtoWB.REGS[queue[i].src_reg0]);
            }
            if (queue[i].src_reg1 != -1) {
                printf("__Pipe_EXECUTE: SRC_REG 1 = %x & SRC_REG 1 val = %lx\n", queue[i].src_reg1, Pipe_Reg_MEMtoWB.REGS[queue[i].src_reg1]);
            }
            if (queue[i].dest_reg != -1) {
                printf("__Pipe_EXECUTE: DEST_REG = %x & DEST_REG val = %lx\n", queue[i].dest_reg, Pipe_Reg_MEMtoWB.REGS[queue[i].dest_reg]);
            }
        }

        queue[i].curr_stage = STAGE_MEM;
        
        //if branch, squash the stalled instruction at IF and branch to target address calculate and updated by the
        //execute_b and execute_cb functions
        /*if (queue[i].instr_type == INSTRUCTION_TYPE_B || queue[i].instr_type == INSTRUCTION_TYPE_CB ||
            (queue[i].instr_type == INSTRUCTION_TYPE_R && queue[i].decoded_instruction.instruction_r.opcode == BR)) {
            
            if (Pipe_Reg_MEMtoWB.PC != Pipe_Reg_IFtoDE.PC) {
                CURRENT_STATE.PC = Pipe_Reg_MEMtoWB.PC;
                if (DEBUG) printf("___________IN MEM, Branching to PC @ %lx\n", CURRENT_STATE.PC);

                if (tail >= 0 && queue[tail].curr_stage == STAGE_FETCH) {
                    tail--;
                }
                return;
            }
        }
        */

        //write back the flags
        if (queue[i].update_flags) {
            Pipe_Reg_IFtoDE.FLAG_N = Pipe_Reg_MEMtoWB.FLAG_N;
            Pipe_Reg_IFtoDE.FLAG_Z = Pipe_Reg_MEMtoWB.FLAG_Z;

            Pipe_Reg_DEtoEX.FLAG_N = Pipe_Reg_MEMtoWB.FLAG_N;
            Pipe_Reg_DEtoEX.FLAG_Z = Pipe_Reg_MEMtoWB.FLAG_Z;
        }

        if (queue[i].instr_type == INSTRUCTION_TYPE_D) {

            INSTRUCTION_D curr_instr_d = queue[i].decoded_instruction.instruction_d;

            //1 hit, 0 miss
            int hit_or_miss = cache_update(data_cache, Pipe_Reg_EXtoMEM.REGS[curr_instr_d.rn] + curr_instr_d.dt_address);

            execute_d(queue[i].decoded_instruction.instruction_d, hit_or_miss);

            if (hit_or_miss) {
                //if hit
                
            } else {
                //if miss
                mem_wb_addr = Pipe_Reg_EXtoMEM.REGS[curr_instr_d.rn] + curr_instr_d.dt_address;
                data_cache_stall = 50;
            }

            if(i != tail && has_dependency(queue[i], queue[i+1])) {
                stall = 1;
                return;
            }
        }

        if ((i < tail) &&
            (queue[i].dest_reg != -1) &&
            (queue[i].dest_reg != queue[i+1].dest_reg)) {
            
            Pipe_Reg_EXtoMEM.REGS[queue[i].dest_reg] = Pipe_Reg_MEMtoWB.REGS[queue[i].dest_reg];
        }
 
        Pipe_Reg_DEtoEX.REGS[queue[i].dest_reg] = Pipe_Reg_MEMtoWB.REGS[queue[i].dest_reg];
        Pipe_Reg_IFtoDE.REGS[queue[i].dest_reg] = Pipe_Reg_MEMtoWB.REGS[queue[i].dest_reg];

	}
}

void pipe_stage_execute()
{
	if (tail < 0 || reached_HLT)
		return;

    if (stall)
        return;

	if (DEBUG) printf("---CURRENT STAGE: EXECUTE---\n");

	Pipe_Reg_EXtoMEM = Pipe_Reg_DEtoEX;

	INSTRUCTION_TYPE instr_type;
	int i = 0;
	for (i = 0; i <= tail; i++) {
		if (queue[i].curr_stage != STAGE_DECODE)
			continue;

		if (DEBUG) printf("\nDEBUG: current stage = %i\n", queue[i].curr_stage);
		queue[i].curr_stage = STAGE_EXECUTE;

        if (DEBUG) {
            if (queue[i].src_reg0 != -1) {
                printf("__Pipe_EXECUTE: SRC_REG 0 = %x & SRC_REG 0 val = %lx\n", queue[i].src_reg0, Pipe_Reg_EXtoMEM.REGS[queue[i].src_reg0]);
            }
            if (queue[i].src_reg1 != -1) {
                printf("__Pipe_EXECUTE: SRC_REG 1 = %x & SRC_REG 1 val = %lx\n", queue[i].src_reg1, Pipe_Reg_EXtoMEM.REGS[queue[i].src_reg1]);
            }
            if (queue[i].dest_reg != -1) {
                printf("__Pipe_EXECUTE: DEST_REG = %x & DEST_REG val = %lx\n", queue[i].dest_reg, Pipe_Reg_EXtoMEM.REGS[queue[i].dest_reg]);
            }
        }

		switch (queue[i].instr_type) {
			case INSTRUCTION_TYPE_B:
                execute_b(queue[i].decoded_instruction.instruction_b, &(queue[i].predict));
				break;
			case INSTRUCTION_TYPE_CB:
                execute_cb(queue[i].decoded_instruction.instruction_cb, &(queue[i].predict));
				break;
			case INSTRUCTION_TYPE_I:
				execute_i(queue[i].decoded_instruction.instruction_i);
				break;
			case INSTRUCTION_TYPE_R:
				execute_r(queue[i].decoded_instruction.instruction_r, &(queue[i].predict));
				break;
			//case INSTRUCTION_TYPE_D:
			//	memory stage will handle it
			case INSTRUCTION_TYPE_IW:
                execute_iw(queue[i].decoded_instruction.instruction_iw);
				break;
            case INSTRUCTION_TYPE_HLT:
                if (DEBUG) printf("DEBUG: ###Reached HLT###\n");
                reached_HLT = 1;
                break;
		}

        //account for the need to cancel a cache miss stall if a predicted PC address is incorrect,
        //meaning the next instr's addr is already in cache (so you cancel the stall and proceed as normal)
        //is instruction a branch?
        if (queue[i].instr_type == INSTRUCTION_TYPE_B || queue[i].instr_type == INSTRUCTION_TYPE_CB ||
            (queue[i].instr_type == INSTRUCTION_TYPE_R && queue[i].decoded_instruction.instruction_r.opcode == BR)) {
            
            //is the cpu currently stalling to wait for the instr cache miss + mem_fetch to finish
            if (instr_cache_stall > 0) {
                if (DEBUG_CACHE) printf("______________________BRANCH BUT CURRENTLY STALLING\n");
                uint64_t block1 = Pipe_Reg_EXtoMEM.PC >> 5;
                uint64_t block2 = stalled_instr_addr >> 5;

                if (DEBUG_CACHE) printf("______________________pipe_reg.pc = %lx, current_state.pc = %lx\n",
                    Pipe_Reg_EXtoMEM.PC, stalled_instr_addr);

                //check if predicted is "incorrect"
                if (block1 != block2) {
                    if (DEBUG_CACHE) printf("______________________BLOCK1 != BLOCK2\n");
                    if (is_addr_in_cache(instr_cache, Pipe_Reg_EXtoMEM.PC)) {
                        //cancel stall if predicted address was incorrect
                        if (DEBUG_CACHE) printf("______________________CANCELED STALL\n");
                        instr_cache_stall = 0;
                    }
                } 
            }

            return;
        }

        if (queue[i].instr_type == INSTRUCTION_TYPE_D)
            return;

        //forward the dest_reg's data back to the appropriate pipeline register
        //if (has_dependency(queue[i], queue[i+1]) || has_dependency(queue[i], queue[i+2])) {
        //if (i != tail && (queue[i].dest_reg != queue[i+1].dest_reg)) {
        //    Pipe_Reg_DEtoEX.REGS[queue[i].dest_reg] = Pipe_Reg_EXtoMEM.REGS[queue[i].dest_reg];
        //    Pipe_Reg_IFtoDE.REGS[queue[i].dest_reg] = Pipe_Reg_EXtoMEM.REGS[queue[i].dest_reg];
        //}
            
        //}
	}
}

void pipe_stage_decode()
{
	if (tail < 0 || reached_HLT)
		return;

    if (stall)
        return;

	if (DEBUG) printf("---CURRENT STAGE: DECODE---\n");

	Pipe_Reg_DEtoEX = Pipe_Reg_IFtoDE;

	INSTRUCTION_TYPE instr_type;
	int i = 0;
	for (i = 0; i <= tail; i++) {
		if (queue[i].curr_stage != STAGE_FETCH)
			continue;

		instr_type = decode(queue[i].instruction);

		if (DEBUG) printf("DEBUG: instr_type = %i\n", instr_type);
		queue[i].curr_stage = STAGE_DECODE;

        if (DEBUG) {
            if (queue[i].src_reg0 != -1) {
                printf("__Pipe_EXECUTE: SRC_REG 0 = %x & SRC_REG 0 val = %lx\n", queue[i].src_reg0, Pipe_Reg_DEtoEX.REGS[queue[i].src_reg0]);
            }
            if (queue[i].src_reg1 != -1) {
                printf("__Pipe_EXECUTE: SRC_REG 1 = %x & SRC_REG 1 val = %lx\n", queue[i].src_reg1, Pipe_Reg_DEtoEX.REGS[queue[i].src_reg1]);
            }
            if (queue[i].dest_reg != -1) {
                printf("__Pipe_EXECUTE: DEST_REG = %x & DEST_REG val = %lx\n", queue[i].dest_reg, Pipe_Reg_DEtoEX.REGS[queue[i].dest_reg]);
            }
        }

		switch (instr_type) {
			case INSTRUCTION_TYPE_B:
                decode_b(&(queue[i]));
				break;
			case INSTRUCTION_TYPE_CB:
                decode_cb(&(queue[i]));
				break;
			case INSTRUCTION_TYPE_I:
				decode_i(&(queue[i]));
				if (DEBUG) printf("DEBUG: >>>>>>queue[i] dest_reg = %i\n", queue[i].dest_reg);
				break;
			case INSTRUCTION_TYPE_R:
				decode_r(&(queue[i]));
				break;
			case INSTRUCTION_TYPE_D:
                decode_d(&(queue[i]));
				break;
			case INSTRUCTION_TYPE_IW:
                decode_iw(&(queue[i]));
				break;
            case INSTRUCTION_TYPE_HLT:
                if (DEBUG) printf("___________Assigned queue[i].instr_type = HLT 0\n");
                queue[i].instr_type = INSTRUCTION_TYPE_HLT;
                break;
		}
		break;
	}
}

void pipe_stage_fetch()
{
    if (reached_HLT)
        return;

    if (stall)
        return;

    if (!is_addr_in_cache(instr_cache, CURRENT_STATE.PC)) {
        //if miss
        if (DEBUG_CACHE) printf("_____________________NOT IN CACHE\n");
        instr_cache_stall = 50;
        stalled_instr_addr = CURRENT_STATE.PC;
        return;
    }

	//fetch instruction and initialize all the important fields in a PIPELINED_INSTRUCTION struct
	tail++; //update tail so we are fetching into the newest pipelined instruction

	queue[tail].curr_stage = STAGE_FETCH;

	queue[tail].instr_type = INSTRUCTION_TYPE_INVALID;

    //queue[tail].instruction = mem_read_32(CURRENT_STATE.PC);
    queue[tail].instruction = cache_read(instr_cache, CURRENT_STATE.PC);

	queue[tail].src_reg0 = -1;
	queue[tail].src_reg1 = -1;
	
	queue[tail].dest_reg = -1;

	queue[tail].needs_flags = 0;
	queue[tail].update_flags = 0;

    Pipe_Reg_IFtoDE = CURRENT_STATE;

    bp_predict(CURRENT_STATE.PC, &(queue[tail].predict));
	
	CURRENT_STATE.PC = queue[tail].predict.predict_addr;

	// For debugging purposes
    if (DEBUG) printf("DEBUG: curr_instr = %x", queue[tail].instruction);
    if (DEBUG) printf("\n\n");
}