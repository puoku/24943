#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <ctype.h>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

#define SOCKET_PATH  "./socket"
#define BUF 1024
#define MAX_CLIENTS 5
#define CHUNK_SIZE 30

static volatile sig_atomic_t g_stop = 0;

static void on_sigint(int sig) {
    (void)sig;
    g_stop = 1;
}

static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        exit(1);
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL");
        exit(1);
    }
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

int main(void) {
    int server_fd, client_fd;
    struct sockaddr_un socket_addr;

    struct pollfd fds[MAX_CLIENTS + 1];
    int client_id[MAX_CLIENTS + 1];
    time_t connect_time[MAX_CLIENTS + 1];

    int client_count = 1;
    int next_id = 1;

    char mixed_buffer[BUF * 10];
    int mixed_pos = 0;
    int last_id = 0;

    signal(SIGINT, on_sigint);
    signal(SIGTERM, on_sigint);

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }
    set_nonblocking(server_fd);

    memset(&socket_addr, 0, sizeof(socket_addr));
    socket_addr.sun_family = AF_UNIX;
    strncpy(socket_addr.sun_path, SOCKET_PATH, sizeof(socket_addr.sun_path) - 1);

    unlink(SOCKET_PATH);

    if (bind(server_fd, (struct sockaddr*)&socket_addr, sizeof(socket_addr)) < 0) {
        perror("bind");
        return 1;
    }
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen");
        return 1;
    }

    memset(fds, 0, sizeof(fds));
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;

    {
        char t[16];
        now_hms(t);
        printf("[%s] server: started, socket=%s\n", t, SOCKET_PATH);
        fflush(stdout);
    }

    while (!g_stop) {
        int ready = poll(fds, client_count, 100);
        if (ready < 0) {
            if (errno == EINTR) continue;
            perror("poll");
            break;
        }
        if (ready == 0) continue;

        for (int i = 0; i < client_count; i++) {
            if (fds[i].revents == 0) continue;

            if (fds[i].fd == server_fd) {
                if (fds[i].revents & POLLIN) {
                    while (1) {
                        client_fd = accept(server_fd, NULL, NULL);
                        if (client_fd == -1) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                            perror("accept");
                            break;
                        }

                        if (client_count < MAX_CLIENTS + 1) {
                            set_nonblocking(client_fd);

                            fds[client_count].fd = client_fd;
                            fds[client_count].events = POLLIN;

                            client_id[client_count] = next_id++;
                            connect_time[client_count] = time(NULL);

                            char t[16];
                            now_hms(t);
                            printf("[%s] + connect: id=%d fd=%d (clients=%d)\n",
                                   t, client_id[client_count], client_fd, client_count);
                            fflush(stdout);

                            client_count++;
                        } else {
                            close(client_fd);
                        }
                    }
                }
            } else if (fds[i].revents & POLLIN) {
                char ch;
                ssize_t bytes = read(fds[i].fd, &ch, 1);

                if (bytes > 0) {
                    ch = (char)toupper((unsigned char)ch);

                    if (mixed_pos < (int)sizeof(mixed_buffer) - 1) {
                        mixed_buffer[mixed_pos++] = ch;
                        mixed_buffer[mixed_pos] = '\0';
                    }
                    last_id = client_id[i];

                    if (mixed_pos >= CHUNK_SIZE) {
                        char t[16];
                        now_hms(t);
                        printf("[%s] message: id=%d == %s", t, last_id, mixed_buffer);
                        putchar('\n');
                        fflush(stdout);
                        mixed_pos = 0;
                    }
                } else if (bytes == -1) {
                    if (!(errno == EAGAIN || errno == EWOULDBLOCK)) {
                        close(fds[i].fd);
                        fds[i].fd = -1;
                    }
                } else if (bytes == 0) {
                    char t[16];
                    now_hms(t);

                    int secs = (int)difftime(time(NULL), connect_time[i]);
                    printf("[%s] - disconnect: id=%d fd=%d (alive=%ds)\n",
                           t, client_id[i], fds[i].fd, secs);
                    fflush(stdout);

                    close(fds[i].fd);
                    fds[i].fd = -1;
                }
            }
        }

        for (int i = 0; i < client_count; i++) {
            if (fds[i].fd == -1) {
                for (int j = i; j < client_count - 1; j++) {
                    fds[j] = fds[j + 1];
                    client_id[j] = client_id[j + 1];
                    connect_time[j] = connect_time[j + 1];
                }
                i--;
                client_count--;
            }
        }
    }

    {
        char t[16];
        now_hms(t);
        printf("[%s] server: stopping...\n", t);
        fflush(stdout);
    }

    for (int i = 0; i < client_count; i++) {
        if (fds[i].fd != -1) close(fds[i].fd);
    }
    unlink(SOCKET_PATH);
    return 0;
}
