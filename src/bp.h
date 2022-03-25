// Names: Chanik Bryan Lee, Yilei Bai
// CNET IDs: bryan22lee, yileib

// The above are group members for this project.

/*
 * ARM pipeline timing simulator
 *
 * CMSC 22200, Fall 2016
 */

#ifndef _BP_H_
#define _BP_H_
#include <stdint.h>
#include "pipe.h"

#define PHT_SIZE 256
#define BTB_SIZE 1024
#define BTB_MASK 0x0FFC
#define GHR_MASK 0x03FC

#define STACK_SIZE 8

extern uint64_t addr_stack[STACK_SIZE];
extern int stack_top;

typedef struct btb
{
    // address tag (64 bits)
    uint64_t addr_tag;
    int valid_bit; // 0 or 1 (valid bit)
    int cond_bit; // 0 or 1 (conditional bit)
    int indirect; //1 or 0 (indirect branch or direct)
    uint64_t branch_target; // low two bits always 2'b00
} btb_t;

typedef struct bp
{
    //00 = strongly not taken
    //01 = weakly not taken
    //10 = weakly taken
    //11 = strongly taken
    short int pht[PHT_SIZE];

    // 1024 entries indexed by bits [11:2] of the PC
    btb_t btb[BTB_SIZE];
} bp_t;

void bp_predict(uint64_t addr, predict_t *p_predict);
void bp_update(uint64_t addr, int valid_bit, int cond_bit, uint64_t target, int taken, int indirect);

#endif
