# Level 2: Variable-Sized Block Allocator

## Designing Process

Moved from fixed blocks to variable-sized regions to reduce wasted space.
Kept checksum and alignment logic from Level 1, then added splitting and coalescing.
This level is mainly about fragmentation control.

## Design Decisions

- Variable-sized block headers preserved with checksum protection.
- 4-byte alignment maintained for payload addresses.
- Global coalescing pass after `mem_free()` to merge neighboring free blocks.

## Strategy Chosen and Why

- `strategy=0` First-Fit: faster search in common cases.
- `strategy=1` Best-Fit: better packing, fewer tiny unusable leftovers.

Both are supported to show the speed-vs-fragmentation tradeoff clearly.

### Which Wins in Practice?

- On this repository benchmark (`level2/benchmark.c`), **Best-Fit wins**.
- Reason: workload alternates large/small allocations and delayed frees, so placement quality matters more than search speed.
- Best-Fit left one clean free fragment at the end (`frags=1`, ratio `0.0000`), while First-Fit left two fragments (`frags=2`, ratio `0.0330`).

## Benchmark Output

### `mem_dump()`
```text
Allocated: p1=8  p2=116
[offset=   0  size=100     status=FREE  task=0    csum=OK ]
[offset= 108  size=200     status=USED  task=2    csum=OK ]
[offset= 316  size=65212   status=FREE  task=0    csum=OK ]
HEAP SUMMARY: 3 blocks | 1 used | 2 free | 65312 free bytes
```

### `mem_stats()`
```text
free=65312  largest=65212  frags=2  ratio=0.002
```

### Strategy Benchmark (`level2/benchmark.c`)
```text
Strategy: first_fit
  Alloc succeeded   : 576
  Alloc failed (OOM): 424
  Free calls        : 300
  Final free bytes  : 364
  Largest free block: 352
  Free fragments    : 2
  Fragmentation ratio: 0.0330

Strategy: best_fit
  Alloc succeeded   : 575
  Alloc failed (OOM): 425
  Free calls        : 300
  Final free bytes  : 352
  Largest free block: 352
  Free fragments    : 1
  Fragmentation ratio: 0.0000

Winner: best_fit (delta = 0.0330)
```
