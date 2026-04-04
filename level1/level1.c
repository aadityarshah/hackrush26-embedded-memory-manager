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

#define BLOCK_SIZE 64
#define NUM_OF_BLOCKS (DEVICE_RAM / BLOCK_SIZE)

/* ── Global RAM ─────────────────────────────────────────────────────────────── */
static uint8_t ram[DEVICE_RAM];

/* ── Header layout ──────────────────────────────────────────────────────────── */
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
    memcpy(buf, &size, 4);
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

/* ── Level 1 Implementation ─────────────────────────────────────────────────── */

void mem_init(void) {
    memset(ram, 0, DEVICE_RAM);
    for (int i = 0; i < NUM_OF_BLOCKS; i++) {
        _write_header(i * BLOCK_SIZE, BLOCK_SIZE - HEADER_SIZE, STATUS_FREE, 0);
    }
}

int mem_alloc(int n_bytes, int task_id, int strategy) {
    if (n_bytes > (BLOCK_SIZE - HEADER_SIZE)) return -1; 
    if (n_bytes < MIN_ALLOC) n_bytes = MIN_ALLOC;
    
    for (int i = 0; i < NUM_OF_BLOCKS; i++) {
        int offset = i * BLOCK_SIZE;
        bool is_valid;
        BlockHeader h = _read_header(offset, &is_valid);

        if (is_valid && h.status == STATUS_FREE) {
            _write_header(offset, BLOCK_SIZE - HEADER_SIZE, STATUS_USED, task_id);
            return offset + HEADER_SIZE;
        }
    }
    return -1; 
}

bool mem_free(int ptr) {
    if (ptr < HEADER_SIZE || ptr >= DEVICE_RAM || (ptr - HEADER_SIZE) % BLOCK_SIZE != 0) return false;

    int offset = ptr - HEADER_SIZE;
    bool is_valid;
    BlockHeader h = _read_header(offset, &is_valid);
    if (is_valid && h.status == STATUS_USED) {
        _write_header(offset, BLOCK_SIZE - HEADER_SIZE, STATUS_FREE, 0);
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
    int free_blocks = 0;
    bool is_valid;

    for (int i = 0; i < NUM_OF_BLOCKS; i++) {
        int offset = i * BLOCK_SIZE;
        BlockHeader h = _read_header(offset, &is_valid);
        if (is_valid && h.status == STATUS_FREE) {
            free_blocks++;
        }
    }

    MemStats s;
    s.total_free_bytes = free_blocks * BLOCK_SIZE;
    s.largest_free_block = (free_blocks > 0) ? (BLOCK_SIZE - HEADER_SIZE) : 0;
    s.num_free_fragments = free_blocks;
    
    if (s.total_free_bytes > 0) {
        s.fragmentation_ratio = 1.0f - ((float)s.largest_free_block / (float)s.total_free_bytes);
    } else {
        s.fragmentation_ratio = 0.0f;
    }
    return s;
}

/* ── Minimal sanity main ────────────────────────────────────────────────────── */
int main(void) {
    mem_init();
    int p1 = mem_alloc(40, 1, 0);
    int p2 = mem_alloc(50, 2, 0);
    printf("Allocated: p1=%d  p2=%d\n", p1, p2);
    mem_free(p1);
    mem_dump();
    MemStats s = mem_stats();
    printf("free=%d  largest=%d  frags=%d  ratio=%.3f\n",
           s.total_free_bytes, s.largest_free_block,
           s.num_free_fragments, s.fragmentation_ratio);
    return 0;
}