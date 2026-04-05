#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/* 
 * We include the source file directly so we can access internal 
 * globals like task_table and DEVICE_RAM for the benchmark.
 * Note: Make sure level4.c does NOT have a main() or use the 
 * #define main level4_main trick.
 */
#define main level4_unused_main
#include "level4.c"
#undef main

#define TARGET_FILL 0.90
#define BLOCK_SIZE 64
#define LARGE_REQUEST 8192

int main() {
    printf("============================================================\n");
    printf("MEMSIM — Level 4 Compaction C-Benchmark\n");
    printf("============================================================\n");

    mem_init();
    handles_init();

    // ── Phase 1: Fill to ~90% ────────────────────────────────────────────────
    printf("\nPhase 1 — Fill to ~90%%\n");
    
    int handles[1024];
    int total_handles = 0;
    int target_bytes = (int)(DEVICE_RAM * TARGET_FILL);
    int used_estimate = 0;

    // Set quota for Task 1 so allocations don't get rejected
    task_table[1].quota_bytes = DEVICE_RAM; 

    while (used_estimate + BLOCK_SIZE + HEADER_SIZE < target_bytes && total_handles < 1024) {
        int h = mem_alloc_handle(BLOCK_SIZE, 1);
        if (h == -1) break;
        handles[total_handles++] = h;
        used_estimate += (BLOCK_SIZE + HEADER_SIZE);
    }
    printf("    Allocated %d handles, ~%d bytes used\n", total_handles, used_estimate);

    // ── Phase 2: Free every other handle ─────────────────────────────────────
    printf("\nPhase 2 — Free every other handle (create fragmentation)\n");
    int freed_count = 0;
    for (int i = 0; i < total_handles; i += 2) {
        mem_free_handle(handles[i]);
        handles[i] = -1; // Mark as freed in our local tracker
        freed_count++;
    }
    printf("    Freed %d handles — heap is now fragmented\n", freed_count);

    // ── Phase 3: Request large block ─────────────────────────────────────────
    printf("\nPhase 3 — Request large block (%d bytes)\n", LARGE_REQUEST);
    
    // Set quota for Task 2
    task_table[2].quota_bytes = DEVICE_RAM;

    // First, verify that a normal mem_alloc (without compaction) would fail
    // Strategy 0 = First Fit
    int direct_fail = mem_alloc(LARGE_REQUEST, 2, 0, NO_HANDLE);
    if (direct_fail != -1) {
        printf("    [NOTE] Direct alloc succeeded without compaction. \n");
        printf("           Your allocator is very efficient at coalescing!\n");
    } else {
        printf("    Direct alloc: FAILED (fragmented, as expected)\n");
    }

    // Now, run the Level 4 wrapper that SHOULD trigger compaction
    clock_t t0 = clock();
    int h_large = mem_alloc_or_compact(LARGE_REQUEST, 2);
    clock_t t1 = clock();

    double elapsed_ms = ((double)(t1 - t0) / CLOCKS_PER_SEC) * 1000;

    if (h_large == -1) {
        printf("    Post-compact alloc: FAILED. Compaction did not create enough space.\n");
        return 1;
    }
    printf("    Post-compact alloc: SUCCESS (handle=%d)\n", h_large);
    printf("    Time taken: %.2f ms\n", elapsed_ms);

    // ── Phase 4: Validate Handle Integrity ───────────────────────────────────
    printf("\nPhase 4 — Validate surviving handle data integrity\n");
    int bad_handles = 0;
    for (int i = 1; i < total_handles; i += 2) {
        if (deref_handle(handles[i]) == -1) {
            bad_handles++;
        }
    }

    if (bad_handles > 0) {
        printf("    FAIL: %d handles became invalid after compaction!\n", bad_handles);
        printf("    (Check if mem_compact is patching the handle_table correctly)\n");
        return 1;
    } else {
        printf("    All surviving handles valid: YES\n");
    }

    printf("\n------------------------------------------------------------\n");
    printf("  Compaction benchmark: PASSED\n");
    printf("------------------------------------------------------------\n");

    return 0;
}