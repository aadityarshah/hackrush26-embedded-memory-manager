/*
 * MEMSIM — Level 4 (C)
 * =============================
 * Architecture complete through Level 3.
 * Level 4 contains harness placeholders.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ── Device constants ───────────────────────────────────────────────────────── */
#define DEVICE_RAM   (64 * 1024)
#define MIN_ALLOC    4
#define MAX_ALLOC    (48*1024)
#define ALIGNMENT    4

#define STATUS_FREE  0
#define STATUS_USED  1
/* ── Tasks parameters ───────────────────────────────────────────────────────── */
#define MAX_TASKS 101
#define STATUS_IDLE 0
#define STATUS_ACTIVE 1
/*── Handle parameters ───────────────────────────────────────────────────────── */
#define NO_HANDLE 0xFFFFFFFF  // A magic number for "No Handle"
/* ── Global RAM ─────────────────────────────────────────────────────────────── */
static uint8_t ram[DEVICE_RAM];

/* ── Header layout (packed, no compiler padding) ───────────────────────────── */
#pragma pack(push, 1)
typedef struct {
    uint32_t size;      /* usable bytes */
    uint8_t  status;    /* FREE or USED */
    uint16_t task_id;
    uint8_t  checksum;  /* XOR of bytes 0..6 */
    uint32_t handle_id; /* Added for Level 4 redirection */
} BlockHeader;
#pragma pack(pop)

#define HEADER_SIZE sizeof(BlockHeader)

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

static void _write_header(int offset, uint32_t size, uint8_t status, uint16_t task_id, uint32_t handle_id) {
    BlockHeader *h = (BlockHeader *)(ram + offset);
    h->size     = size;
    h->status   = status;
    h->task_id  = task_id;
    h->checksum = _compute_checksum(size, status, task_id);
    h->handle_id = handle_id;
}

static BlockHeader _read_header(int offset, bool *checksum_ok) {
    BlockHeader h;
    memcpy(&h, ram + offset, HEADER_SIZE);
    uint8_t expected = _compute_checksum(h.size, h.status, h.task_id);
    if (checksum_ok) *checksum_ok = (h.checksum == expected);
    return h;
}
/* LEVEL 4 EARLY INTIIALISATION*/
#define MAX_HANDLES 4096
static int handle_table[MAX_HANDLES];  /* handle -> offset; -1 = invalid */
static int next_handle = 0;

void handles_init(void) {
    for (int i = 0; i < MAX_HANDLES; i++) handle_table[i] = -1;
    next_handle = 0;
}


/* ── Level 3 ──────────────────────────────────────────────────────────────────────────────────────────────────── */
typedef struct {
    int  task_id;
    int  quota_bytes;
    int  bytes_allocated;   /* peak allocated */
    bool quota_exceeded;
} TaskReport;

TaskReport task_table[MAX_TASKS];
int current_usage[MAX_TASKS]; 
int current_running_task_id = -1;

typedef void (*TaskFn)(void);

TaskReport task_spawn(int task_id, int quota_bytes, TaskFn fn) {
    TaskReport r = {task_id, quota_bytes, 0, false};
    task_table[task_id] = r;
    int previous_task = current_running_task_id;
    current_running_task_id = task_id;
    if (fn!=NULL) fn();
    current_running_task_id = previous_task;
    int offset = 0;

    while (offset + HEADER_SIZE <= DEVICE_RAM) {
        bool csum_ok;
        BlockHeader h = _read_header(offset, &csum_ok);
        if (h.status == STATUS_USED && task_id == h.task_id) {
            printf("Leak occurred\n");
            printf("[offset=%4d  size=%-6u  status=%-4s  task=%-3d  csum=%s ]\n",
               offset, h.size, (h.status == STATUS_USED ? "USED" : "FREE"), h.task_id, (csum_ok ? "OK" : "ERR"));
        }
        offset += HEADER_SIZE + h.size;
    }
    return task_table[task_id];
}

/* ── Level 2 ──────────────────────────────────────────────────────────────────────────────────────────────────── */
static void _coalesce_all() {
    int offset = 0;
    bool is_valid;
    while (offset + HEADER_SIZE <= DEVICE_RAM) {
        BlockHeader h = _read_header(offset, &is_valid);
        if (!is_valid) break;
        int next_offset = offset + HEADER_SIZE + h.size;
        if (next_offset + HEADER_SIZE <= DEVICE_RAM) {
            bool next_valid;
            BlockHeader next_h = _read_header(next_offset, &next_valid);
            if (next_valid && h.status == STATUS_FREE && next_h.status == STATUS_FREE) {
                uint32_t new_size = h.size + HEADER_SIZE + next_h.size;
                _write_header(offset, new_size, STATUS_FREE, 0, NO_HANDLE);
                continue; 
            }
        }
        offset = next_offset;
    }
}

void mem_init(void) {
    memset(ram, 0, DEVICE_RAM);
    memset(current_usage, 0, sizeof(current_usage)); 
    memset(task_table, 0, sizeof(task_table));
    _write_header(0, DEVICE_RAM - HEADER_SIZE, STATUS_FREE, 0, NO_HANDLE); 
}

int mem_alloc(int n_bytes, int task_id, int strategy, int handle_id) {
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
        while (offset + HEADER_SIZE <= DEVICE_RAM) {
            BlockHeader h = _read_header(offset, &is_valid);
            if (!is_valid) break;
            if (h.status == STATUS_FREE && h.size >= (uint32_t)n_bytes) {
                uint32_t diff = h.size - n_bytes;
                if (diff >= HEADER_SIZE + MIN_ALLOC) {
                    _write_header(offset, n_bytes, STATUS_USED, task_id, handle_id);
                    _write_header(offset + HEADER_SIZE + n_bytes, diff - HEADER_SIZE, STATUS_FREE, 0, NO_HANDLE);
                } else {
                    _write_header(offset, h.size, STATUS_USED, task_id, handle_id);
                }
                if (task_id >= 0 && task_id < MAX_TASKS) {
                    current_usage[task_id] += n_bytes;
                    if (task_table[task_id].bytes_allocated < current_usage[task_id]) 
                        task_table[task_id].bytes_allocated = current_usage[task_id];
                }
                return offset + HEADER_SIZE;
            }
            offset += HEADER_SIZE + h.size;
        }
    } else { // Best Fit
        int best_offset = -1;
        uint32_t min_size = DEVICE_RAM + 1;
        while (offset + HEADER_SIZE <= DEVICE_RAM) {
            BlockHeader h = _read_header(offset, &is_valid);
            if (!is_valid) break;
            if (h.status == STATUS_FREE && h.size >= (uint32_t)n_bytes) {
                if (h.size < min_size) {
                    min_size = h.size;
                    best_offset = offset;
                }
            }
            offset += HEADER_SIZE + h.size;
        }
        if (best_offset != -1) {
            uint32_t diff = min_size - n_bytes;
            if (diff >= HEADER_SIZE + MIN_ALLOC) {
                _write_header(best_offset, n_bytes, STATUS_USED, task_id, handle_id);
                _write_header(best_offset + HEADER_SIZE + n_bytes, diff - HEADER_SIZE, STATUS_FREE, 0, NO_HANDLE);
            } else {
                _write_header(best_offset, min_size, STATUS_USED, task_id, handle_id);
            }
            if (task_id >= 0 && task_id < MAX_TASKS) {
                current_usage[task_id] += n_bytes;
                if (task_table[task_id].bytes_allocated < current_usage[task_id]) 
                    task_table[task_id].bytes_allocated = current_usage[task_id];
            }
            return best_offset + HEADER_SIZE;
        }
    }
    return -1;
}

bool mem_free(int ptr) {
    if (ptr < (int)HEADER_SIZE || ptr >= DEVICE_RAM || (ptr % ALIGNMENT) != 0) return false;
    int offset = ptr - HEADER_SIZE;
    bool is_valid;
    BlockHeader h = _read_header(offset, &is_valid);
    if (is_valid && h.status == STATUS_USED) {
        if (h.task_id < MAX_TASKS) current_usage[h.task_id] -= h.size;
        _write_header(offset, h.size, STATUS_FREE, 0, NO_HANDLE);
        if (h.handle_id != NO_HANDLE) handle_table[h.handle_id]=-1;
        _coalesce_all();
        return true;
    }
    return false;
}

void mem_dump(void) {
    int offset = 0;
    while (offset + HEADER_SIZE <= DEVICE_RAM) {
        bool csum_ok;
        BlockHeader h = _read_header(offset, &csum_ok);
        printf("[offset=%4d  size=%-6u  status=%-4s  task=%-3d  csum=%s ]\n",
               offset, h.size, (h.status == STATUS_USED ? "USED" : "FREE"), h.task_id, (csum_ok ? "OK" : "ERR"));
        offset += HEADER_SIZE + h.size;
    }
}

typedef struct {
    int   total_free_bytes;
    int   largest_free_block;
    int   num_free_fragments;
    float fragmentation_ratio;
} MemStats;

MemStats mem_stats(void) {
    MemStats s = {0, 0, 0, 0.0f};
    int offset = 0;
    bool is_valid;
    while (offset + HEADER_SIZE <= DEVICE_RAM) {
        BlockHeader h = _read_header(offset, &is_valid);
        if (!is_valid) break;
        if (h.status == STATUS_FREE) {
            if ((uint32_t)s.largest_free_block < h.size) s.largest_free_block = h.size;
            s.total_free_bytes += h.size;
            s.num_free_fragments++;
        }
        offset += HEADER_SIZE + h.size;
    }
    if (s.total_free_bytes > 0)
        s.fragmentation_ratio = 1.0f - ((float)s.largest_free_block / (float)s.total_free_bytes);
    return s;
}

/*  ── Level 4 ──────────────────────────────────────────────────────────────────────────────────────────────────── **/

/* Returns handle (>= 0) or -1 on failure. */
int mem_alloc_handle(int n_bytes, int task_id) {
    // Find free slot in handle table
    int free_handle_id = -1;
    for (int i=0; i < 4096; i++) {
        if (handle_table[i]==-1) {
            free_handle_id = i;
            break;
        }
    }
    if (free_handle_id==-1) {
        printf("Error: Handle table full");
        return -1;
    }
    int offset = mem_alloc(n_bytes, task_id, 0, free_handle_id);
    if (offset != -1) {
        handle_table[free_handle_id]=offset;
        return free_handle_id;
    } 
    return -1;
}
bool mem_free_handle(int handle) {
    if (handle < 0 || handle >= MAX_HANDLES || handle_table[handle] == -1) return false;
    mem_free(handle_table[handle]); 
    handle_table[handle] = -1; 
    return true;
}

int task_eviction() {
    int max_occup = 0;
    int max_task_id = -1;
    for (int i=0; i < MAX_TASKS; i++) {
        if (i!=current_running_task_id && current_usage[i] > max_occup) {
            max_occup = current_usage[i];
            max_task_id = i;
        }
    
    }
    if (max_task_id==-1) {
        printf("Complete system failure");
        return -1;
    }
    int offset = 0;
    bool is_valid;

    while (offset + HEADER_SIZE <= DEVICE_RAM) {
        BlockHeader h = _read_header(offset, &is_valid);
        if (!is_valid) break;

        // If block belongs to the victim task, "delete" it manually
        if (h.status == STATUS_USED && h.task_id == max_task_id) {
            if (h.handle_id != NO_HANDLE && h.handle_id < MAX_HANDLES) {
                handle_table[h.handle_id] = -1;
            }

            _write_header(offset, h.size, STATUS_FREE, 0, NO_HANDLE);
        }
        offset += HEADER_SIZE + h.size;
    }

    _coalesce_all();

    current_usage[max_task_id] = 0;
    task_table[max_task_id].bytes_allocated = 0;
    task_table[max_task_id].quota_exceeded = false;
    printf("[OOM] Evicted Task %d to recover %d bytes.\n", max_task_id, max_occup);
    return max_task_id;

}
/* Returns current raw offset for handle, or -1. */
int deref_handle(int handle) {
    if (handle < 0 || handle >= MAX_HANDLES) return -1;
    return handle_table[handle];
}

/* Compact heap, patch handle_table. Returns bytes recovered. */
int mem_compact(void) {
    int current_off = 0;
    int target_off = 0;
    int moved_count = 0;
    bool is_valid;

    printf("[System] Compacting heap...\n");

    while (current_off < DEVICE_RAM) {
        BlockHeader h = _read_header(current_off, &is_valid);
        if (!is_valid || (current_off + HEADER_SIZE + h.size) > DEVICE_RAM) break;

        uint32_t block_total_size = HEADER_SIZE + h.size;

        if (h.status == STATUS_USED) {
            // If there's a gap, slide the block down
            if (current_off > target_off) {
                memmove(ram + target_off, ram + current_off, block_total_size);
                
                // Update the handle table with the new pointer (target + header)
                if (h.handle_id != NO_HANDLE && h.handle_id < MAX_HANDLES) {
                    handle_table[h.handle_id] = target_off + HEADER_SIZE;
                }
            }
            target_off += block_total_size;
            moved_count++;
        }
        current_off += block_total_size;
    }

    // Create one large free block at the end
    uint32_t remaining = DEVICE_RAM - target_off;
    if (remaining >= HEADER_SIZE + MIN_ALLOC) {
        _write_header(target_off, remaining - HEADER_SIZE, STATUS_FREE, 0, NO_HANDLE);
    }

    return moved_count;
}

/* Alloc with compaction + OOM eviction fallback. Returns handle or -1. */
int mem_alloc_or_compact(int n_bytes, int task_id) {
    /* TODO */
      int free_handle_id = -1;
    for (int i=0; i < 4096; i++) {
        if (handle_table[i]==-1) {
            free_handle_id = i;
            break;
        }
    }
    if (free_handle_id==-1) {
        printf("Error: Handle table full");
        return -1;
    }
    int offset = mem_alloc(n_bytes, task_id, 0, free_handle_id);
    if (offset != -1) {
        handle_table[free_handle_id]=offset;
        return free_handle_id;
    } else {
        mem_compact();
        offset = mem_alloc(n_bytes, task_id, 0, free_handle_id);
        if (offset!=-1) {
            handle_table[free_handle_id]=offset;
            return free_handle_id;
        } else {
            if (task_eviction()==-1) {
                printf("Cannot find task");
            }
            mem_compact();
            offset = mem_alloc(n_bytes, task_id, 0, free_handle_id);
            if (offset!=-1) {
                handle_table[free_handle_id]=offset;
                return free_handle_id;
            } else {
                printf("Complete failure");
                return -1;
            }
        }
    }
    return -1;
}

/* ── Minimal sanity main ────────────────────────────────────────────────────── */
#include <time.h>

// TASK 1: The "Swiss Cheese" Maker
// Allocates many small blocks, then frees every other one to create fragmentation.
void task_fragmenter(void) {
    int handles[100];
    int count = 0;
    printf("[Task 4] Filling RAM with small blocks...\n");
    for (int i = 0; i < 50; i++) {
        int h = mem_alloc_or_compact(600, 4); 
        if (h != -1) handles[count++] = h;
    }
    printf("[Task 4] Creating fragmentation (freeing every other block)...\n");
    for (int i = 0; i < count; i += 2) {
        mem_free_handle(handles[i]);
    }
}

// TASK 2: The "Large Requester"
// Requests a block that ONLY fits if the holes from Task 4 are slid together.
void task_large_request(void) {
    printf("[Task 5] Requesting 15KB block (This should trigger Compaction)...\n");
    clock_t start = clock();
    int h = mem_alloc_or_compact(15000, 5);
    clock_t end = clock();
    
    double ms = ((double)(end - start) / CLOCKS_PER_SEC) * 1000;
    if (h != -1) {
        printf("[Task 5] SUCCESS! Allocated 15KB in %.2f ms\n", ms);
    } else {
        printf("[Task 5] FAILURE: Could not allocate even after compaction.\n");
    }
}

// TASK 3: The "OOM Victim" 
// Just sits there holding memory, waiting to be evicted.
void task_idle_victim(void) {
    mem_alloc_or_compact(10000, 6); 
    printf("[Task 6] I am idle and holding 10KB.\n");
}
int main(void) {
    printf("========================================\n");
    printf("MEMSIM LEVEL 4: COMPACTION & OOM RECOVERY\n");
    printf("========================================\n");
    
    mem_init();
    handles_init();

    // --- TEST 1: PROVE COMPACTION ---
    // 1. Create fragmentation
    task_spawn(4, 40000, task_fragmenter);
    
    // 2. Request large block (Triggers Compaction Strike 2)
    task_spawn(5, 20000, task_large_request);

  // --- TEST 2: PROVE EVICTION ---
    // 1. Create an IDLE task holding 10KB
    task_spawn(6, 15000, task_idle_victim);
    
    // 2. Set quotas for the "loose" tasks in main
    task_table[7].quota_bytes = 40000; // Allow Task 7 to take up to 40KB
    task_table[8].quota_bytes = 20000; // Allow Task 8 to take up to 20KB

    // 3. Now these will pass the Level 3 quota check
    printf("[Main] Filling remaining RAM...\n");
    mem_alloc_or_compact(30000, 7); 

    printf("[Main] Requesting memory to trigger Eviction of Task 6...\n");
    int h_final = mem_alloc_or_compact(12000, 8);
    if (h_final != -1) {
        printf("[Main] SUCCESS: Strike 3 (Eviction) worked!\n");
    }

    printf("\n--- FINAL HEAP MAP AFTER COMPACTION & EVICTION ---\n");
    mem_dump();

    return 0;
}