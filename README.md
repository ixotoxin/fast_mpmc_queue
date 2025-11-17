# Fast Lock-Free Multi-Producer/Multi-Consumer Queue

This is a C++ implementation of a fast multi-producer/multi-consumer queue. Two variants are provided: one with
a dynamically growing buffer and another with a fixed-size buffer (which requires no allocations). The queue is composed
of slots, and communication between producers and consumers happens through writing to and reading from these slots.
This design minimizes the number of allocations, deallocations, and memory fragmentation, while ensuring consistent
performance over time. In the case of the fixed-size buffer, allocations are eliminated entirely.

* #### [Fast Lock-Free Multi-Producer/Multi-Consumer Queue](docs/fast_mpmc_queue.md)
* #### [Fast Lock-Free and Allocation-Free Multi-Producer/Multi-Consumer Queue](docs/fastest_mpmc_queue.md)

The repository also contains classical implementations of MPSC and MPMC queues that were used for performance comparison.
