Memory allocators in this directory

This folder provides two dynamic memory allocator implementations for ToRTOS:

1) mem0.c
   - Simple free list allocator.
   - Frees blocks but does not coalesce adjacent free blocks.
   - Low overhead and easy to understand, but can fragment over time.

2) Tomem1/mem1.c
   - Byte pool allocator with a circular, address-ordered block list.
   - Uses a roving search pointer to spread allocations.
   - Coalesces adjacent free blocks lazily during allocation.
   - Uses an end-of-pool sentinel block to bound the ring.

Select the allocator based on your footprint, fragmentation tolerance, and performance needs.
