#ifndef PIPELINE_H
#define PIPELINE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sched.h>
#include <errno.h>
#include <stdint.h>
#include <stdatomic.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/eventfd.h>
#include <semaphore.h>

#define FRAME_WIDTH       1920
#define FRAME_HEIGHT      1080
#define BYTES_PER_PIXEL   3
#define FRAME_SIZE        (FRAME_WIDTH * FRAME_HEIGHT * BYTES_PER_PIXEL)

#define NUM_SLOTS         16
#define NUM_CONSUMERS     3

#define TARGET_FPS        60
#define FRAME_PERIOD_NS   (1000000000L / TARGET_FPS)
#define BENCHMARK_FRAMES  600

#define SHM_NAME          "/surgical_pipeline_shm"
#define SEM_SLOTS_FREE    "/surgical_sem_free"
#define SEM_SLOTS_READY   "/surgical_sem_ready"

#define ROLE_PRODUCER     0
#define ROLE_DENOISER     1
#define ROLE_EDGE         2
#define ROLE_OVERLAY      3
#define ROLE_DISPLAY      4

#define SLOT_FREE         0
#define SLOT_WRITING      1
#define SLOT_READY        2

typedef struct {
    uint64_t   frame_id;
    uint64_t   write_ns;
    uint64_t   denoiser_done_ns;
    uint64_t   edge_done_ns;
    uint64_t   overlay_done_ns;
    uint64_t   display_done_ns;
    atomic_int slot_state;   
    atomic_int ref_count;
} FrameMetadata;

typedef struct {
    FrameMetadata meta;
    uint8_t       pixels[FRAME_SIZE];
} RingSlot;

typedef struct {
    atomic_int  head;
    atomic_int  tail;
    atomic_int  running;
    uint64_t    total_frames;
    RingSlot    slots[NUM_SLOTS];
} SharedMemory;

#define SHM_SIZE sizeof(SharedMemory)
#define NEXT_SLOT(x) (((x) + 1) % NUM_SLOTS)

static inline uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

#define DIE(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)

#define COLOR_RESET  "\033[0m"
#define COLOR_RED    "\033[31m"
#define COLOR_GREEN  "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_BLUE   "\033[34m"
#define COLOR_CYAN   "\033[36m"
#define COLOR_BOLD   "\033[1m"

#endif
