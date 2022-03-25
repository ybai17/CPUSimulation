#include "cache.h"
#include <stdint.h>
#include <stdio.h>

uint32_t mem_read_32(uint64_t address) {
    printf("__read address @ %lx\n", address);
    return address;
}

uint32_t stat_cycles = 0;

int main() {
    cache_t *test = cache_new(64, 4, 32);

    printf("initialized test cache\n");

    printf("1: %d\n", cache_update(test, 0x00000018));
    stat_cycles++;
    printf("2: %d\n", cache_update(test, 0x10000008));
    stat_cycles++;
    printf("3: %d\n", cache_update(test, 0x00000018));
    stat_cycles++;
    printf("4: %d\n", cache_update(test, 0x20000008));
    stat_cycles++;
    printf("5: %d\n", cache_update(test, 0x30000008));
    stat_cycles++;
    printf("6: %d\n", cache_update(test, 0x40000008));
    stat_cycles++;
    printf("7: %d\n", cache_update(test, 0x50000008));
    stat_cycles++;
    printf("8: %d\n", cache_update(test, 0x30000008));
    stat_cycles++;
    printf("9: %d\n", cache_update(test, 0x10000008));
    stat_cycles++;
    printf("10: %d\n", cache_update(test, 0x40000008));
    stat_cycles++;

    printf("_____Cache read val = %x\n", cache_read(test, 0x40000008));
}