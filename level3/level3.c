/*
 * MEMSIM — Level 1 Skeleton (C)
 * =============================
 * Structure aligned with starter_harness.c.
 * Keep TODOs for implementation work.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ── Device constants ───────────────────────────────────────────────────────── */
#define DEVICE_RAM   (64 * 1024)
#define MIN_ALLOC    4
#define MAX_ALLOC    4096
#define ALIGNMENT    4
#define HEADER_SIZE  8

#define STATUS_FREE  0
#define STATUS_USED  1
/* ── Task constraints ───────────────────────────────────────────────────────── */
#define MAX_TASKS 101
/* ── Global RAM ─────────────────────────────────────────────────────────────── */
static uint8_t ram[DEVICE_RAM];

/* ── Header layout (packed, no compiler padding) ───────────────────────────── */
#pragma pack(push, 1)
typedef struct {
    uint32_t size;      /* usable bytes */
    uint8_t  status;    /* FREE or USED */
    uint16_t task_id;
    uint8_t  checksum;  /* XOR of bytes 0..6 */
} BlockHeader;
#pragma pack(pop)

/* ── Header helpers ─────────────────────────────────────────────────────────── */
static uint8_t _compute_checksum(uint32_t size, uint8_t status, uint16_t task_id) {
    uint8_t cs = 0;
    uint8_t buf[7];
    memcpy(buf,   &size,    4);
    buf[4] = status;
    memcpy(buf + 5, &task_id, 2);
    for (int i = 0; i < 7; i++) cs ^= buf[i];
    return cs;
}

static void _write_header(int offset, uint32_t size, uint8_t status, uint16_t task_id) {
    BlockHeader *h = (BlockHeader *)(ram + offset);
    h->size     = size;
    h->status   = status;
    h->task_id  = task_id;
    h->checksum = _compute_checksum(size, status, task_id);
}

static BlockHeader _read_header(int offset, bool *checksum_ok) {
    BlockHeader h;
    memcpy(&h, ram + offset, HEADER_SIZE);
    uint8_t expected = _compute_checksum(h.size, h.status, h.task_id);
    if (checksum_ok) *checksum_ok = (h.checksum == expected);
    return h;
}
/* ── Level 3 ──────────────────────────────────────────────────────────────────────────────────────────────────── */
typedef struct {
    int  task_id;
    int  quota_bytes;
    int  bytes_allocated;   /* peak allocated */
    bool quota_exceeded;
    /* leaks: scan after fn() returns for blocks still tagged with task_id */
} TaskReport;

TaskReport task_table[MAX_TASKS];

typedef void (*TaskFn)(void);

TaskReport task_spawn(int task_id, int quota_bytes, TaskFn fn) {
    TaskReport r = {task_id, quota_bytes, 0, false};
    task_table[task_id] = r;
    if (fn!=NULL) fn();
    int offset = 0;

    while (offset + HEADER_SIZE <= DEVICE_RAM) {
        bool csum_ok;
        BlockHeader h = _read_header(offset, &csum_ok);
        if (h.status == STATUS_USED &&  task_id == h.task_id) {
            printf("Leak occurred\n");
            printf("[offset=%4d  size=%-6u  status=%-4s  task=%-3d  csum=%s ]\n",
               offset, 
               h.size,
               (h.status == STATUS_USED ? "USED" : "FREE"),
               h.task_id,
               (csum_ok ? "OK" : "ERR"));
        }
        offset += HEADER_SIZE + h.size;
    }
    return task_table[task_id];
}

int current_usage[MAX_TASKS]; 

/* ── Level 2 ──────────────────────────────────────────────────────────────────────────────────────────────────── */
/* A custom helper function to coalesce all free blocks*/
static void _coalesce_all() {
    int offset = 0;
    bool is_valid;
    BlockHeader h = _read_header(offset, &is_valid);

    while (is_valid && (offset + HEADER_SIZE <= DEVICE_RAM)) {
        // Calculate where the NEXT block starts
        int next_offset = offset + HEADER_SIZE + h.size;
        if (next_offset + HEADER_SIZE <= DEVICE_RAM) {
            bool next_valid;
            BlockHeader next_h = _read_header(next_offset, &next_valid);
            if (next_valid && h.status == STATUS_FREE && next_h.status == STATUS_FREE) {
                uint32_t new_size = h.size + HEADER_SIZE + next_h.size;
                _write_header(offset, new_size, STATUS_FREE, 0);
                h = _read_header(offset, &is_valid);
                continue; 
            }
        }
        offset = next_offset;
        if (offset + HEADER_SIZE <= DEVICE_RAM) {
            h = _read_header(offset, &is_valid);
        } else {
            is_valid = false;
        }
    }
}
void mem_init(void) {
    // Set everything to zero
    memset(ram, 0, DEVICE_RAM);
    memset(current_usage, 0, sizeof(current_usage)); 
    memset(task_table, 0, sizeof(task_table));
    _write_header(0, DEVICE_RAM - HEADER_SIZE, STATUS_FREE, 0); 
}
int mem_alloc(int n_bytes, int task_id, int strategy) {
    if (current_usage[task_id] + n_bytes > task_table[task_id].quota_bytes) {
        task_table[task_id].quota_exceeded = true;
        printf("Cannot allocate %d bytes", n_bytes);
        return -1;
    }
    if (n_bytes > MAX_ALLOC) return -1;
    if (n_bytes < MIN_ALLOC) n_bytes = MIN_ALLOC;
    if (n_bytes % ALIGNMENT != 0) n_bytes = ((n_bytes / ALIGNMENT) + 1) * ALIGNMENT;

    int offset = 0;
    bool is_valid;
    
    if (strategy == 0) { // First Fit
        BlockHeader h = _read_header(offset, &is_valid);
        while (is_valid && (offset + HEADER_SIZE <= DEVICE_RAM)) {
            if (h.status == STATUS_FREE && h.size >= (uint32_t)n_bytes) {
                uint32_t diff = h.size - n_bytes;
                if (diff >= HEADER_SIZE + MIN_ALLOC) {
                    _write_header(offset, n_bytes, STATUS_USED, task_id);
                    _write_header(offset + HEADER_SIZE + n_bytes, diff - HEADER_SIZE, STATUS_FREE, 0);
                } else {
                    _write_header(offset, h.size, STATUS_USED, task_id);
                }
                current_usage[task_id]+=n_bytes;
                if (task_table[task_id].bytes_allocated < current_usage[task_id]) 
                    task_table[task_id].bytes_allocated = current_usage[task_id]; 
                return offset + HEADER_SIZE;
            }
            offset += HEADER_SIZE + h.size;
            if (offset + HEADER_SIZE <= DEVICE_RAM) h = _read_header(offset, &is_valid);
            else is_valid = false;
        }
    } else { // Best Fit
        int best_offset = -1;
        uint32_t min_size = DEVICE_RAM + 1;
        BlockHeader h = _read_header(offset, &is_valid);
        while (is_valid && (offset + HEADER_SIZE <= DEVICE_RAM)) {
            if (h.status == STATUS_FREE && h.size >= (uint32_t)n_bytes) {
                if (h.size < min_size) {
                    min_size = h.size;
                    best_offset = offset;
                }
            }
            offset += HEADER_SIZE + h.size;
            if (offset + HEADER_SIZE <= DEVICE_RAM) h = _read_header(offset, &is_valid);
            else is_valid = false;
        }
        if (best_offset != -1) {
            uint32_t diff = min_size - n_bytes;
            if (diff >= HEADER_SIZE + MIN_ALLOC) {
                _write_header(best_offset, n_bytes, STATUS_USED, task_id);
                _write_header(best_offset + HEADER_SIZE + n_bytes, diff - HEADER_SIZE, STATUS_FREE, 0);
            } else {
                _write_header(best_offset, min_size, STATUS_USED, task_id);
            }
            current_usage[task_id]+=n_bytes;
            if (task_table[task_id].bytes_allocated < current_usage[task_id]) 
                task_table[task_id].bytes_allocated = current_usage[task_id]; 
            return best_offset + HEADER_SIZE;
        }
    }
    return -1;
}

bool mem_free(int ptr) {
    if (ptr < HEADER_SIZE || ptr >= DEVICE_RAM || (ptr % ALIGNMENT) != 0) return false;
    
    int offset = ptr - HEADER_SIZE;
    bool is_valid;
    BlockHeader h = _read_header(offset, &is_valid);
    
    if (is_valid && h.status == STATUS_USED) {
        _write_header(offset, h.size, STATUS_FREE, 0);
        current_usage[h.task_id] -= h.size;
        _coalesce_all();
        
        return true;
    }
    return false;
}

void mem_dump(void) {
    int offset = 0;
    int n_blocks = 0, n_used = 0, n_free = 0;
    uint32_t free_bytes = 0;
    while (offset + HEADER_SIZE <= DEVICE_RAM) {
        bool csum_ok;
        BlockHeader h = _read_header(offset, &csum_ok);

        printf("[offset=%4d  size=%-6u  status=%-4s  task=%-3d  csum=%s ]\n",
               offset, 
               h.size,
               (h.status == STATUS_USED ? "USED" : "FREE"),
               h.task_id,
               (csum_ok ? "OK" : "ERR"));

        n_blocks++;
        if (h.status == STATUS_USED) {
            n_used++;
        } else {
            n_free++;
            free_bytes += h.size;
        }

        offset += HEADER_SIZE + h.size;
    }
    
    printf("HEAP SUMMARY: %d blocks | %d used | %d free | %u free bytes\n", 
           n_blocks, n_used, n_free, free_bytes);
}


typedef struct {
    int   total_free_bytes;
    int   largest_free_block;
    int   num_free_fragments;
    float fragmentation_ratio;
} MemStats;

MemStats mem_stats(void) {
    /* TODO */
    int total_free_bytes = 0;
    int largest_free_block = 0;
    int num_free_fragments = 0;
    int offset = 0;
    bool is_valid;
    BlockHeader h = _read_header(offset, &is_valid);
    while (is_valid && (offset+h.size+HEADER_SIZE)<=DEVICE_RAM && h.size>0) {
        if (h.status == STATUS_FREE) {
            if ((uint32_t) largest_free_block < h.size) largest_free_block = h.size;
            total_free_bytes += h.size;
            num_free_fragments += 1;
        }
        offset += HEADER_SIZE + h.size;
        if (offset + HEADER_SIZE <= DEVICE_RAM)
            h = _read_header(offset, &is_valid);
    }

    MemStats s;
    s.total_free_bytes = total_free_bytes;
    s.largest_free_block = largest_free_block;
    s.num_free_fragments = num_free_fragments;
    if (total_free_bytes!=0)
        s.fragmentation_ratio = 1.0 - ((float) largest_free_block / (float) total_free_bytes);
    else
        s.fragmentation_ratio = 0.0f;
    return s;
}


/* ── Minimal sanity main ────────────────────────────────────────────────────── */
// TASK 1
// Allocates 100, then frees it. Shows 0 leaks and 100 peak.
void task_good(void) {
    int p = mem_alloc(100, 1, 0);
    if (p != -1) mem_free(p);
}

// TASK 2
// Allocates 200 but NEVER frees. Shows a leak in the scan.
void task_leaky(void) {
    mem_alloc(200, 2, 0); 
}

// TASK 3
// Has a quota of 500, but tries to take 1000. mem_alloc returns -1.
void task_greedy(void) {
    int p = mem_alloc(1000, 3, 0);
    if (p == -1) {
        printf("[Task 3] My allocation was blocked by the Supervisor!\n");
    }
}
int main(void) {
    printf("========== LEVEL 3 SYSTEM TEST ==========\n");
    mem_init(); // This should reset your task_table and current_usage!

    // Run Task 1
    TaskReport r1 = task_spawn(1, 1000, task_good);
    printf("REPORT Task 1: Peak=%d, Exceeded=%s\n", 
           r1.bytes_allocated, r1.quota_exceeded ? "YES" : "NO");

    // Run Task 2
    TaskReport r2 = task_spawn(2, 1000, task_leaky);
    printf("REPORT Task 2: Peak=%d, Exceeded=%s\n", 
           r2.bytes_allocated, r2.quota_exceeded ? "YES" : "NO");

    // Run Task 3
    TaskReport r3 = task_spawn(3, 500, task_greedy);
    printf("REPORT Task 3: Peak=%d, Exceeded=%s\n", 
           r3.bytes_allocated, r3.quota_exceeded ? "YES" : "NO");

    printf("\n--- FINAL HEAP MAP ---\n");
    mem_dump();
    
    return 0;
}