# Level 1: Fixed-Size Block Allocator

## Design Decisions
- **Fixed-Size Blocks**: The memory is divided into 1024 blocks of 64 bytes each. This simplifies management as each block is identical in structure and size.
- **Header Structure**: Each block starts with an 8-byte header containing the size (56 usable bytes), status (FREE/USED), task ID, and an XOR checksum.
- **Safety**: An XOR checksum is computed for every header to detect memory corruption.

## Strategy: Fixed-Size Allocation
- **Why**: Choosing a fixed-size strategy eliminates external fragmentation. It is highly predictable and fast, which is critical for many embedded real-time systems. While it may lead to internal fragmentation (wasted space within a block), the overhead is offset by the simplicity of the `mem_alloc` and `mem_free` logic.

## Benchmark Output
### mem_dump()
```text
Allocated: p1=8  p2=72
[offset=   0  size=56      status=FREE  task=0    csum=OK ]
[offset=  64  size=56      status=USED  task=2    csum=OK ]
[offset= 128  size=56      status=FREE  task=0    csum=OK ]
... (truncated) ...
HEAP SUMMARY: 1024 blocks | 1 used | 1023 free | 57288 free bytes
```

### mem_stats()
```text
free=65472  largest=56  frags=1023  ratio=0.999
```
