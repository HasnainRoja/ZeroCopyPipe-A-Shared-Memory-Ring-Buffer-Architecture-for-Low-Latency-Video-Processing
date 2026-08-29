#include "../include/pipeline.h"

void run_producer(SharedMemory *shm, int efd[NUM_CONSUMERS], int enable_rt);
void run_consumer(SharedMemory *shm, int efd, int role);
void run_display(SharedMemory *shm);
void run_baseline(void);
SharedMemory *create_shared_memory(int enable_mlock);
void destroy_shared_memory(SharedMemory *shm);

static void cleanup_previous_run(void) {
    shm_unlink(SHM_NAME);
    sem_unlink(SEM_SLOTS_FREE);
    sem_unlink(SEM_SLOTS_READY);
}

int main(void) {
    int enable_mlock = 0;
    int enable_rt = 0;

    const char *env_mlock = getenv("PIPE_MLOCK");
    const char *env_rt    = getenv("PIPE_RT");
    if (env_mlock && env_mlock[0] == '1') enable_mlock = 1;
    if (env_rt && env_rt[0] == '1')       enable_rt = 1;

    printf(COLOR_BOLD
           "\n=== Zero-Copy Surgical Video Pipeline ===\n"
           "%dx%d | slots=%d | %dfps | frames=%d\n"
           "Experiment: mlock=%s | SCHED_FIFO=%s\n\n" COLOR_RESET,
           FRAME_WIDTH, FRAME_HEIGHT, NUM_SLOTS, TARGET_FPS, BENCHMARK_FRAMES,
           enable_mlock ? "ON" : "OFF",
           enable_rt ? "ON" : "OFF");

    cleanup_previous_run();
    SharedMemory *shm = create_shared_memory(enable_mlock);

    int efd[NUM_CONSUMERS];
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        efd[i] = eventfd(0, 0);
        if (efd[i] == -1) DIE("eventfd");
    }

    pid_t pids[5];
    const char *names[5] = {
        "Denoiser", "EdgeDetector", "Overlay", "Display", "Producer"
    };

    printf("[MAIN] Forking (consumers first)...\n");

    for (int i = 0; i < 5; i++) {
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("fork");
            for (int j = 0; j < i; j++) kill(pids[j], SIGTERM);
            destroy_shared_memory(shm);
            return 1;
        }

        if (pids[i] == 0) {
            switch (i) {
                case 0: run_consumer(shm, efd[0], ROLE_DENOISER); break;
                case 1: run_consumer(shm, efd[1], ROLE_EDGE);     break;
                case 2: run_consumer(shm, efd[2], ROLE_OVERLAY);  break;
                case 3: run_display(shm);                         break;
                case 4: run_producer(shm, efd, enable_rt);        break;
            }
            _exit(0);
        }

        printf("[MAIN] %s PID=%d\n", names[i], pids[i]);
        if (i < 3) {
            struct timespec st = { 0, 20000000L };
            nanosleep(&st, NULL);
        }
    }

    printf(COLOR_BOLD "\n[MAIN] Running...\n\n" COLOR_RESET);

    int ok = 1;
    for (int i = 0; i < 5; i++) {
        int st;
        if (waitpid(pids[i], &st, 0) == -1) {
            perror("waitpid");
            ok = 0;
            continue;
        }
        if (WIFEXITED(st) && WEXITSTATUS(st) == 0)
            printf("[MAIN] %s OK\n", names[i]);
        else {
            printf(COLOR_RED "[MAIN] %s failed\n" COLOR_RESET, names[i]);
            ok = 0;
        }
    }

    printf(COLOR_BOLD "\n[MAIN] Zero-copy %s\n" COLOR_RESET,
           ok ? COLOR_GREEN "SUCCESS" COLOR_RESET
              : COLOR_RED "ERRORS" COLOR_RESET);

    run_baseline();

    system("python3 ../scripts/analyze.py 2>/dev/null || "
           "python3 scripts/analyze.py 2>/dev/null || "
           "echo '[MAIN] analyzer skipped'");

    for (int i = 0; i < NUM_CONSUMERS; i++)
        close(efd[i]);
    destroy_shared_memory(shm);

    printf("[MAIN] Done\n");
    return ok ? 0 : 1;
}