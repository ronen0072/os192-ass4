#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"
#define DRNTSIZE sizeof(struct dirent)
#define NUM_PARAMS 7
#define DIRSIZ 14
#define INODEINFOS_PATH "/proc/inodeinfo"

//struct dirent {
//    ushort inum;
//    char name[DIRSIZ];
//};
//
//struct inode {
//    uint dev;           // Device number
//    uint inum;          // Inode number
//    int ref;            // Reference count
//    struct sleeplock lock; // protects everything below here
//    int valid;          // inode has been read from disk?
//
//    short type;         // copy of disk inode
//    short major;
//    short minor;
//    short nlink;
//    uint size;
//    uint addrs[NDIRECT+1];
//};

int appentStrToBuf(char * buf , char * str, uint off){
    int n = strlen(str);
    memmove(buf+off, str, n);
    return n;
}


void print_inodeInfo(char * file_name) {

    int nparams = 0;
    char buf[512];

    char full_path[42];
    char param[10];
    char *ch;
    int i = 0;
    int size =0;
    memset(full_path, 0, 32);
    size += appentStrToBuf(full_path,INODEINFOS_PATH, 0 );
    size += appentStrToBuf(full_path, "/", size);
    size += appentStrToBuf(full_path, file_name, size);
    memset(buf,0,512);

    int fd = open(full_path, O_RDONLY);
    if (read(fd, buf, sizeof(buf)) < 0) { // read content to buf
        printf(1, "cat: lsnd error\n");
        exit();
    }

    ch = buf;
    while (nparams < NUM_PARAMS) {
        i=0;
        memset(param, 0, 10);
        nparams++;

        // skip labels:
        while (*ch != ':')
            ch++;
        // extract param
        ch++;
        while (*ch != '\n') {
            param[i] = *ch;
            i++;
            ch++;
        }
        param[i] = ' ';

        if (write(1, param, strlen(param)) < 0) {
            printf(1, "cat: lsnd error\n");
            exit();
        }

    }

    close(fd);
}




void lsnd(){

    int n;
    struct dirent drnt;
    memset((void *)&drnt, 0, DRNTSIZE);
    int fd = open(INODEINFOS_PATH, O_RDONLY);
    while((n = read(fd, &drnt, DRNTSIZE)) > 0) {// read content to buf

        if (drnt.inum > 200) {  //  a virtual file
            print_inodeInfo(drnt.name);
            printf(1, "\n");
        }
    }
    close(fd);
}


int
main(int argc, char *argv[])
{

    lsnd();
    exit();
}




