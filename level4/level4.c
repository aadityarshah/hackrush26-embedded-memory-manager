/*
 * MEMSIM — Level 4 (C)
 * =============================
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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
    uint32_t handle_id;
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
    memcpy(&h, ram + offset, HEADER_SIZE + 4); // Include handle_id if needed, but HEADER_SIZE was 8... 
    // Wait, the struct is now larger than 8 bytes with handle_id. 
    // Let's adjust HEADER_SIZE to match the packed struct size.
    uint8_t expected = _compute_checksum(h.size, h.status, h.task_id);
    if (checksum_ok) *checksum_ok = (h.checksum == expected);
    return h;
}

// Updating HEADER_SIZE to match the struct with handle_id
#undef HEADER_SIZE
#define HEADER_SIZE sizeof(BlockHeader)

/* ── Level 3 ──────────────────────────────────────────────────────────────────────────────────────────────────── */
typedef struct {
    int  task_id;
    int  quota_bytes;
    int  bytes_allocated;   /* peak allocated */
    bool quota_exceeded;
} TaskReport;

TaskReport task_table[MAX_TASKS];
int current_usage[MAX_TASKS]; 

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
                _write_header(offset, new_size, STATUS_FREE, 0, 0);
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
    _write_header(0, DEVICE_RAM - HEADER_SIZE, STATUS_FREE, 0, 0); 
}

int mem_alloc(int n_bytes, int task_id, int strategy) {
    if (task_id >= 0 && task_id < MAX_TASKS && current_usage[task_id] + n_bytes > task_table[task_id].quota_bytes) {
        task_table[task_id].quota_exceeded = true;
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
                    _write_header(offset, n_bytes, STATUS_USED, task_id, 0);
                    _write_header(offset + HEADER_SIZE + n_bytes, diff - HEADER_SIZE, STATUS_FREE, 0, 0);
                } else {
                    _write_header(offset, h.size, STATUS_USED, task_id, 0);
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
    }
    return -1;
}

bool mem_free(int ptr) {
    if (ptr < HEADER_SIZE || ptr >= DEVICE_RAM || (ptr % ALIGNMENT) != 0) return false;
    int offset = ptr - HEADER_SIZE;
    bool is_valid;
    BlockHeader h = _read_header(offset, &is_valid);
    if (is_valid && h.status == STATUS_USED) {
        if (h.task_id >= 0 && h.task_id < MAX_TASKS) current_usage[h.task_id] -= h.size;
        _write_header(offset, h.size, STATUS_FREE, 0, 0);
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

#define MAX_HANDLES 4096
static int handle_table[MAX_HANDLES];
static int next_handle = 0;

void handles_init(void) {
    for (int i = 0; i < MAX_HANDLES; i++) handle_table[i] = -1;
    next_handle = 0;
}

int mem_compact(void);

int mem_alloc_handle(int n_bytes, int task_id) {
    if (next_handle >= MAX_HANDLES) return -1;
    
    int offset = mem_alloc(n_bytes, task_id, 0);
    if (offset == -1) {
        mem_compact();
        offset = mem_alloc(n_bytes, task_id, 0);
    }
    
    if (offset != -1) {
        int handle = next_handle++;
        handle_table[handle] = offset;
        // Update header with handle_id
        bool ok;
        BlockHeader h = _read_header(offset - HEADER_SIZE, &ok);
        _write_header(offset - HEADER_SIZE, h.size, h.status, h.task_id, handle + 1);
        return handle;
    }
    return -1;
}

bool mem_free_handle(int handle) {
    if (handle < 0 || handle >= MAX_HANDLES || handle_table[handle] == -1) return false;
    bool ok = mem_free(handle_table[handle]);
    if (ok) handle_table[handle] = -1;
    return ok;
}

int deref_handle(int handle) {
    if (handle < 0 || handle >= MAX_HANDLES) return -1;
    return handle_table[handle];
}

int mem_compact(void) {
    int write_offset = 0;
    int read_offset = 0;
    int recovered = 0;
    bool ok;

    while (read_offset + HEADER_SIZE <= DEVICE_RAM) {
        BlockHeader h = _read_header(read_offset, &ok);
        if (!ok) break;

        if (h.status == STATUS_USED) {
            if (read_offset != write_offset) {
                // Move block
                memmove(ram + write_offset, ram + read_offset, HEADER_SIZE + h.size);
                // Update handle table
                if (h.handle_id > 0 && h.handle_id <= MAX_HANDLES) {
                    handle_table[h.handle_id - 1] = write_offset + HEADER_SIZE;
                }
            }
            write_offset += HEADER_SIZE + h.size;
        }
        read_offset += HEADER_SIZE + h.size;
    }

    if (write_offset < DEVICE_RAM) {
        int free_size = DEVICE_RAM - write_offset - HEADER_SIZE;
        if (free_size >= 0) {
            _write_header(write_offset, free_size, STATUS_FREE, 0, 0);
            recovered = free_size;
        }
    }
    return recovered;
}
