# Level 1: Fixed-Size Block Allocator

## Designing Process

Started with a deterministic baseline for the 64 KB RAM model:
fixed block size first, then minimal metadata validation, then simple allocate/free path.
The goal at this stage was correctness and debuggability, not best memory utilization.

## Design Decisions

- 1024 fixed blocks (`64 KB / 64`).
- Header fields: `size`, `status`, `task_id`, `checksum`.
- `mem_alloc`: find first free valid block and mark USED.
- `mem_free`: validate pointer shape and block status, then mark FREE.

## Strategy Chosen and Why

Fixed-size allocation was chosen for predictability.
It removes external fragmentation and keeps the allocator logic straightforward.
Tradeoff accepted: internal fragmentation inside partially used 56-byte payload blocks.

## Benchmark Output

### `mem_dump()`
```text
Allocated: p1=8  p2=72
[offset=   0  size=56      status=FREE  task=0    csum=OK ]
[offset=  64  size=56      status=USED  task=2    csum=OK ]
[offset= 128  size=56      status=FREE  task=0    csum=OK ]
... (truncated) ...
HEAP SUMMARY: 1024 blocks | 1 used | 1023 free | 57288 free bytes
```

### `mem_stats()`
```text
free=65472  largest=56  frags=1023  ratio=0.999
```
