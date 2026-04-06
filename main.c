#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

// Ułamek x w domenie u1.23: przedział [0, 2)
#define X_MAX (1 << 24)
// Skok iteracji: 4096 minimalizuje czas wykonania (4096 próbek sprzętowych)
#define X_STEP 4096

int main() {
    int fd_x = open("/sys/kernel/sykt_sysfs/dsskma", O_WRONLY);
    int fd_ctrl = open("/sys/kernel/sykt_sysfs/dtskma", O_WRONLY);
    int fd_y = open("/sys/kernel/sykt_sysfs/drskma", O_RDONLY);

    if (fd_x < 0 || fd_ctrl < 0 || fd_y < 0) {
        perror("Błąd otwarcia węzłów sysfs");
        return 1;
    }

    char buf[32];
    double max_err = 0.0;
    int samples = 0;
    
    printf("x_raw,x_float,y_hw_raw,y_hw_float,y_ref,err\n");

    for (uint32_t x_raw = 0; x_raw < X_MAX; x_raw += X_STEP) {
        // Zapis argumentu X (format dziesiętny do sysfs)
        int len = snprintf(buf, sizeof(buf), "%u\n", x_raw);
        pwrite(fd_x, buf, len, 0);

        // Wyzwolenie cyklu sprzętowego (start = 1)
        pwrite(fd_ctrl, "1\n", 2, 0);

        // Odczyt wyniku Y
        memset(buf, 0, sizeof(buf));
        pread(fd_y, buf, sizeof(buf)-1, 0);
        uint32_t y_raw = strtoul(buf, NULL, 10);

        // Transformacja stałoprzecinkowa u1.23 dla X
        double x_float = (double)x_raw / (1 << 23);
        
        // Transformacja stałoprzecinkowa s2.23 dla Y (25-bitowy znak)
        int32_t y_signed = y_raw;
        if (y_signed & (1 << 24)) {
            y_signed -= (1 << 25);
        }
        double y_hw_float = (double)y_signed / (1 << 23);

        // Złoty model matematyczny (referencja CPU)
        double y_ref = sin(2.0 * x_float - M_PI / 4.0);
        
        // Kwantyfikacja błędu bezwzględnego
        double err = fabs(y_hw_float - y_ref);
        if (err > max_err) max_err = err;
        samples++;

        printf("%u,%.6f,%u,%.6f,%.6f,%.6e\n", x_raw, x_float, y_raw, y_hw_float, y_ref, err);
    }

    fprintf(stderr, "Przetworzono próbek: %d\n", samples);
    fprintf(stderr, "Maksymalny błąd bezwzględny (odchylenie sprzętowe LUT): %.6e\n", max_err);

    close(fd_x);
    close(fd_ctrl);
    close(fd_y);
    return 0;
}
