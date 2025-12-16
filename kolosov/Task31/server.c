#include <ctype.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#define SOCKET_PATH "./socket"
#define BUF_SIZE 1024
#define MAX_CLIENTS 32

static volatile sig_atomic_t g_stop = 0;

static void on_sigint(int sig) {
    (void)sig;
    g_stop = 1;
}

static void now_hms(char out[16]) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    if (!tm) {
        strcpy(out, "??:??:??");
        return;
    }
    snprintf(out, 16, "%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
}

static void die(const char *msg) {
    perror(msg);
    exit(1);
}

int main(void) {
    int server_fd;
    struct sockaddr_un addr;

    struct pollfd fds[MAX_CLIENTS + 1];
    int client_id[MAX_CLIENTS + 1];
    time_t client_t0[MAX_CLIENTS + 1];
    int nfds = 1;

    int next_id = 1;

    signal(SIGINT, on_sigint);
    signal(SIGTERM, on_sigint);

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) die("socket");

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    unlink(SOCKET_PATH);
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) die("bind");
    if (listen(server_fd, 16) < 0) die("listen");

    memset(fds, 0, sizeof(fds));
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;

    char tbuf[16];
    now_hms(tbuf);
    printf("[%s] server: started, socket=%s\n", tbuf, SOCKET_PATH);
    fflush(stdout);

    while (!g_stop) {
        int r = poll(fds, nfds, 200);
        if (r < 0) {
            if (errno == EINTR) continue;
            die("poll");
        }
        if (r == 0) continue;

        if (fds[0].revents & POLLIN) {
            int cfd = accept(server_fd, NULL, NULL);
            if (cfd >= 0) {
                if (nfds <= MAX_CLIENTS) {
                    fds[nfds].fd = cfd;
                    fds[nfds].events = POLLIN;
                    client_id[nfds] = next_id++;
                    client_t0[nfds] = time(NULL);

                    now_hms(tbuf);
                    printf("[%s] + connect: id=%d fd=%d (clients=%d)\n",
                           tbuf, client_id[nfds], cfd, nfds);
                    fflush(stdout);

                    nfds++;
                } else {
                    now_hms(tbuf);
                    printf("[%s] ! reject: too many clients\n", tbuf);
                    fflush(stdout);
                    close(cfd);
                }
            }
        }
        for (int i = 1; i < nfds; i++) {
            if (!(fds[i].revents & POLLIN)) continue;

            char buf[BUF_SIZE];
            ssize_t n = read(fds[i].fd, buf, sizeof(buf));
            if (n > 0) {
                for (ssize_t k = 0; k < n; k++) {
                    buf[k] = (char)toupper((unsigned char)buf[k]);
                }

                now_hms(tbuf);
                printf("[%s] message: id=%d == %.*s",
                       tbuf, client_id[i], (int)n, buf);
                if (buf[n - 1] != '\n') printf("\n");
                fflush(stdout);
            } else {
                now_hms(tbuf);
                int secs = (int)difftime(time(NULL), client_t0[i]);
                printf("[%s] - disconnect: id=%d (alive=%ds)\n",
                       tbuf, client_id[i], secs);
                fflush(stdout);

                close(fds[i].fd);

                nfds--;
                if (i != nfds) {
                    fds[i] = fds[nfds];
                    client_id[i] = client_id[nfds];
                    client_t0[i] = client_t0[nfds];
                    i--;
                }
            }
        }
    }

    now_hms(tbuf);
    printf("[%s] stop\n", tbuf);
    fflush(stdout);

    for (int i = 1; i < nfds; i++) close(fds[i].fd);
    close(server_fd);
    unlink(SOCKET_PATH);
    return 0;
}
