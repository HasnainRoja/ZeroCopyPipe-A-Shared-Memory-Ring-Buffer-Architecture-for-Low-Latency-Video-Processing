# ZeroCopyPipe

### A Shared-Memory Ring-Buffer Architecture for Low-Latency Video Processing

**Operating Systems · Multi-Process Systems · Performance Engineering**

ZeroCopyPipe is a Linux C project that optimizes how large video frames move between processes. Instead of copying ~6 MB 1080p frames from stage to stage, processes share one POSIX shared-memory ring buffer and coordinate with `eventfd` and atomic reference counts.

---

## 1. What is this project about?

In multi-stage video pipelines (capture → enhance → analyze → display), each stage is often a separate process. The usual approach **copies** the full frame into each stage’s private buffer. At 1080p RGB, that is millions of bytes per handoff, which wastes memory bandwidth and increases latency.

This project treats that as an **operating-systems problem**: inter-process data movement, synchronization, scheduling, and measurement—not as a computer-vision product.

**Domain motivation:** surgical / endoscopic-style video, where delay and jitter matter.  
**Technical focus:** processes, shared memory, IPC, and evidence-based optimization.

---

## 2. What does it do?

The system runs **five processes**:

| Process | Role |
|---------|------|
| **Producer** | Writes each frame once into a free ring slot |
| **Denoiser** | Processes the shared frame in place |
| **Edge detector** | Processes the same slot in place |
| **Overlay** | Annotates the same slot in place |
| **Display** | Measures end-to-end latency and writes CSV logs |

A parent process creates shared memory and event descriptors, forks the children, waits for completion, then runs a **copy-based baseline** for comparison. A Python analyzer summarizes latency statistics.

**Data path (zero-copy):**

1. Producer writes pixels into shared memory (one write).  
2. Three consumers are notified via per-consumer `eventfd`.  
3. Each consumer works on the same slot and decrements an atomic `ref_count`.  
4. When `ref_count` reaches 0, the slot is freed.  
5. Display logs timestamps and latency.

---
## 3. Demo video Link for Project Demonstration
- [Play in browser](https://drive.google.com/file/d/1GaW89cjR-qgZesWpN3hK30PWXVDGE7g7/preview)
- [Open in Drive](https://drive.google.com/file/d/1GaW89cjR-qgZesWpN3hK30PWXVDGE7g7/view?usp=sharing)
---

## 4. Features

- Multi-process architecture using `fork` / `waitpid`
- POSIX shared-memory ring buffer (`shm_open`, `mmap`, `ftruncate()`)
- Zero-copy inter-stage pixel access
- Per-consumer `eventfd` signaling (no wake-up stealing)
- Atomic reference counting for safe slot reuse
- Instrumented latency logging (mean, P95, P99, max)
- Copy-based `memcpy` baseline for fair comparison
- Optional `mlock` and producer `SCHED_FIFO` via environment flags
- Synthetic 1080p surgical-style frames (privacy-safe workload)

---

## 5. What was optimized?

| Optimization target | Approach | Outcome |
|---------------------|----------|---------|
| **Frame copies between processes** | Shared-memory ring; single write per frame | ~2.0–2.4× lower average latency vs memcpy baseline |
| **Coordination cost** | `eventfd` + atomic `ref_count` | Stable multi-consumer sharing, **0 drops** over 600 frames |
| **Timing stability** | Absolute `clock_nanosleep` | Steady producer cadence |
| **Tail latency (predictability)** | Optional `mlock` + `SCHED_FIFO` | Best P99 ≈ **3.13 ms** when both enabled with privileges |

**In one line:** Initially, frame drops caused by wake-up distribution and producer-consumer imbalance were resolved through per-consumer `eventfd` synchronization, processing optimization, and improved process startup, achieving 600 frames with 0 drops. Large frames are **shared**, not recopied; optional OS policies improve **latency predictability**.

---

## 6. Experimental results

All runs: **600 frames**, **1920×1080 RGB**, **0 drops** on the zero-copy path.

### 6.1 Configuration matrix

| Run | Command | mlock | SCHED_FIFO |
|-----|---------|-------|------------|
| 1 | `PIPE_MLOCK=0 PIPE_RT=0 ./pipeline` | OFF | OFF |
| 2 | `PIPE_MLOCK=1 PIPE_RT=0 ./pipeline` | ON | OFF |
| 3 | `sudo env PIPE_MLOCK=0 PIPE_RT=1 ./pipeline` | OFF | ON |
| 4 | `sudo env PIPE_MLOCK=1 PIPE_RT=1 ./pipeline` | ON | ON |

### 6.2 Zero-copy latency by configuration

| Configuration | Avg (ms) | P95 (ms) | P99 (ms) | Max (ms) | Drops |
|---------------|----------|----------|----------|----------|-------|
| mlock OFF, RT OFF | 1.741 | 2.573 | 7.169 | 43.935 | 0 |
| mlock requested*, RT OFF | 1.703 | 2.009 | 7.676 | 59.414 | 0 |
| mlock OFF, RT ON | 1.536 | 1.834 | 6.044 | 30.763 | 0 |
| mlock ON, RT ON | 1.476 | 2.053 | 3.127 | 26.652 | 0 |

But these results may vary for every executions. This result table is one of the executions for all 4 OS configurations.
Tail latency varies on laptop/WSL.

### 6.3 Zero-copy vs copy baseline (representative)

| Metric | Zero-copy | Baseline (`memcpy`) | Gain |
|--------|-----------|---------------------|------|
| Average latency | ~1.5–1.7 ms | ~3.4–4.0 ms | **~2.0–2.5×** |
| Frame drops | 0 | — | Stable completion |
| P99 (best OS config) | **3.13 ms** | higher | Stronger predictability |
| Clinical-style gate (P99 < 70 ms) | **PASS** | — | — |

**Takeaway:** Shared memory delivers the primary speedup. Real-time scheduling and successful memory locking mainly improve **tail latency**.

---

## 7. Skills developed

| Area | Skills demonstrated |
|------|---------------------|
| **Systems programming** | C on Linux, POSIX APIs, robust process lifecycle |
| **Process management** | `fork`, inheritance of mappings/FDs, `waitpid` |
| **Shared memory & IPC** | `shm_open` / `mmap`, `eventfd`, zero-copy design |
| **Concurrency** | Atomic coordination, multi-consumer ring buffer |
| **Performance engineering** | Baseline design, latency instrumentation, P99 analysis |
| **OS policy controls** | `mlock`, `SCHED_FIFO`, privilege/limit behavior |


---

## 8. How this connects to future interests

### Medical computational systems
Surgical and interventional systems are sensitive to **latency and jitter**. This project is direct practice in:

- moving large sensor/video buffers without needless copies  
- structuring producer–consumer pipelines across processes  
- measuring average vs tail latency under OS policies  

Those ideas transfer to real-time imaging paths, monitoring pipelines, and low-latency display chains (always subject to clinical validation beyond coursework).

### Space systems
Spacecraft and ground software often stream high-rate sensor or camera data through staged processing under tight CPU, memory, and timing budgets. The same principles apply:

- shared buffers instead of copy-heavy pipelines  
- clear process isolation and failure boundaries  
- scheduling and memory residency as first-class design choices  
- evidence via baselines and tail-latency metrics  

 “I design and measure **OS-level data paths** for real-time, high-bandwidth workloads in domains I care about.”

---

