# Level 4: Heap Compaction & OOM Recovery

## Design Decisions
- **Handle-Based Allocation**: To allow for memory blocks to be moved during compaction, the system uses handles instead of raw offsets. A handle table maps a fixed ID to a moving memory offset.
- **Compaction**: The allocator can "slide" all USED blocks to the start of the RAM, merging all scattered FREE fragments into one large continuous block at the end.
- **Header Extension**: The block header has been extended to include a `handle_id` to allow for reverse-mapping during compaction.

## Strategy: Move-and-Patch
- **Why**: External fragmentation is the "final boss" of memory management. Compaction solves this by physically moving data. Using a redirection table (Handle Table) is the standard industry approach to make this movement transparent to the user.

## Benchmark Output
Run the benchmark using:
```bash
gcc -o level4_bench level4/level4_benchmark.c
./level4_bench
```

Paste your results here:
```text
(Paste your output after running the benchmark)
```
