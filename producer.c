#include "../include/pipeline.h"

static void generate_frame(uint8_t *pixels, uint64_t frame_id) {
    int cx = FRAME_WIDTH / 2;
    int cy = FRAME_HEIGHT / 2;
    int radius = 300;
    int instr_x = (int)(frame_id * 3) % FRAME_WIDTH;

    memset(pixels, 10, FRAME_SIZE);

    for (int y = 0; y < FRAME_HEIGHT; y += 2) {
        for (int x = 0; x < FRAME_WIDTH; x += 2) {
            int idx = (y * FRAME_WIDTH + x) * BYTES_PER_PIXEL;
            int dx = x - cx, dy = y - cy;

            if (dx * dx + dy * dy < radius * radius) {
                pixels[idx]     = (uint8_t)(170 + (frame_id % 40));
                pixels[idx + 1] = (uint8_t)(80  + (x % 35));
                pixels[idx + 2] = (uint8_t)(70  + (y % 25));
            }
            if (abs(x - instr_x) < 8) {
                pixels[idx]     = 230;
                pixels[idx + 1] = 230;
                pixels[idx + 2] = 210;
            }
        }
    }
}

void run_producer(SharedMemory *shm, int efd[NUM_CONSUMERS], int enable_rt) {
    printf(COLOR_BOLD COLOR_BLUE
           "[PRODUCER] PID=%d  %dfps  %d frames\n" COLOR_RESET,
           getpid(), TARGET_FPS, BENCHMARK_FRAMES);

    if (enable_rt) {
        struct sched_param sp = { .sched_priority = 50 };
        if (sched_setscheduler(0, SCHED_FIFO, &sp) == -1) {
            fprintf(stderr, COLOR_YELLOW
                    "[PRODUCER] SCHED_FIFO FAILED: %s "
                    "(continuing with normal scheduling)\n" COLOR_RESET,
                    strerror(errno));
        } else {
            printf(COLOR_GREEN
                   "[PRODUCER] SCHED_FIFO ON (priority 50)\n" COLOR_RESET);
        }
    } else {
        printf(COLOR_YELLOW
               "[PRODUCER] SCHED_FIFO OFF (normal scheduling)\n" COLOR_RESET);
    }

    struct timespec next_frame;
    clock_gettime(CLOCK_MONOTONIC, &next_frame);

    uint64_t frame_id = 0;
    int dropped = 0;

    while (frame_id < (uint64_t)BENCHMARK_FRAMES &&
           atomic_load(&shm->running)) {

        next_frame.tv_nsec += FRAME_PERIOD_NS;
        if (next_frame.tv_nsec >= 1000000000L) {
            next_frame.tv_sec++;
            next_frame.tv_nsec -= 1000000000L;
        }

        int slot = atomic_load(&shm->head);
        RingSlot *s = &shm->slots[slot];

        if (atomic_load(&s->meta.ref_count) != 0) {
            dropped++;
            if (dropped <= 5 || dropped % 30 == 1) {
                printf(COLOR_RED
                       "[PRODUCER] drop frame %lu (slot %d busy) total=%d\n"
                       COLOR_RESET, (unsigned long)frame_id, slot, dropped);
            }
        } else {
            atomic_store(&s->meta.slot_state, SLOT_WRITING);
            s->meta.frame_id = frame_id;
            s->meta.write_ns = now_ns();
            s->meta.denoiser_done_ns = 0;
            s->meta.edge_done_ns     = 0;
            s->meta.overlay_done_ns  = 0;
            s->meta.display_done_ns  = 0;

            generate_frame(s->pixels, frame_id);

            atomic_store_explicit(&s->meta.ref_count, NUM_CONSUMERS,
                                  memory_order_release);
            atomic_store(&s->meta.slot_state, SLOT_READY);

            atomic_store(&shm->head, NEXT_SLOT(slot));
            shm->total_frames = frame_id + 1;

            uint64_t one = 1;
            for (int i = 0; i < NUM_CONSUMERS; i++) {
                ssize_t w = write(efd[i], &one, sizeof(one));
                (void)w;
            }

            if (frame_id % 30 == 0) {
                printf(COLOR_BLUE "[PRODUCER] frame %lu → slot %d\n"
                       COLOR_RESET, (unsigned long)frame_id, slot);
            }
            frame_id++;
        }

        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_frame, NULL);
    }

    atomic_store(&shm->running, 0);

    uint64_t one = 1;
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        ssize_t w = write(efd[i], &one, sizeof(one));
        (void)w;
    }

    printf(COLOR_BOLD COLOR_BLUE
           "[PRODUCER] Done. produced=%lu dropped=%d\n" COLOR_RESET,
           (unsigned long)frame_id, dropped);
}