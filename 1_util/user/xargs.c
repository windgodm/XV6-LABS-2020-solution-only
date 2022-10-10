#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    char buf[512], *p, *pp;
    int n_argc;
    char *n_argv[16];

    // new argv[i] <= origin argv[i+1]
    for(n_argc = 0; n_argc < argc - 1; n_argc++)
        n_argv[n_argc] = argv[n_argc + 1];

    for(p = buf, pp = buf; read(0, p, 1);){
        if(*p == ' '){
            // add arg
            n_argv[n_argc++] = pp;
            *p++ = 0;
            pp = p;
        }
        else if(*p == '\n'){
            // add arg
            n_argv[n_argc++] = pp;
            *p = 0;
            // set zero end
            n_argv[n_argc] = 0;
            // fork & exec
            if(fork()==0){
                exec(n_argv[0], n_argv);
            }
            wait(0);
            // clear additional arg
            p = buf;
            pp = buf;
            n_argc = argc - 1;
        }
        else{
            p++;
        }
    }

    exit(0);
}
