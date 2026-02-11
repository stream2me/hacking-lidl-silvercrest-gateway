# TX Optimization Memo (RTL8196E Minimal Driver)

## Starting Point
- TX path uses RTL819x pkthdr/mbuf rings with KSEG1 pointers (no data copy).
- On submit: fill pktHdr/mbuf fields, flush cache, set OWN bit, kick TX via `TXFD`.
- TX reclaim happens in IRQ (TX-done) and via a short periodic timer to avoid stuck queues during TX-only traffic.
- Watchdog recovery resets the TX ring on timeout.

## What Was Tried
- **Instrumented TX path**: added verbose ISR/descriptor logging to confirm OWN transitions, ring indices, and kick behavior.
- **TX kick behavior**: verified `TXFD` assertion after submit; confirmed descriptors are RISC-owned before submit and SWCORE-owned afterward.
- **Reclaim + watchdog**: reinforced TX reclaim in ISR and timer; kept timeout recovery to clear stuck queues.
- **Performance experiments that didn’t help**: optional “TX performance” tweaks (e.g., alternate interrupt usage, extra debug/stat paths) did not improve throughput and were reverted to keep the baseline clean.

## Current Status
- TX is functional and stable under light traffic, but **gateway→host TCP throughput remains much lower** than host→gateway (≈20 Mbps vs 80–90 Mbps in iperf2), indicating TX-side inefficiency or pacing.

## Next Investigation Paths (TX-Focused)
1. **Batch TX kicks (MMIO reduction)**  
   Use `was_empty` to only assert `TXFD` on empty→non-empty transitions. This reduces register writes and can improve TX pacing.
2. **Interrupt vs timer balance**  
   Verify TX-done interrupts are reliably delivered and handled. If IRQs are sparse, consider a larger reclaim batch in the timer or a small NAPI-style TX poll.
3. **Descriptor + threshold tuning**  
   Increase `RTL8196E_TX_DESC` if memory allows and tune `TX_STOP/WAKE` thresholds to reduce queue stalls.
4. **Checksum/offload strategy**  
   If hardware supports L3/L4 checksum generation, expose it in TX flags to reduce CPU cost. If not, ensure the stack isn’t doing unnecessary work.
5. **Cache/memory alignment**  
   Revisit cache flush granularity and ensure pktHdr/mbuf are cacheline aligned; avoid redundant `dma_cache_wback_inv()` calls.
6. **TCP window effects (iperf2)**  
   TX from gateway shows low throughput with default window sizes. Test with larger socket buffers (`-w`, `-l`, `-P`) to separate driver vs TCP window limits.
7. **TX timeout root cause**  
   If timeouts persist under sustained load, audit for missed TX-done IRQs or a ring index drift; add a lightweight counter to detect “no progress” conditions without heavy logging.
