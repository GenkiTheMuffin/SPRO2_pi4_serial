/* serial_listener.c
 * Runs as a daemon on the Pi 4.
 * Listens for responses from the Pico 2 over USB serial and
 * triggers the corresponding Klipper macro via the Klippy Unix socket.
 *
 * Build:   gcc -O2 -Wall -o serial_listener serial_listener.c
 * Install: sudo cp serial_listener /usr/local/bin/
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>

/* ── Configuration ─────────────────────────────────────────────────────── */
#define SERIAL_PORT     "/dev/serial/by-id/usb-Raspberry_Pi_Pico_026AB59ED014A442-if00"
#define BAUD_RATE       B115200
#define KLIPPER_SOCKET  "/home/mainsail/printer_data/comms/klippy.sock"
#define BUF_SIZE        256

/* ── Command map — Pico response → Klipper macro ───────────────────────── */
typedef struct {
    const char *serial_response;
    const char *klipper_macro;
} CmdMap;

static const CmdMap CMD_MAP[] = {
    { "JAW_OPENED",  "JAW_FULLY_OPEN"   },
    { "JAW_CLOSED",  "JAW_FULLY_CLOSED" },
    { "STATUS_DONE", "STATUS_DONE"      },
    { NULL, NULL }   /* sentinel */
};

/* ── Globals ────────────────────────────────────────────────────────────── */
static volatile int running = 1;

static void handle_sigint(int sig) {
    (void)sig;
    running = 0;
}

/* ── Open + configure serial port ──────────────────────────────────────── */
static int open_serial(const char *port, speed_t baud) {
    int fd = open(port, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        fprintf(stderr, "[listener] cannot open %s: %s\n",
                port, strerror(errno));
        return -1;
    }

    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        fprintf(stderr, "[listener] tcgetattr: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    cfsetispeed(&tty, baud);
    cfsetospeed(&tty, baud);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;  /* 8-bit chars        */
    tty.c_cflag |= (CLOCAL | CREAD);              /* ignore modem ctrl  */
    tty.c_cflag &= ~(PARENB | PARODD);            /* no parity          */
    tty.c_cflag &= ~CSTOPB;                       /* 1 stop bit         */
    tty.c_cflag &= ~CRTSCTS;                      /* no HW flow control */

    tty.c_iflag &= ~(IXON | IXOFF | IXANY);      /* no SW flow control */
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK |
                     ISTRIP | INLCR | IGNCR | ICRNL);

    tty.c_lflag = 0;   /* raw mode — no echo, no signals */
    tty.c_oflag = 0;   /* raw output                     */

    tty.c_cc[VMIN]  = 0;   /* non-blocking read  */
    tty.c_cc[VTIME] = 10;  /* 1 s read timeout   */

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        fprintf(stderr, "[listener] tcsetattr: %s\n", strerror(errno));
        close(fd);
        return -1;
    }
    return fd;
}

/* ── Send a G-code command to Klipper via its Unix socket ───────────────── */
static int send_to_klipper(const char *gcode) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("[listener] socket");
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, KLIPPER_SOCKET, sizeof(addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[listener] connect");
        close(sock);
        return -1;
    }

    /* Klipper JSON-RPC + ETX terminator */
    char request[BUF_SIZE * 2];
    snprintf(request, sizeof(request),
             "{\"id\":1,\"method\":\"gcode/script\","
             "\"params\":{\"script\":\"%s\"}}\x03",
             gcode);

    ssize_t n = write(sock, request, strlen(request));
    if (n < 0) perror("[listener] write");

    usleep(100000);  /* 100 ms — give Klipper time to process */
    close(sock);
    return (n < 0) ? -1 : 0;
}

/* ── Look up a Pico response string in the command map ─────────────────── */
static const char *lookup_macro(const char *token) {
    for (int i = 0; CMD_MAP[i].serial_response != NULL; i++) {
        if (strcmp(token, CMD_MAP[i].serial_response) == 0)
            return CMD_MAP[i].klipper_macro;
    }
    return NULL;
}

/* ── Read a newline-terminated line from the serial fd ─────────────────── */
static int read_line(int fd, char *buf, size_t maxlen) {
    size_t pos = 0;
    while (pos < maxlen - 1 && running) {
        char c;
        ssize_t n = read(fd, &c, 1);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return 0;    /* timeout, no data */
        if (c == '\n' || c == '\r') {
            if (pos > 0) break;  /* complete line */
            continue;            /* skip leading CR/LF */
        }
        buf[pos++] = c;
    }
    buf[pos] = '\0';
    return (int)pos;
}

/* ── Main ───────────────────────────────────────────────────────────────── */
int main(void) {
    signal(SIGINT,  handle_sigint);
    signal(SIGTERM, handle_sigint);

    printf("[listener] Opening %s @ 115200 baud\n", SERIAL_PORT);
    int serial_fd = open_serial(SERIAL_PORT, BAUD_RATE);
    if (serial_fd < 0) return EXIT_FAILURE;

    printf("[listener] Ready — waiting for responses from Pico 2\n");

    char line[BUF_SIZE];
    while (running) {
        int n = read_line(serial_fd, line, sizeof(line));
        if (n < 0) {
            fprintf(stderr, "[listener] read error: %s\n", strerror(errno));
            break;
        }
        if (n == 0) continue;  /* timeout, loop again */

        printf("[listener] RX: \"%s\"\n", line);

        const char *macro = lookup_macro(line);
        if (macro) {
            printf("[listener] → triggering macro: %s\n", macro);
            send_to_klipper(macro);
        } else {
            printf("[listener] → unknown response, ignoring\n");
        }
    }

    close(serial_fd);
    printf("[listener] Shutting down\n");
    return EXIT_SUCCESS;
}
