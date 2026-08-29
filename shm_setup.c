#include "../include/pipeline.h"

SharedMemory *create_shared_memory(int enable_mlock) {
    printf(COLOR_CYAN "[SHM] Creating %s (%.2f MB)\n" COLOR_RESET,
           SHM_NAME, (double)SHM_SIZE / (1024.0 * 1024.0));

    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR | O_TRUNC, 0666);
    if (fd == -1) DIE("shm_open");

    if (ftruncate(fd, (off_t)SHM_SIZE) == -1) DIE("ftruncate");

    SharedMemory *shm = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE,
                             MAP_SHARED, fd, 0);
    if (shm == MAP_FAILED) DIE("mmap");
    close(fd);

    if (enable_mlock) {
        if (mlock(shm, SHM_SIZE) == -1) {
            fprintf(stderr, COLOR_YELLOW
                    "[SHM] mlock FAILED: %s (running without lock)\n"
                    COLOR_RESET, strerror(errno));
        } else {
            printf(COLOR_GREEN "[SHM] mlock ON (pages pinned in RAM)\n"
                   COLOR_RESET);
        }
    } else {
        printf(COLOR_YELLOW "[SHM] mlock OFF\n" COLOR_RESET);
    }

    if (madvise(shm, SHM_SIZE, MADV_SEQUENTIAL) == -1) {
        fprintf(stderr, COLOR_YELLOW "[SHM] madvise failed: %s\n" COLOR_RESET,
                strerror(errno));
    }

    atomic_init(&shm->head, 0);
    atomic_init(&shm->tail, 0);
    atomic_init(&shm->running, 1);
    shm->total_frames = 0;

    for (int i = 0; i < NUM_SLOTS; i++) {
        atomic_init(&shm->slots[i].meta.slot_state, SLOT_FREE);
        atomic_init(&shm->slots[i].meta.ref_count, 0);
        shm->slots[i].meta.frame_id = 0;
        shm->slots[i].meta.write_ns = 0;
        shm->slots[i].meta.denoiser_done_ns = 0;
        shm->slots[i].meta.edge_done_ns = 0;
        shm->slots[i].meta.overlay_done_ns = 0;
        shm->slots[i].meta.display_done_ns = 0;
    }

    printf(COLOR_GREEN "[SHM] Ready (%d slots)\n" COLOR_RESET, NUM_SLOTS);
    return shm;
}

void destroy_shared_memory(SharedMemory *shm) {
    if (munmap(shm, SHM_SIZE) == -1)
        perror("munmap");
    if (shm_unlink(SHM_NAME) == -1)
        perror("shm_unlink");
    printf(COLOR_CYAN "[SHM] Destroyed\n" COLOR_RESET);
}