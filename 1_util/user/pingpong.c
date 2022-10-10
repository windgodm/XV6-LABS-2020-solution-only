#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    int p1[2]; // parent => child
    int p2[2]; // child => parent
    int pid;
    char buf[10];

    pipe(p1);
    pipe(p2);
    if(fork()) {
        // parent
        pid = getpid();
        close(p1[0]);
        close(p2[1]);
        write(p1[1], "ping", 4);
        close(p1[1]);
        wait(0); // wait for child read
        read(p2[0], buf, sizeof(buf));
        close(p2[0]);
        printf("%d: received %s\n", pid, buf);
    } else {
        // child
        pid = getpid();
        close(p1[1]);
        close(p2[0]);
        write(p2[1], "pong", 4);
        close(p2[1]);
        read(p1[0], buf, sizeof(buf));
        close(p1[0]);
        printf("%d: received %s\n", pid, buf);
    }
    exit(0);
}
