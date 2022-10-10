#include "kernel/types.h"
#include "user/user.h"

// send number to first child
void
parent(int outfd, int maxn)
{
    for(int i = 2; i <= maxn; i++)
    {
        write(outfd, &i, 4);
    }
}

void
child(int infd)
{
    int btr;
    int n, prime;
    int p[2];

    // process first number

    btr = read(infd, &n, 4);
    // only EOF, stop
    if (btr == 0)
        return;
    printf("prime %d\n", n);
    prime = n;

    // process the rest number

    pipe(p);
    if (fork() == 0) {
        close(p[1]);
        child(p[0]);
        close(p[0]);
        exit(0);
    }

    close(p[0]);
    while (read(infd, &n, 4)) {
        // drop
        if (n % prime == 0)
            continue;
        // trans
        write(p[1], &n, 4);
    }
    close(p[1]);
    wait(0);
}

int
main(int argc, char *argv[])
{
    int p[2]; // parent => child

    pipe(p);
    // parent
    if(fork()) {
        close(p[0]);
        parent(p[1], 35);
        close(p[1]);
        wait(0);
    }
    // child
    else {
        close(p[1]);
        child(p[0]);
        close(p[0]);
        exit(0);
    }
    exit(0);
}
