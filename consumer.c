#include "../include/pipeline.h"

static void apply_denoiser(uint8_t *pixels, uint64_t frame_id) {
    (void)frame_id;
    for (int y = 1; y < FRAME_HEIGHT - 1; y += 8) {
        for (int x = 1; x < FRAME_WIDTH - 1; x += 8) {
            int idx   = (y * FRAME_WIDTH + x) * 3;
            int above = ((y - 1) * FRAME_WIDTH + x) * 3;
            int below = ((y + 1) * FRAME_WIDTH + x) * 3;
            int left  = (y * FRAME_WIDTH + (x - 1)) * 3;
            int right = (y * FRAME_WIDTH + (x + 1)) * 3;
            for (int c = 0; c < 3; c++) {
                pixels[idx + c] = (uint8_t)(
                    (pixels[idx + c] + pixels[above + c] +
                     pixels[below + c] + pixels[left + c] +
                     pixels[right + c]) / 5);
            }
        }
    }
}

static void apply_edge_detector(uint8_t *pixels, uint64_t frame_id) {
    (void)frame_id;
    for (int y = 1; y < FRAME_HEIGHT - 1; y += 8) {
        for (int x = 1; x < FRAME_WIDTH - 1; x += 8) {
            int idx   = (y * FRAME_WIDTH + x) * 3;
            int right = (y * FRAME_WIDTH + (x + 1)) * 3;
            int below = ((y + 1) * FRAME_WIDTH + x) * 3;
            int mag = abs((int)pixels[right] - (int)pixels[idx]) +
                      abs((int)pixels[below] - (int)pixels[idx]);
            if (mag > 30) {
                pixels[idx]     = (uint8_t)(pixels[idx] + 50 > 255 ? 255 : pixels[idx] + 50);
                pixels[idx + 1] = (uint8_t)(pixels[idx + 1] + 80 > 255 ? 255 : pixels[idx + 1] + 80);
                pixels[idx + 2] = (uint8_t)(pixels[idx + 2] + 80 > 255 ? 255 : pixels[idx + 2] + 80);
            }
        }
    }
}

static void apply_overlay_renderer(uint8_t *pixels, uint64_t frame_id) {
    int cx = FRAME_WIDTH / 2, cy = FRAME_HEIGHT / 2;
    int inner_r = 220, outer_r = 235;

    for (int y = cy - outer_r; y <= cy + outer_r; y += 2) {
        if (y < 0 || y >= FRAME_HEIGHT) continue;
        for (int x = cx - outer_r; x <= cx + outer_r; x += 2) {
            if (x < 0 || x >= FRAME_WIDTH) continue;
            int dx = x - cx, dy = y - cy, d2 = dx * dx + dy * dy;
            if (d2 >= inner_r * inner_r && d2 <= outer_r * outer_r) {
                int idx = (y * FRAME_WIDTH + x) * 3;
                pixels[idx] = 255;
                pixels[idx + 1] = 50;
                pixels[idx + 2] = 50;
            }
        }
    }

    int ix = (int)(frame_id * 3) % FRAME_WIDTH;
    for (int y = cy - 20; y <= cy + 20; y++) {
        if (y < 0 || y >= FRAME_HEIGHT) continue;
        int idx = (y * FRAME_WIDTH + ix) * 3;
        pixels[idx] = 255;
        pixels[idx + 1] = 255;
        pixels[idx + 2] = 0;
    }
}

void run_consumer(SharedMemory *shm, int efd, int role) {
    const char *names[] = {
        "Producer", "Denoiser", "EdgeDetect", "Overlay", "Display"
    };
    const char *colors[] = {
        COLOR_BLUE, COLOR_GREEN, COLOR_YELLOW, COLOR_CYAN, COLOR_RESET
    };

    printf("%s[%s] PID=%d\n" COLOR_RESET, colors[role], names[role], getpid());

    uint64_t processed_ids[NUM_SLOTS];
    for (int i = 0; i < NUM_SLOTS; i++)
        processed_ids[i] = UINT64_MAX;

    uint64_t frames_processed = 0;

    while (frames_processed < (uint64_t)BENCHMARK_FRAMES) {
        uint64_t val = 0;
        ssize_t r = read(efd, &val, sizeof(val)); /* blocking */
        if (r == -1) {
            if (errno == EINTR) continue;
            perror("[CONSUMER] eventfd read");
            break;
        }

        int progress = 1;
        while (progress && frames_processed < (uint64_t)BENCHMARK_FRAMES) {
            progress = 0;

            for (int i = 0; i < NUM_SLOTS; i++) {
                RingSlot *s = &shm->slots[i];

                int state = atomic_load_explicit(&s->meta.slot_state,
                                                 memory_order_acquire);
                uint64_t fid = s->meta.frame_id;
                int rc = atomic_load_explicit(&s->meta.ref_count,
                                              memory_order_acquire);

                if (state != SLOT_READY || rc <= 0 || fid == processed_ids[i])
                    continue;

                processed_ids[i] = fid;

                switch (role) {
                    case ROLE_DENOISER:
                        apply_denoiser(s->pixels, fid);
                        s->meta.denoiser_done_ns = now_ns();
                        break;
                    case ROLE_EDGE:
                        apply_edge_detector(s->pixels, fid);
                        s->meta.edge_done_ns = now_ns();
                        break;
                    case ROLE_OVERLAY:
                        apply_overlay_renderer(s->pixels, fid);
                        s->meta.overlay_done_ns = now_ns();
                        break;
                }

                frames_processed++;

                int left = atomic_fetch_sub_explicit(
                               &s->meta.ref_count, 1, memory_order_release) - 1;
                if (left == 0) {
                    atomic_store_explicit(&s->meta.slot_state, SLOT_FREE,
                                          memory_order_release);
                }

                progress = 1;

                if (frames_processed % 30 == 0) {
                    printf("%s[%s] processed %lu\n" COLOR_RESET,
                           colors[role], names[role],
                           (unsigned long)frames_processed);
                }
                break;
            }
        }

        if (!atomic_load(&shm->running) &&
            frames_processed >= shm->total_frames)
            break;
    }

    printf(COLOR_BOLD "%s[%s] Done. total=%lu\n" COLOR_RESET,
           colors[role], names[role], (unsigned long)frames_processed);
}