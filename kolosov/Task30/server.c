#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCK_PATH "socket_path"

int main(void){
    int srv, cli, n, i;
    struct sockaddr_un addr;
    char buf[1024];

    srv = socket(AF_UNIX, SOCK_STREAM, 0);

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCK_PATH);
    unlink(SOCK_PATH);

    bind(srv, (struct sockaddr *)&addr, sizeof(addr));
    listen(srv, 1);

    cli = accept(srv, NULL, NULL);
    close(srv);

    while ((n = read(cli, buf, sizeof(buf))) > 0) {
        for (i = 0; i < n; i++)
            buf[i] = (char)toupper((unsigned char)buf[i]);
        write(1, buf, n);
    }

    close(cli);
    unlink(SOCK_PATH);
    return 0;
}
