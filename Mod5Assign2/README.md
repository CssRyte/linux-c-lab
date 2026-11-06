cat > README.md <<'EOF'
# Module 5 Assignment 2 — Memory Management Simulator

## Overview
A C++17 memory management simulator demonstrating segmentation,
two-level paging, TLB caching, and physical memory management.

## Features
- Segment bounds and access permission validation
- Two-level page table address translation
- Translation Lookaside Buffer (TLB) with LRU eviction
- TLB hit, miss, and hit-rate statistics
- Physical frame allocation and deallocation
- LRU physical frame victim selection
- Page replacement with page-table updates
- TLB invalidation after page replacement
- Automatic identification of the page owning an LRU frame
- Page fault and page replacement statistics

## Build

g++ -std=c++17 -Wall -Wextra -pedantic memory_simulator.cpp -o memory_simulator

## Run

./memory_simulator

## Save Test Results

./memory_simulator > test_results.txt

## Testing
The program includes tests for:
- TLB hits, misses, and eviction
- Two-level address translation
- Segment protection
- Dynamic physical frame allocation
- Page faults
- LRU page replacement
- Cross-directory page replacement
- Automatic victim-page identification
- Failed replacement safety

Screenshots of the tests are stored in the screenshots/ directory.

## Limitations
This is an educational simulator. It does not implement a backing
store, dirty-page writeback, or fully automatic page-fault recovery.
EOF