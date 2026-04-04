# IIT Gandhinagar HackRush'26 — Embedded Systems Memory Manager

This repository contains a multi-level implementation of a custom memory manager for a simulated 64KB RAM, built from scratch in C99.

## Project Overview
The project evolves from a simple fixed-size block allocator to a sophisticated multi-tasking sandbox, with an experimental look into heap compaction.

### Implemented Levels
- **Level 1: Fixed-Size Block Allocator** (Complete)
  - Predictable 64-byte block system with XOR checksums.
- **Level 2: Variable-Sized Block Allocator** (Complete)
  - Dynamic allocation with First-Fit/Best-Fit and block coalescing.
- **Level 3: Multi-Task Sandbox** (Complete)
  - Resource isolation, quotas, and automated leak detection.
- **Level 4: Heap Compaction & OOM Recovery** (Partial Implementation)
  - Implementation of a handle-based redirection table.
  - Experimental "Move-and-Patch" compaction logic to solve external fragmentation.

## Repository Structure
```text
hackrush26-embedded-memory-manager/
├── README.md           # This file
├── starter_harness.c   # Original scaffold
├── level1/             # Level 1 implementation & docs
├── level2/             # Level 2 implementation & docs
├── level3/             # Level 3 implementation & docs
├── level4/             # Level 4 implementation & benchmark
└── scripts/
    └── run_all.sh      # Comprehensive test script
```

## Build & Run Instructions
To build and run all completed levels:
```bash
bash scripts/run_all.sh
```

To run the Level 4 Compaction Benchmark:
```bash
gcc -o level4_bench level4/level4_benchmark.c
./level4_bench
```

## Future Work (Bonus)
- Generational tracing garbage collection for automated memory management.
