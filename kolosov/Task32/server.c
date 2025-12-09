#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <time.h>
#include <aio.h>
#include <fcntl.h>

#define SOCKET_PATH "./socket"
#define BUF         1024
#define MAX_CLIENTS 5
#define MAX_OUT     200000

struct client {
    int fd;
    struct aiocb aio;
    char buf[BUF];
    int id;
    int msg_count;
};

int main(void)
{
    int srv, newfd;
    struct sockaddr_un addr;
    struct client clients[MAX_CLIENTS];

    char outbuf[MAX_OUT];
    int outlen = 0;

    int i, n;
    int next_id = 1;

    time_t start;
    struct timeval start_tv;
    struct timeval first, last;
    int have_first = 0;

    for (i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = -1;
        clients[i].id = 0;
        clients[i].msg_count = 0;
    }
    srv = socket(AF_UNIX, SOCK_STREAM, 0);

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCKET_PATH);
    unlink(SOCKET_PATH);

    bind(srv, (struct sockaddr *)&addr, sizeof(addr));
    listen(srv, MAX_CLIENTS);

    {
        int flags = fcntl(srv, F_GETFL, 0);
        fcntl(srv, F_SETFL, flags | O_NONBLOCK);
    }

    start = time(NULL);
    gettimeofday(&start_tv, NULL);

    printf("Async server (Task32) started\n");

    while (1) {
        if (time(NULL) - start >= 15)
            break;

        newfd = accept(srv, NULL, NULL);
        if (newfd >= 0) {
            for (i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].fd < 0) {
                    clients[i].fd = newfd;
                    clients[i].id = next_id++;
                    clients[i].msg_count = 0;

                    memset(&clients[i].aio, 0, sizeof(struct aiocb));
                    clients[i].aio.aio_fildes = newfd;
                    clients[i].aio.aio_buf = clients[i].buf;
                    clients[i].aio.aio_nbytes = BUF;

                    aio_read(&clients[i].aio);
                    break;
                }
            }
        }

        for (i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd >= 0) {
                int err = aio_error(&clients[i].aio);

                if (err == 0) {
                    struct timeval now;
                    long sec, usec;
                    int len, j;
                    int cid;

                    n = aio_return(&clients[i].aio);
                    if (n <= 0) {
                        close(clients[i].fd);
                        clients[i].fd = -1;
                        clients[i].id = 0;
                        clients[i].msg_count = 0;
                        continue;
                    }

                    gettimeofday(&now, NULL);

                    if (!have_first) {
                        first = now;
                        have_first = 1;
                    }
                    last = now;

                    sec = now.tv_sec - start_tv.tv_sec;
                    usec = now.tv_usec - start_tv.tv_usec;
                    if (usec < 0) {
                        sec--;
                        usec += 1000000;
                    }

                    cid = clients[i].id;
                    clients[i].msg_count++;

                    if (outlen < MAX_OUT) {
                        len = snprintf(outbuf + outlen,
                                       MAX_OUT - outlen,
                                       "[%ld.%06ld] %d message from (%d client): ",
                                       sec, usec,
                                       clients[i].msg_count,
                                       cid);
                        if (len > 0) {
                            outlen += len;
                            if (outlen > MAX_OUT)
                                outlen = MAX_OUT;
                        }
                    }

                    for (j = 0; j < n && outlen < MAX_OUT; j++) {
                        char c = clients[i].buf[j];
                        c = (char)toupper((unsigned char)c);
                        outbuf[outlen++] = c;
                    }

                    if (outlen < MAX_OUT)
                        outbuf[outlen++] = '\n';

                    memset(&clients[i].aio, 0, sizeof(struct aiocb));
                    clients[i].aio.aio_fildes = clients[i].fd;
                    clients[i].aio.aio_buf = clients[i].buf;
                    clients[i].aio.aio_nbytes = BUF;
                    aio_read(&clients[i].aio);
                }
            }
        }

        usleep(10000);
    }

    if (outlen > 0)
        write(1, outbuf, outlen);

    if (have_first) {
        long sec = last.tv_sec - first.tv_sec;
        long usec = last.tv_usec - first.tv_usec;
        if (usec < 0) {
            sec--;
            usec += 1000000;
        }
        printf("%ld.%06ld s\n", sec, usec);
    }

    close(srv);
    unlink(SOCKET_PATH);
    return 0;
}
