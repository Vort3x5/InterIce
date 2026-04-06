#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <time.h>

struct quadra_batch {
    uint32_t count;
    uint32_t *x_in;
    uint32_t *y_out;
};
#define QUADRA_IOC_BATCH _IOWR('q', 1, struct quadra_batch)

#define X_MAX (1 << 24)
#define COUNT 4096
#define X_STEP (X_MAX / COUNT)

int main() {
    int fd = open("/dev/quadra", O_RDWR);
    if (fd < 0) {
        perror("Błąd warstwy jądra - brak węzła /dev/quadra");
        return 1;
    }

    uint32_t *x_buf = malloc(COUNT * sizeof(uint32_t));
    uint32_t *y_buf = malloc(COUNT * sizeof(uint32_t));

    for (int i = 0; i < COUNT; i++) {
        x_buf[i] = i * X_STEP;
    }

    struct quadra_batch batch = { .count = COUNT, .x_in = x_buf, .y_out = y_buf };
    struct timespec start, end;
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    if (ioctl(fd, QUADRA_IOC_BATCH, &batch) < 0) {
        perror("Zrzut strumieniowy przerwany");
        return 1;
    }
    clock_gettime(CLOCK_MONOTONIC, &end);

    double time_taken = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    double max_err = 0.0;

    for (int i = 0; i < COUNT; i++) {
        double x_float = (double)x_buf[i] / (1 << 23);
        int32_t y_signed = y_buf[i];
        if (y_signed & (1 << 24)) y_signed -= (1 << 25);
        
        double y_hw_float = (double)y_signed / (1 << 23);
        double y_ref = sin(2.0 * x_float - M_PI / 4.0);
        
        double err = fabs(y_hw_float - y_ref);
        if (err > max_err) max_err = err;
    }

    printf("Ewaluacja wsadowa: %d probek\n", COUNT);
    printf("Czas propagacji magistrali: %.4f s (%.0f probek/s)\n", time_taken, COUNT / time_taken);
    printf("Szczytowy blad kwantyzacji: %.6e\n", max_err);

    free(x_buf);
    free(y_buf);
    close(fd);
    return 0;
}
