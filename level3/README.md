# Level 3: Multi-Task Sandbox

## Design Decisions
- **Task Quotas**: Each task is assigned a specific memory quota. The allocator checks this quota before granting any memory request, preventing a single task from exhausting system resources.
- **Leak Detection**: After a task completes, the sandbox scans the entire heap for any blocks still tagged with that task's ID. This provides an automated way to identify memory leaks.
- **Peak Usage Tracking**: The system tracks the maximum amount of memory each task has allocated at any single point in time.

## Strategy: Resource Isolation
- **Why**: In a multi-tasking embedded environment, isolating resources is crucial for stability. By tagging every block with a `task_id` and enforcing quotas, we ensure that a bug in one task (like a memory leak or an over-allocation) does not cause the entire system to crash.

## Benchmark Output
### System Test Results
```text
========== LEVEL 3 SYSTEM TEST ==========
REPORT Task 1: Peak=100, Exceeded=NO
Leak occurred
[offset=   0  size=200     status=USED  task=2    csum=OK ]
REPORT Task 2: Peak=200, Exceeded=NO
Cannot allocate 1000 bytes[Task 3] My allocation was blocked by the Supervisor!
REPORT Task 3: Peak=0, Exceeded=YES
```

### FINAL HEAP MAP
```text
[offset=   0  size=200     status=USED  task=2    csum=OK ]
[offset= 208  size=65320   status=FREE  task=0    csum=OK ]
HEAP SUMMARY: 2 blocks | 1 used | 1 free | 65320 free bytes
```
