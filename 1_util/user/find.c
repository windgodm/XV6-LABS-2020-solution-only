#include "kernel/types.h"
#include "user/user.h"
#include "kernel/stat.h"
#include "kernel/fs.h"

void
find(char *path, char *file)
{
    int d_fd, f_fd;
    struct stat d_st, f_st;
    struct dirent de;
    char buf[512], *p;

    // get directory message
    if((d_fd = open(path, 0)) < 0){
        fprintf(2, "find: cannot open %s\n", path);
        exit(1);
    }
    if(fstat(d_fd, &d_st) < 0){
        fprintf(2, "find: cannot stat %s\n", path);
        close(d_fd);
        exit(1);
    }
    if(d_st.type != T_DIR){
        close(d_fd);
        return;
    }

    // check each de
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(d_fd, &de, sizeof(de)) == sizeof(de)){
        // pass empty de
        if (de.inum == 0)
            continue;
        // pass '.' or '..'
        if (strcmp(de.name, ".") == 0)
            continue;
        if (strcmp(de.name, "..") == 0)
            continue;

        // get file message
        strcpy(p, de.name);
        if((f_fd = open(buf, 0)) < 0){
            fprintf(2, "find: cannot open %s\n", buf);
            close(d_fd);
            exit(1);
        }
        if(fstat(f_fd, &f_st) < 0){
            fprintf(2, "find: cannot stat %s\n", buf);
            close(f_fd);
            close(d_fd);
            exit(1);
        }
        close(f_fd);

        // find
        if(strcmp(de.name, file) == 0)
            printf("%s\n", buf);

        // search sub-directory
        if(f_st.type == T_DIR)
            find(buf, file);
    }
    
    close(d_fd);
}

int
main(int argc, char *argv[])
{
    char *path, *file;

    if(argc != 3){
        fprintf(2, "Usage: find [path] [file]\n");
        exit(1);
    }
    path = argv[1];
    file = argv[2];

    find(path, file);

    exit(0);
}
