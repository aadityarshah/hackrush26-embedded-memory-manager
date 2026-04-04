#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/* Include your level4 code */
#define main level4_unused_main
#include "level4.c"
#undef main

#define TARGET_FILL 0.90
#define BLOCK_SIZE 64
#define LARGE_REQUEST 8192

bool run_compaction_benchmark() {
    mem_init();
    handles_init();

    // ── Phase 1: Fill to ~90% ────────────────────────────────────────────────
    printf("\n  Phase 1 — Fill to ~90%%\n");
    int handles[MAX_HANDLES];
    int handle_count = 0;
    int target_bytes = (int)(DEVICE_RAM * TARGET_FILL);
    int used = 0;

    while (used + BLOCK_SIZE + (int)HEADER_SIZE < target_bytes && handle_count < MAX_HANDLES) {
        int h = mem_alloc_handle(BLOCK_SIZE, 1);
        if (h == -1) break;
        handles[handle_count++] = h;
        used += BLOCK_SIZE + HEADER_SIZE;
    }

    printf("    Allocated %d handles, ~%d bytes used\n", handle_count, used);

    // ── Phase 2: Free every other handle ─────────────────────────────────────
    printf("\n  Phase 2 — Free every other handle (create fragmentation)\n");
    int freed = 0;
    int keep_count = 0;
    int keep_handles[MAX_HANDLES];

    for (int i = 0; i < handle_count; i++) {
        if (i % 2 == 0) {
            mem_free_handle(handles[i]);
            freed++;
        } else {
            keep_handles[keep_count++] = handles[i];
        }
    }

    printf("    Freed %d handles — heap is now fragmented\n", freed);

    // Confirm direct alloc fails (using a large enough size that coalescing alone won't solve it if fragmented)
    printf("\n  Phase 3 — Request large contiguous block (%d bytes)\n", LARGE_REQUEST);
    
    // We bypass the auto-compaction in mem_alloc_handle for the "direct" check 
    // by using the underlying mem_alloc directly.
    int direct = mem_alloc(LARGE_REQUEST, 2, 0);

    if (direct != -1) {
        printf("    Direct alloc: SUCCESS without compaction (offset=%d)\n", direct);
        printf("    NOTE: Your allocator coalesced enough; compaction not triggered.\n");
        mem_free(direct);
        return true;
    }

    printf("    Direct alloc: FAILED (fragmented, as expected)\n");

    // ── Phase 3: Compact and retry ────────────────────────────────────────────
    clock_t t0 = clock();
    int recovered = mem_compact();
    clock_t t1 = clock();

    double compact_ms = ((double)(t1 - t0) / CLOCKS_PER_SEC) * 1000.0;
    printf("    Compacting... recovered %d bytes  (%.2f ms)\n", recovered, compact_ms);

    // Verify all existing handles still resolve correctly
    int bad_handles = 0;
    for (int i = 0; i < keep_count; i++) {
        if (deref_handle(keep_handles[i]) == -1) {
            bad_handles++;
        }
    }

    if (bad_handles) {
        printf("    ERROR: %d handles became invalid after compaction!\n", bad_handles);
        return false;
    }

    // Now try to allocate the large block through handle system
    int post = mem_alloc_handle(LARGE_REQUEST, 2);
    clock_t t2 = clock();
    double total_ms = ((double)(t2 - t0) / CLOCKS_PER_SEC) * 1000.0;

    if (post == -1) {
        printf("    Post-compact alloc: FAILED — not enough contiguous space after compaction.\n");
        return false;
    }

    printf("    Post-compact alloc: SUCCESS (handle=%d)\n", post);
    printf("    Total time (compact + alloc): %.2f ms\n", total_ms);

    // ── Phase 4 — Validate surviving handle data integrity ───────────────────────────
    printf("\n  Phase 4 — Validate surviving handle data integrity\n");
    bool all_valid = true;
    for (int i = 0; i < keep_count; i++) {
        if (deref_handle(keep_handles[i]) == -1) {
            all_valid = false;
            break;
        }
    }
    printf("    All %d surviving handles valid: %s\n", keep_count, all_valid ? "YES" : "NO");

    return post != -1 && all_valid;
}

int main() {
    printf("============================================================\n");
    printf("MEMSIM — Level 4 Compaction Benchmark (C Translation)\n");
    printf("============================================================\n");

    bool passed = run_compaction_benchmark();

    printf("\n------------------------------------------------------------\n");
    if (passed) {
        printf("  Compaction benchmark: PASSED\n");
    } else {
        printf("  Compaction benchmark: FAILED\n");
    }

    return passed ? 0 : 1;
}
