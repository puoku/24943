// client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#define SOCKET_PATH "./socket"

#define COUNT_DEFAULT 100
#define INTERVAL_MS_DEFAULT 20
#define DELAY_MS_DEFAULT 0

static void die(const char *msg) {
    perror(msg);
    exit(1);
}

static void sleep_ms(int ms) {
    if (ms <= 0) return;
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

int main(int argc, char **argv) {
    // usage:
    //   ./client "text"
    //   ./client "text" count interval_ms delay_ms
    if (argc < 2) {
        fprintf(stderr, "Usage: %s \"text\" [count interval_ms delay_ms]\n", argv[0]);
        return 2;
    }

    const char *text = argv[1];

    int count = COUNT_DEFAULT;
    int interval_ms = INTERVAL_MS_DEFAULT;
    int delay_ms = DELAY_MS_DEFAULT;

    if (argc >= 5) {
        count = atoi(argv[2]);
        interval_ms = atoi(argv[3]);
        delay_ms = atoi(argv[4]);
    }

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) die("socket");

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) die("connect");

    char msg[2048];
    snprintf(msg, sizeof(msg), "%s\n", text);
    size_t len = strlen(msg);

    sleep_ms(delay_ms);

    for (int i = 0; i < count; i++) {
        if (write(fd, msg, len) < 0) die("write");
        sleep_ms(interval_ms);
    }

    close(fd);
    return 0;
}
