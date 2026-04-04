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
    memset(ram, 0, DEVICE_RAM);
    // Initialise the header (Header size is 8 bytes) which contains details of the initially free chunk 
    // The free chunk is set at offset = 0, size = 64*1024 - 8
    _write_header(0, DEVICE_RAM - HEADER_SIZE, STATUS_FREE, 0); 
}
int mem_alloc(int n_bytes, int task_id, int strategy) {
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
int main(void) {
    mem_init();
    int p1 = mem_alloc(100, 1, 0);
    int p2 = mem_alloc(200, 2, 0);
    printf("Allocated: p1=%d  p2=%d\n", p1, p2);
    mem_free(p1);
    mem_dump();
    MemStats s = mem_stats();
    printf("free=%d  largest=%d  frags=%d  ratio=%.3f\n",
           s.total_free_bytes, s.largest_free_block,
           s.num_free_fragments, s.fragmentation_ratio);
    return 0;
}
