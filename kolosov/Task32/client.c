#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCK_PATH "socket_path"

int main(void)
{
    int s, i;
    struct sockaddr_un addr;
    char buf[256];

    s = socket(AF_UNIX, SOCK_STREAM, 0);

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCK_PATH);

    connect(s, (struct sockaddr *)&addr, sizeof(addr));

    for (i = 1; i <= 100; i++) {
        int n = snprintf(buf, sizeof(buf),
                         "message %d from client\n", i);
        write(s, buf, n);
        usleep(10000);
    }

    close(s);
    return 0;
}
