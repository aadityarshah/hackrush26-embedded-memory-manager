# HackRush'26 Embedded Memory Manager

A C99 memory-management project built for a simulated 64 KB embedded RAM.  
The repository is organized as progressive levels, where each level keeps the previous foundations and introduces one major systems concern.

## Overview

| Level | Focus | Key Additions |
|---|---|---|
| 1 | Deterministic allocation | Fixed-size blocks, metadata checksum validation |
| 2 | Space efficiency | Variable-size blocks, First-Fit/Best-Fit, block splitting/coalescing |
| 3 | Multi-task isolation | Per-task quota enforcement, peak usage tracking, leak scan |
| 4 | Recovery under pressure | Handle indirection, heap compaction, OOM eviction fallback |
| Bonus | Future scope | Generational tracing GC direction (not implemented) |

## Repository Structure

```text
.
├── level1/             # Fixed-size allocator
├── level2/             # Variable-size allocator + fragmentation control
├── level3/             # Task-aware supervisor + quotas/leak reporting
├── level4/             # Handle-based allocator + compaction/OOM recovery
├── bonus/              # Future GC scope notes
├── scripts/            # Convenience scripts
└── starter_harness.c   # Baseline scaffold from challenge
```

## Build and Run

Run all levels:

```bash
bash scripts/run_all.sh
```

Compile levels individually:

```bash
gcc -std=c99 -Wall -O2 -o l1 level1/level1.c && ./l1
gcc -std=c99 -Wall -O2 -o l2 level2/level2.c && ./l2
gcc -std=c99 -Wall -O2 -o l3 level3/level3.c && ./l3
gcc -std=c99 -Wall -O2 -o l4 level4/level4.c && ./l4
```

Run Level 4 benchmark:

```bash
gcc -std=c99 -Wall -O2 -o level4_bench level4/level4_benchmark.c && ./level4_bench
```

## Submission Deliverables Mapping

- One standalone runnable source file per level:
  `level1/level1.c`, `level2/level2.c`, `level3/level3.c`, `level4/level4.c`.
- One short README per level explaining:
  design decisions, chosen strategy and rationale, and benchmark output.
- Bonus file: `bonus/bonus.c` with future GC direction notes in `bonus/README.md`.

This repository is structured directly around that format.
