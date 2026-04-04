#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

/* 
 * MEMSIM — Level 2 Fragmentation Benchmark (C Translation)
 */
#define main level2_unused_main
#include "level2.c"
#undef main

#define SEED 99
#define TOTAL_OPS 1000

typedef struct {
    char* strategy_name;
    int alloc_succeeded;
    int alloc_failed;
    int free_calls;
    int total_free_bytes;
    int largest_free_block;
    int num_free_fragments;
    float fragmentation_ratio;
} BenchResult;

BenchResult run_benchmark(int strategy_type, char* name) {
    srand(SEED);
    mem_init();

    int live_ptrs[TOTAL_OPS];
    int head = 0, tail = 0;
    int alloc_ok = 0, alloc_fail = 0, free_count = 0;

    for (int step = 0; step < TOTAL_OPS; step++) {
        int size;
        if (step % 2 == 1) {
            size = (rand() % (128 - 4 + 1)) + 4;      // small: 4-128
        } else {
            size = (rand() % (4096 - 512 + 1)) + 512; // large: 512-4096
        }

        int ptr = mem_alloc(size, 0, strategy_type);
        if (ptr == -1) {
            alloc_fail++;
        } else {
            alloc_ok++;
            live_ptrs[tail++] = ptr;
        }

        // Free the oldest allocation every third step after step 100
        if (step > 100 && step % 3 == 0 && head < tail) {
            mem_free(live_ptrs[head++]);
            free_count++;
        }
    }

    MemStats s = mem_stats();
    BenchResult r;
    r.strategy_name = name;
    r.alloc_succeeded = alloc_ok;
    r.alloc_failed = alloc_fail;
    r.free_calls = free_count;
    r.total_free_bytes = s.total_free_bytes;
    r.largest_free_block = s.largest_free_block;
    r.num_free_fragments = s.num_free_fragments;
    r.fragmentation_ratio = s.fragmentation_ratio;
    return r;
}

void print_result(BenchResult r) {
    printf("\n  Strategy: %s\n", r.strategy_name);
    printf("    Alloc succeeded   : %d\n", r.alloc_succeeded);
    printf("    Alloc failed (OOM): %d\n", r.alloc_failed);
    printf("    Free calls        : %d\n", r.free_calls);
    printf("    Final free bytes  : %d\n", r.total_free_bytes);
    printf("    Largest free block: %d\n", r.largest_free_block);
    printf("    Free fragments    : %d\n", r.num_free_fragments);
    printf("    Fragmentation ratio: %.4f\n", r.fragmentation_ratio);
}

int main() {
    printf("============================================================\n");
    printf("MEMSIM — Level 2 Fragmentation Benchmark\n");
    printf("============================================================\n");

    BenchResult ff = run_benchmark(0, "first_fit");
    BenchResult bf = run_benchmark(1, "best_fit");

    print_result(ff);
    print_result(bf);

    printf("\n------------------------------------------------------------\n");
    char* winner = (ff.fragmentation_ratio <= bf.fragmentation_ratio) ? "first_fit" : "best_fit";
    float diff = (ff.fragmentation_ratio > bf.fragmentation_ratio) ? 
                 (ff.fragmentation_ratio - bf.fragmentation_ratio) : 
                 (bf.fragmentation_ratio - ff.fragmentation_ratio);
                 
    printf("  Winner: %s  (delta = %.4f)\n", winner, diff);
    printf("\n  Copy the table above into your README and add a paragraph\n");
    printf("  explaining WHY the winning strategy performed better on\n");
    printf("  this alternating small/large workload.\n");

    return 0;
}
