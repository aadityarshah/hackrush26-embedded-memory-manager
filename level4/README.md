# Level 4: Heap Compaction and OOM Recovery

## Designing Process

This level was developed as a resilience layer on top of Level 3.
First step was handle indirection (stable external reference), then compaction (move blocks),
then fallback recovery (evict task on hard OOM).

## Design Decisions

- `handle_table[MAX_HANDLES]`: maps handle ID to current payload address.
- `mem_alloc_handle()`: allocates a handle and backing block together.
- `deref_handle()`: resolves current payload pointer for a handle.
- `mem_compact()`: relocates USED blocks and rebuilds a single tail free region.
- `task_eviction()`: frees all blocks of the highest-usage victim task.
- `mem_alloc_or_compact()`: orchestrates compact/evict fallback policy.

## Strategy Chosen and Why

Chosen strategy is:
`direct alloc -> compact -> retry -> evict -> compact -> final retry`

Why this order:
- If direct allocation works, no relocation overhead.
- If fragmentation blocks allocation, compaction is the least destructive fix.
- If memory is genuinely exhausted, controlled eviction is used as last resort.

## Benchmark Output

Compile and run:

```bash
gcc -std=c99 -Wall -O2 -o level4_bench level4/level4_benchmark.c
./level4_bench
```

Benchmark stages:

1. Fill heap to ~90% with small handle allocations.
2. Free alternating blocks to force fragmentation.
3. Request a large block via `mem_alloc_or_compact()`.
4. Validate that surviving handles still dereference correctly.

Success criteria:

- Large post-compact allocation succeeds.
- Existing non-freed handles remain valid.

Sample output:

```text
Phase 1 — Fill to ~90%
    Allocated 776 handles, ~58976 bytes used

Phase 2 — Free every other handle (create fragmentation)
    Freed 388 handles — heap is now fragmented

Phase 3 — Request large block (8192 bytes)
    Direct alloc: FAILED (fragmented, as expected)
[System] Compacting heap...
    Post-compact alloc: SUCCESS (handle=0)
    Time taken: 0.06 ms

Phase 4 — Validate surviving handle data integrity
    All surviving handles valid: YES
Compaction benchmark: PASSED
```

### `mem_dump()` / `mem_stats()`
Level 4 benchmark is focused on compaction success and handle integrity.
`mem_dump()` and `mem_stats()` are still available in `level4.c` and can be printed
around the benchmark phases if the evaluator wants heap snapshots before/after compaction.
