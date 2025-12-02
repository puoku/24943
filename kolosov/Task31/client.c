#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCK_PATH "socket_path"

int main(void)
{
    int s, n;
    struct sockaddr_un addr;
    char buf[1024];

    s = socket(AF_UNIX, SOCK_STREAM, 0);

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCK_PATH);

    connect(s, (struct sockaddr *)&addr, sizeof(addr));

    while ((n = read(0, buf, sizeof(buf))) > 0) {
        write(s, buf, n);
    }

    close(s);
    return 0;
}
