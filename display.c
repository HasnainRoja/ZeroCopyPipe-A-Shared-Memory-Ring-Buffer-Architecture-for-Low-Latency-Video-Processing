#include "../include/pipeline.h"

#define MAX_IDLE 8000

void run_display(SharedMemory *shm) {
    printf(COLOR_BOLD COLOR_CYAN "[DISPLAY] PID=%d\n" COLOR_RESET, getpid());

    FILE *log = fopen("../results/latency_log.csv", "w");
    if (!log) log = fopen("results/latency_log.csv", "w");
    if (!log) {
        perror("fopen latency_log.csv");
        return;
    }

    fprintf(log,
            "frame_id,write_ns,denoiser_done_ns,edge_done_ns,"
            "overlay_done_ns,display_done_ns,"
            "end_to_end_us,denoiser_us,edge_us,overlay_us\n");
    fflush(log);

    uint64_t frames_logged = 0;
    uint64_t total_ns = 0, max_ns = 0, min_ns = UINT64_MAX;
    uint64_t last_id = UINT64_MAX;
    int idle = 0;

    while (frames_logged < (uint64_t)BENCHMARK_FRAMES) {
        int found = -1;
        for (int i = 0; i < NUM_SLOTS; i++) {
            RingSlot *s = &shm->slots[i];
            if (s->meta.denoiser_done_ns > 0 &&
                s->meta.edge_done_ns > 0 &&
                s->meta.overlay_done_ns > 0 &&
                s->meta.frame_id != last_id &&
                s->meta.frame_id < (uint64_t)BENCHMARK_FRAMES &&
                atomic_load(&s->meta.ref_count) == 0) {
                found = i;
                break;
            }
        }

        if (found < 0) {
            idle++;
            if (!atomic_load(&shm->running) && idle > MAX_IDLE)
                break;
            usleep(200);
            continue;
        }

        idle = 0;
        RingSlot *s = &shm->slots[found];
        s->meta.display_done_ns = now_ns();
        last_id = s->meta.frame_id;

        uint64_t e2e = s->meta.display_done_ns - s->meta.write_ns;
        uint64_t d_ns = s->meta.denoiser_done_ns - s->meta.write_ns;
        uint64_t e_ns = s->meta.edge_done_ns - s->meta.write_ns;
        uint64_t o_ns = s->meta.overlay_done_ns - s->meta.write_ns;

        fprintf(log, "%lu,%lu,%lu,%lu,%lu,%lu,%.3f,%.3f,%.3f,%.3f\n",
                (unsigned long)s->meta.frame_id,
                (unsigned long)s->meta.write_ns,
                (unsigned long)s->meta.denoiser_done_ns,
                (unsigned long)s->meta.edge_done_ns,
                (unsigned long)s->meta.overlay_done_ns,
                (unsigned long)s->meta.display_done_ns,
                e2e / 1000.0, d_ns / 1000.0, e_ns / 1000.0, o_ns / 1000.0);

        total_ns += e2e;
        if (e2e > max_ns) max_ns = e2e;
        if (e2e < min_ns) min_ns = e2e;
        frames_logged++;

        if (frames_logged % 30 == 0) {
            printf(COLOR_CYAN "[DISPLAY] logged %lu | e2e=%.2f ms\n" COLOR_RESET,
                   (unsigned long)frames_logged, e2e / 1e6);
            fflush(log);
        }

        s->meta.denoiser_done_ns = 0;
        s->meta.edge_done_ns = 0;
        s->meta.overlay_done_ns = 0;
    }

    fclose(log);

    printf(COLOR_BOLD COLOR_CYAN
           "\n[DISPLAY] DONE frames=%lu avg=%.3f ms min=%.3f max=%.3f\n"
           COLOR_RESET,
           (unsigned long)frames_logged,
           frames_logged ? (total_ns / frames_logged) / 1e6 : 0.0,
           min_ns / 1e6, max_ns / 1e6);
}