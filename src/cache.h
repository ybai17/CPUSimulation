// Names: Chanik Bryan Lee, Yilei Bai
// CNET IDs: bryan22lee, yileib

// The above are group members for this project.

/*
 * CMSC 22200, Fall 2016
 *
 * ARM pipeline timing simulator
 *
 */
#ifndef _CACHE_H_
#define _CACHE_H_

//0011 1110 0000
#define MASK_INSTR_INDEX 0x000003E0
//0001 1111 1110 0000
#define MASK_DATA_INDEX 0x00001FE0

#define MASK_INSTR_TAG 0xFFFFFC00
#define MASK_DATA_TAG 0xFFFFE000

#define DEBUG_CACHE (1)

#include <stdint.h>
//#include "shell.h"

extern uint32_t stat_cycles;
uint32_t mem_read_32(uint64_t address);
void mem_write_32(uint64_t address, uint32_t value);

typedef struct {
    int valid; //0 or 1
    uint64_t tag;
    uint32_t mem_blocks[8];
    int cycle_accessed; //records the clock cycle when the block was accessed, thus recording its age for use in replacement
} block_t;

typedef struct {
    block_t *p_blocks; //will be array of how many block_t's there are, depending on 4-way or 8-way (how many columns)
} set_t;

typedef struct {
    int sets;
    int ways;
    int block_size; //always 32 in this lab assignment (bytes)

    set_t *p_sets; //will be array of how many set_t's there will be (how many rows)
} cache_t;

cache_t *cache_new(int sets, int ways, int block);
void cache_destroy(cache_t *c);
int cache_update(cache_t *c, uint64_t addr);

uint32_t cache_read(cache_t *cache, uint64_t addr);
void cache_write(cache_t *cache, uint64_t addr, uint32_t data);
void cache_write_block(cache_t *cache, uint64_t addr);
int is_addr_in_cache(cache_t *cache, uint64_t addr);

#endif
