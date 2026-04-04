# Level 2: Variable-Sized Block Allocator

## Design Decisions
- **Variable-Sized Blocks**: To optimize memory usage, blocks are now variable in size, allowing for more flexible allocations.
- **Coalescing**: When a block is freed, the allocator automatically merges it with any adjacent free blocks. This is a critical feature to combat external fragmentation.
- **Alignment**: All allocations are aligned to a 4-byte boundary to ensure CPU compatibility and performance.

## Strategy: First-Fit and Best-Fit
- **Why**: 
    - **First-Fit (Strategy 0)**: Chosen for its speed. It finds the first available block that meets the size requirement, minimizing search time.
    - **Best-Fit (Strategy 1)**: Chosen for memory efficiency. It searches for the smallest free block that can accommodate the request, thereby reducing the creation of tiny, unusable free fragments.

## Benchmark Output
### mem_dump()
```text
Allocated: p1=8  p2=116
[offset=   0  size=100     status=FREE  task=0    csum=OK ]
[offset= 108  size=200     status=USED  task=2    csum=OK ]
[offset= 316  size=65212   status=FREE  task=0    csum=OK ]
HEAP SUMMARY: 3 blocks | 1 used | 2 free | 65312 free bytes
```

### mem_stats()
```text
free=65312  largest=65212  frags=2  ratio=0.002
```
