# Level 3: Multi-Task Sandbox

## Designing Process

Kept the Level 2 allocator and added supervisor controls around it.
The focus here is isolation: each task gets limits, tracked usage, and post-run leak checks.

## Design Decisions

- Each block stores `task_id`; allocator updates per-task usage on alloc/free.
- `task_spawn()` wraps task execution and produces a `TaskReport`.
- Quota violations set `quota_exceeded=true` and reject allocation.
- Leak reporting is post-task and based on surviving USED blocks.

## Strategy Chosen and Why

Used quota-based gating before allocation, plus per-task accounting during allocation/free.
This catches abuse early and keeps one task from consuming the full heap.
Leak scanning after task execution gives a simple practical signal for memory hygiene.

## Benchmark Output

### System Test
```text
========== LEVEL 3 SYSTEM TEST ==========
REPORT Task 1: Peak=100, Exceeded=NO
Leak occurred
[offset=   0  size=200     status=USED  task=2    csum=OK ]
REPORT Task 2: Peak=200, Exceeded=NO
Cannot allocate 1000 bytes[Task 3] My allocation was blocked by the Supervisor!
REPORT Task 3: Peak=0, Exceeded=YES
```

### Final Heap Map
```text
[offset=   0  size=200     status=USED  task=2    csum=OK ]
[offset= 208  size=65320   status=FREE  task=0    csum=OK ]
HEAP SUMMARY: 2 blocks | 1 used | 1 free | 65320 free bytes
```

### `mem_stats()`
```text
free=65320  largest=65320  frags=1  ratio=0.000
```
