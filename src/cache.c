// Names: Chanik Bryan Lee, Yilei Bai
// CNET IDs: bryan22lee, yileib

// The above are group members for this project.

/*
 * CMSC 22200, Fall 2016
 *
 * ARM pipeline timing simulator
 *
 */

#include "cache.h"
#include <stdlib.h>
#include <stdio.h>

cache_t *cache_new(int sets, int ways, int block)
{
    cache_t *cache_out = calloc(1, sizeof(cache_t));

    cache_out->sets = sets;
    cache_out->ways = ways;
    cache_out->block_size = block;

    cache_out->p_sets = calloc(sets, sizeof(set_t));
    //cache_out->p_sets->p_blocks = calloc(ways, sizeof(block_t*));

    for (int i = 0; i < sets; i++) {
        (cache_out->p_sets + i)->p_blocks = calloc(ways, sizeof(block_t));
    }
    
    return cache_out;
}

void cache_destroy(cache_t *c)
{

}

//in my lab's context, this function checks to see if the cache hits or misses for the given addr
//if hit, return 1
//if miss, return 0 and update the cache and actually write the new value into it
int cache_update(cache_t *c, uint64_t addr)
{
    uint64_t index = 0;
    uint64_t tag = 0;
    if (c->ways == 4) {
        index = (MASK_INSTR_INDEX & addr) >> 5;
        tag = MASK_INSTR_TAG & addr;
    } else if (c->ways == 8) {
        index = (MASK_DATA_INDEX & addr) >> 5;
        tag = MASK_DATA_TAG & addr;
    }

    if (DEBUG_CACHE) printf("initialized index = %lx and tag = %lx\n", index, tag);

    //indexes match
    set_t *curr_set = c->p_sets + index;
    block_t *curr_block = NULL;

    //search for tag
    for (int j = 0; j < c->ways; j++) {
        curr_block = curr_set->p_blocks + j;
        if (curr_block->valid && curr_block->tag == tag) {
            //tags match, therefore we have a cache hit
            if (DEBUG_CACHE) printf("__________CACHE HIT @ tag = %lx for addr = %lx\n", curr_block->tag, addr);
            curr_block->cycle_accessed = stat_cycles;
            return 1;
        }
    }

    if (DEBUG_CACHE) printf("searched for tag and did not find\n");

    //case: miss
    //check if they're all invalid/empty, while also figuring out which one is the oldest
    block_t *oldest_offset = NULL;

    //get the 0 of the block the addr is in
    //___1 1100
    uint64_t addr_0 = addr & 0xFFFFFFE0;

    for (int i = 0; i < c->ways; i++) {
        curr_block = curr_set->p_blocks + i;
        if (oldest_offset == NULL || curr_block->cycle_accessed < oldest_offset->cycle_accessed) {
            //if (DEBUG_CACHE)
            oldest_offset = curr_block;
        }

        if (!curr_block->valid) {
            //we found an empty block so we fill it in
            curr_block->valid = 1;
            curr_block->tag = tag;

            for (int j = 0; j < 8; j++) {
                curr_block->mem_blocks[j] = mem_read_32(addr_0 + j * 4);
            }

            curr_block->cycle_accessed = stat_cycles;

            return 0;
        }
    }

    if (DEBUG_CACHE) printf("did not find empty block\n");

    //case: miss + all blocks were valid
    //now we find the oldest block and replace it
    if (DEBUG_CACHE) printf("replacing block %lx\n", oldest_offset->tag);

    oldest_offset->tag = tag;

    for (int j = 0; j < 8; j++) {
        oldest_offset->mem_blocks[j] = mem_read_32(addr_0 + j * 4);
    }

    oldest_offset->cycle_accessed = stat_cycles;

    return 0;
}

//assume cache_update has already been called, and that the data is already in the cache (valid bit = 1)
uint32_t cache_read(cache_t *cache, uint64_t addr) {

    uint64_t offset = (addr & 0x0000001C) >> 2;
    uint64_t index = 0;
    uint64_t tag = 0;

    uint32_t output = 0;

    if (cache->ways == 4) {
        index = (addr & MASK_INSTR_INDEX) >> 5;
        tag = addr & MASK_INSTR_TAG;
    } else if (cache->ways == 8) {
        index = (addr & MASK_DATA_INDEX) >> 5;
        tag = addr & MASK_DATA_TAG;
    }

    if (DEBUG_CACHE) printf("READ: index = %lx + tag = %lx + offset = %lx\n", index, tag, offset);

    //first locate the right set using the index
    set_t *curr_set = cache->p_sets + index;

    //then locate the right block using the tag
    block_t *curr_block = NULL;

    for (int i = 0; i < cache->ways; i++) {
        curr_block = curr_set->p_blocks + i;
        if (curr_block->tag == tag) {
            //found the right block with the matching tag
            if (DEBUG_CACHE) printf(">>>READ: found matching tag %lx\n", tag);
            break;
        }
    }

    //now locate the right element inside the block using the offset and assign val
    output = curr_block->mem_blocks[offset];

    return output;
}

void cache_write(cache_t *cache, uint64_t addr, uint32_t data) {

    uint64_t offset = (addr & 0x0000001C) >> 2;
    uint64_t index = 0;
    uint64_t tag = 0;

    if (cache->ways == 4) {
        index = (addr & MASK_INSTR_INDEX) >> 5;
        tag = addr & MASK_INSTR_TAG;
    } else if (cache->ways == 8) {
        index = (addr & MASK_DATA_INDEX) >> 5;
        tag = addr & MASK_DATA_TAG;
    }

    if (DEBUG_CACHE) printf("WRITE: index = %lx + tag = %lx + offset = %lx\n", index, tag, offset);

    //first locate the right set using the index
    set_t *curr_set = cache->p_sets + index;

    //then locate the right block using the tag
    block_t *curr_block = NULL;

    for (int i = 0; i < cache->ways; i++) {
        curr_block = curr_set->p_blocks + i;
        if (curr_block->tag == tag) {
            //found the right block with the matching tag
            if (DEBUG_CACHE) printf(">>>WRITE: found matching tag %lx\n", tag);
            break;
        }
    }

    //now locate the right element inside the block using the offset and write the new data value
    curr_block->mem_blocks[offset] = data;

    return;

}

void cache_write_block(cache_t *cache, uint64_t addr) {

    uint64_t offset = (addr & 0x0000001C) >> 2;
    uint64_t index = 0;
    uint64_t tag = 0;

    if (cache->ways == 4) {
        index = (addr & MASK_INSTR_INDEX) >> 5;
        tag = addr & MASK_INSTR_TAG;
    } else if (cache->ways == 8) {
        index = (addr & MASK_DATA_INDEX) >> 5;
        tag = addr & MASK_DATA_TAG;
    }

    if (DEBUG_CACHE) printf("WRITE_BLOCK: index = %lx + tag = %lx + offset = %lx\n", index, tag, offset);

    //first locate the right set using the index
    set_t *curr_set = cache->p_sets + index;

    //then locate the right block using the tag
    block_t *curr_block = NULL;

    for (int i = 0; i < cache->ways; i++) {
        curr_block = curr_set->p_blocks + i;
        if (curr_block->tag == tag) {
            //found the right block with the matching tag
            if (DEBUG_CACHE) printf(">>>WRITE_BLOCK: found matching tag %lx\n", tag);
            break;
        }
    }

    //get the 0 of the block the addr is in
    //___1 1100
    uint64_t addr_0 = addr & 0xFFFFFFE0;

    //write the entire block into memory
    for (int i = 0; i < 8; i++) {
        mem_write_32(addr_0 + i * 4, curr_block->mem_blocks[i]);
    }
    
    return;

}

//function to check if the desired address in memory is currently stored in the cache
//return 1 if yes, 0 if no
int is_addr_in_cache(cache_t *cache, uint64_t addr) {

    uint64_t index = 0;
    uint64_t tag = 0;

    if (cache->ways == 4) {
        index = (addr & MASK_INSTR_INDEX) >> 5;
        tag = addr & MASK_INSTR_TAG;
    } else if (cache->ways == 8) {
        index = (addr & MASK_DATA_INDEX) >> 5;
        tag = addr & MASK_DATA_TAG;
    }

    if (DEBUG_CACHE) printf("CHECK IN CACHE: index = %lx + tag = %lx\n", index, tag);

    //first locate the right set using the index
    set_t *curr_set = cache->p_sets + index;

    //then locate the right block using the tag
    block_t *curr_block = NULL;

    for (int i = 0; i < cache->ways; i++) {
        curr_block = curr_set->p_blocks + i;
        if (curr_block->tag == tag) {
            //found the right block with the matching tag
            if (DEBUG_CACHE) printf(">>>CHECK IN CACHE: found matching tag %lx\n", tag);

            return 1;
        }
    }

    return 0;
}