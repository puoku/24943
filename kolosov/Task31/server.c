#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <time.h>

#define SOCK_PATH "socket_path"

int main(void)
{
    int srv, cli, i, n, maxfd;
    int clients[FD_SETSIZE];
    struct sockaddr_un addr;
    fd_set rfds;
    char buf[1024];

    char outbuf[65536];
    int outlen = 0;
    int delayed = 1; 
    time_t start = time(NULL);

    srv = socket(AF_UNIX, SOCK_STREAM, 0);

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCK_PATH);
    unlink(SOCK_PATH);

    bind(srv, (struct sockaddr *)&addr, sizeof(addr));
    listen(srv, 5);

    for (i = 0; i < FD_SETSIZE; i++)
        clients[i] = -1;

    while (1) {
        FD_ZERO(&rfds);
        FD_SET(srv, &rfds);
        maxfd = srv;

        for (i = 0; i < FD_SETSIZE; i++) {
            if (clients[i] != -1) {
                FD_SET(clients[i], &rfds);
                if (clients[i] > maxfd)
                    maxfd = clients[i];
            }
        }

        struct timeval tv, *ptv = NULL;
        if (delayed) {
            tv.tv_sec = 1;
            tv.tv_usec = 0;
            ptv = &tv;
        }

        select(maxfd + 1, &rfds, NULL, NULL, ptv);

        if (delayed && time(NULL) - start >= 10) {
            delayed = 0;
            if (outlen > 0)
                write(1, outbuf, outlen);
            break;
        }

        if (FD_ISSET(srv, &rfds)) {
            cli = accept(srv, NULL, NULL);
            for (i = 0; i < FD_SETSIZE; i++){
                if (clients[i] == -1) {
                    clients[i] = cli;
                    break;
                }
            }
        }

        for (i = 0; i < FD_SETSIZE; i++) {
            int fd = clients[i];
            if (fd != -1 && FD_ISSET(fd, &rfds)) {
                n = read(fd, buf, sizeof(buf));
                if (n <= 0) {
                    close(fd);
                    clients[i] = -1;
                } else {
                    int j;
                    for (j = 0; j < n; j++)
                        buf[j] = (char)toupper((unsigned char)buf[j]);

                    if (delayed) {
                        if (outlen + n < (int)sizeof(outbuf)) {
                            memcpy(outbuf + outlen, buf, n);
                            outlen += n;
                        }
                    } else {
                        write(1, buf, n);
                    }
                }
            }
        }
    }
    close(srv);
    unlink(SOCK_PATH);
    return 0;
}
