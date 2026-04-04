#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

/* Include your level1 code so we can test it directly */
#define main level1_unused_main
#include "level1.c"
#undef main

#define SEED 42

typedef struct {
    int ptr;
    int size;
    int canary_len;
} LivePtr;

void run_stress_test() {
    printf("  [....] 500-call random stress test\n");
    srand(SEED);
    mem_init();

    LivePtr live_ptrs[500];
    int live_count = 0;
    int errors = 0;

    for (int step = 0; step < 500; step++) {
        int action = rand() % 3; // 0,1 = alloc, 2 = free
        
        if (action < 2 || live_count == 0) {
            int size = (rand() % (MAX_ALLOC - MIN_ALLOC + 1)) + MIN_ALLOC;
            int ptr = mem_alloc(size, 1, 0);

            if (ptr == -1) continue;

            if (ptr < HEADER_SIZE) {
                printf("         -> Step %d: mem_alloc returned ptr=%d below HEADER_SIZE\n", step, ptr);
                errors++;
            } else if (ptr >= DEVICE_RAM) {
                printf("         -> Step %d: mem_alloc returned ptr=%d >= DEVICE_RAM\n", step, ptr);
                errors++;
            } else if (ptr % ALIGNMENT != 0) {
                printf("         -> Step %d: mem_alloc returned unaligned ptr=%d\n", step, ptr);
                errors++;
            } else {
                int canary_len = (size < 8) ? size : 8;
                for (int i = 0; i < canary_len; i++) ram[ptr + i] = (uint8_t)((ptr + i) & 0xFF);
                live_ptrs[live_count++] = (LivePtr){ptr, size, canary_len};
            }
        } else if (live_count > 0) {
            int idx = rand() % live_count;
            LivePtr lp = live_ptrs[idx];
            for (int i = 0; i < lp.canary_len; i++) {
                if (ram[lp.ptr + i] != (uint8_t)((lp.ptr + i) & 0xFF)) {
                    printf("         -> Step %d: canary corrupted at ram[%d]\n", step, lp.ptr + i);
                    errors++;
                }
            }
            if (!mem_free(lp.ptr)) {
                printf("         -> Step %d: mem_free(%d) returned false for live pointer\n", step, lp.ptr);
                errors++;
            }
            live_ptrs[idx] = live_ptrs[--live_count];
        }
    }
    for (int i = 0; i < live_count; i++) mem_free(live_ptrs[i].ptr);

    // Final Stats check
    MemStats s = mem_stats();
    if (s.total_free_bytes < DEVICE_RAM - HEADER_SIZE * 2) {
        printf("         -> After cleanup, only %d bytes free (leak/coalescing issue)\n", s.total_free_bytes);
        errors++;
    }
    if (errors == 0) printf("  [PASS] 500-call random stress test\n");
    else printf("  [FAIL] 500-call random stress test\n");
}

void run_double_free_test() {
    mem_init();
    int ptr = mem_alloc(64, 0, 0);
    mem_free(ptr);
    if (mem_free(ptr) == false) printf("  [PASS] Double-free protection\n");
    else printf("  [FAIL] Double-free of ptr=%d returned true\n", ptr);
}

void run_invalid_ptr_test() {
    mem_init();
    int bad_ptrs[] = {-1, 0, 3, DEVICE_RAM, DEVICE_RAM + 100};
    bool ok = true;
    for (int i = 0; i < 5; i++) {
        if (mem_free(bad_ptrs[i]) != false) { printf("         -> mem_free(%d) returned true\n", bad_ptrs[i]); ok = false; }
    }
    if (ok) printf("  [PASS] Invalid pointer rejection\n");
    else printf("  [FAIL] Invalid pointer rejection\n");
}

void run_alignment_test() {
    mem_init();
    int sizes[] = {4, 5, 7, 8, 13, 64, 100, 255, 256, 1000, 4096};
    bool ok = true;
    for (int i = 0; i < 11; i++) {
        int ptr = mem_alloc(sizes[i], 0, 0);
        if (ptr != -1 && ptr % ALIGNMENT != 0) { printf("         -> mem_alloc(%d) unaligned ptr=%d\n", sizes[i], ptr); ok = false; }
        if (ptr != -1) mem_free(ptr);
    }
    if (ok) printf("  [PASS] Alignment guarantee\n");
    else printf("  [FAIL] Alignment guarantee\n");
}

int main() {
    printf("============================================================\n");
    printf("MEMSIM — Level 1 Stress Test (C Translation)\n");
    printf("============================================================\n");
    run_stress_test();
    run_double_free_test();
    run_invalid_ptr_test();
    run_alignment_test();
    return 0;
}
