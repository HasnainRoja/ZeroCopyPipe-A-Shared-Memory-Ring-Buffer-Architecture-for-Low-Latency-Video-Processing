# ZeroCopyPipe

### A Shared-Memory Ring-Buffer Architecture for Low-Latency Video Processing

Zero-copy multi-process pipeline in C on Linux for high-resolution video workloads. Five cooperating processes share 1080p frames through a POSIX shared-memory ring buffer, coordinated with `eventfd` and atomic reference counts, eliminating inter-stage pixel copies. The design is evaluated against a `memcpy`-based baseline (approximately 2.0–2.4× lower average latency, zero frame drops) and under optional `mlock` / `SCHED_FIFO` configurations for tail-latency analysis.

---

## 1. Overview

Full-frame RGB video at 1080p occupies on the order of six megabytes per frame. In a multi-stage software pipeline—capture, denoising, edge extraction, overlay, and display—transferring each frame by repeated copying between processes incurs substantial memory-bandwidth cost and increases end-to-end latency.

**ZeroCopyPipe** addresses this problem at the operating-system level. Frames reside in a bounded **shared-memory ring buffer**. A producer process writes each frame once into a slot; three analysis processes operate on that same mapped memory; a display process records timing. Synchronization uses per-consumer event descriptors and atomic reference counts rather than payload copies.

A sequential **copy-based baseline** implements the same logical stages with explicit `memcpy` between buffers. Side-by-side measurement isolates the effect of data-movement policy.

### Representative results (600 frames)

| Pipeline | Average latency | Frame drops |
|----------|-----------------|-------------|
| Zero-copy (shared memory) | ~1.5–1.7 ms | 0 |
| Copy baseline (`memcpy`) | ~3.4–4.0 ms | — |

Relative improvement is approximately **2.0–2.4×** in mean latency. The 99th-percentile latency remains under a 70 ms clinical-style reference threshold. With privileged `mlock` and producer `SCHED_FIFO`, observed P99 latency improved further (approximately 3.1 ms in the best configuration).

---

## 2. Features

- Multi-process architecture using `fork` (producer, three consumers, display)
- POSIX shared-memory ring buffer (`shm_open`, `ftruncate`, `mmap`)
- Zero-copy inter-stage access to frame pixels
- Per-consumer `eventfd` notification (avoids wake-up contention)
- Lock-free style slot release via atomic reference counts
- Optional memory residency control (`mlock`) and real-time producer scheduling (`SCHED_FIFO`)
- Instrumented latency logging and automated comparison via Python
- Privacy-preserving synthetic 1080p surgical-style workload

---
