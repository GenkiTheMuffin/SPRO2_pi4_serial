/* send_serial.c
 * Called by Klipper's gcode_shell_command to send a command to the Pico 2.
 * Takes the command as a command-line argument.
 *
 * Build:   gcc -O2 -Wall -o send_serial send_serial.c
 * Install: sudo cp send_serial /usr/local/bin/
 * Usage:   /usr/local/bin/send_serial OPEN_JAW
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>

/* ── Configuration ─────────────────────────────────────────────────────── */
#define SERIAL_PORT "/dev/ttyACM0"
#define BAUD_RATE    B115200
#define BUF_SIZE     256

/* ── Open + configure serial port (write-only) ──────────────────────────── */
static int open_serial(const char *port, speed_t baud) {
    int fd = open(port, O_WRONLY | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        fprintf(stderr, "[send_serial] cannot open %s: %s\n",
                port, strerror(errno));
        return -1;
    }

    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        fprintf(stderr, "[send_serial] tcgetattr: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    cfsetospeed(&tty, baud);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;  /* 8-bit chars        */
    tty.c_cflag |= (CLOCAL | CREAD);              /* ignore modem ctrl  */
    tty.c_cflag &= ~(PARENB | PARODD);            /* no parity          */
    tty.c_cflag &= ~CSTOPB;                       /* 1 stop bit         */
    tty.c_cflag &= ~CRTSCTS;                      /* no HW flow control */

    tty.c_iflag &= ~(IXON | IXOFF | IXANY);      /* no SW flow control */
    tty.c_lflag = 0;                              /* raw mode           */
    tty.c_oflag = 0;                              /* raw output         */

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        fprintf(stderr, "[send_serial] tcsetattr: %s\n", strerror(errno));
        close(fd);
        return -1;
    }
    return fd;
}

/* ── Main ───────────────────────────────────────────────────────────────── */
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <COMMAND>\n", argv[0]);
        fprintf(stderr, "  e.g. %s OPEN_JAW\n", argv[0]);
        return EXIT_FAILURE;
    }

    int fd = open_serial(SERIAL_PORT, BAUD_RATE);
    if (fd < 0) return EXIT_FAILURE;

    /* Append newline so the Pico can detect end-of-command */
    char buf[BUF_SIZE];
    snprintf(buf, sizeof(buf), "%s\n", argv[1]);

    ssize_t n = write(fd, buf, strlen(buf));
    if (n < 0) {
        fprintf(stderr, "[send_serial] write error: %s\n", strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }

    tcdrain(fd);  /* block until all bytes are physically transmitted */
    close(fd);

    printf("[send_serial] Sent: %s\n", argv[1]);
    return EXIT_SUCCESS;
}
