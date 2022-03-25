// Names: Chanik Bryan Lee, Yilei Bai
// CNET IDs: bryan22lee, yileib

// The above are group members for this project.

/*
 * ARM pipeline timing simulator
 *
 * CMSC 22200, Fall 2016
 */

#include "bp.h"
#include <stdlib.h>
#include <stdio.h>

bp_t bp_data;

// 8-global branch history register (GHR)
uint8_t gshare;

//stack for BL + BR
uint64_t addr_stack[STACK_SIZE];
int stack_top = -1; //-1 = empty stack

//return 1 if we will take, 0 if we won't take
int check_GHR(uint64_t addr) {

    //get GHR_index by XOR-ing bits [9,2] of PC address with Gshare
    uint8_t PHT_index = (unsigned) (((addr & GHR_MASK) >> 2) ^ gshare);

    //now predict its direction using the PHT
    if (bp_data.pht[PHT_index] == 0b10 || bp_data.pht[PHT_index] == 0b11) { //weakly or strongly taken
        return 1;
    } else {
        //PHT[GHR_index] = 0b01 or 0b00 (weakly or strongly not taken, respectively)
        return 0;
    }
}

//call at fetch finish
void bp_predict(uint64_t addr, predict_t *p_predict)
{
    /* Predict next PC */

    //check if it's a branch
    //get index from PC
    uint16_t index_search = (unsigned) ((addr & BTB_MASK) >> 2); //bits [11,2] of PC that serve as index
    btb_t b_entry = bp_data.btb[index_search];

    if (b_entry.valid_bit && b_entry.addr_tag == addr) {
        p_predict->missed = 0;
        if (b_entry.indirect) {
            if (stack_top >= 0) { //if stack has address(es) in it 
                p_predict->predict_addr = addr_stack[stack_top];
                p_predict->predict_taken = 1;
            } else { //if stack is empty
                p_predict->predict_addr = addr + 4;
                p_predict->predict_taken = 1;
            }
            return;
        }
        if (b_entry.cond_bit == 0 || check_GHR(addr)) {
            p_predict->predict_taken = 1;
            p_predict->predict_addr = b_entry.branch_target;
            return;
        }

        p_predict->predict_taken = 0;
        p_predict->predict_addr = addr + 4;

        return;
    }
    
    p_predict->missed = 1;
    p_predict->predict_taken = 0;
    p_predict->predict_addr = addr + 4;
}

//indirect 1 = it is an indirect branch, 0 = it is a direct branch
void bp_update(uint64_t addr, int valid_bit, int cond_bit, uint64_t target, int taken, int indirect)
{
    /* Update BTB */
    uint16_t index_search = (unsigned) ((addr & BTB_MASK) >> 2); //bits [11,2] of PC that serve as index
    btb_t b_entry = bp_data.btb[index_search];

    if (b_entry.valid_bit == 0 || b_entry.addr_tag != addr) {
        b_entry.addr_tag = addr;
        b_entry.valid_bit = valid_bit;
        b_entry.cond_bit = cond_bit;
        b_entry.indirect = indirect;
        b_entry.branch_target = target;

        bp_data.btb[index_search] = b_entry;
    }

    if (cond_bit == 0) {
        return;
    }

    /* Update gshare directional predictor */

    //get GHR_index by XOR-ing bits [9,2] of PC address with Gshare
    uint8_t PHT_index = (unsigned) (((addr & GHR_MASK) >> 2) ^ gshare);

    short pht_entry = bp_data.pht[PHT_index];

    //if branch taken, PHT[entry]++ unless == 0b11
    if (taken) {
        if (pht_entry != 3)
            pht_entry++;
    } else {
        //if not taken, PHT[entry]-- unless == 0b00
        if (pht_entry != 0)
            pht_entry--;
    }

    bp_data.pht[PHT_index] = pht_entry;
        
    /* Update global history register */
    gshare <<= 1;
    //if no branch last cycle, least sig bit = 0
        //do nothing
    //if branch last cycle, least sig bit = 1
    if (taken)
        gshare |= 1;
    
}
