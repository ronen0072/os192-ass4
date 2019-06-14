#include "types.h"
#include "stat.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "buf.h"
#define min(a, b) ((a) < (b) ? (a) : (b))

int num_inodes = 0;

void itoa(char *s, int n){
    int i = 0;
    int len = 0;
    if (n == 0){
        s[0] = '0';
        return;
    }
    while(n != 0){
        s[len] = n % 10 + '0';
        n = n / 10;
        len++;
    }
    for(i = 0; i < len/2; i++){
        char tmp = s[i];
        s[i] = s[len - 1 - i];
        s[len - 1 - i] = tmp;
    }
}



void appendDirentTobuf(char * buf, ushort inum, char * name, int direntNum){
    struct dirent drnt;
   drnt.inum = inum;
   memmove(&drnt.name, name, DIRSIZ);
   memmove(buf+(direntNum* sizeof(drnt)),(void *)&drnt, sizeof(drnt));

}


int getProcDirents(struct inode *ip, char *buf){

    char dirname[DIRSIZ];
    memset(dirname,0,DIRSIZ);
    dirname[0]='.'; //CWD
    appendDirentTobuf(buf, ip->inum,dirname, 0);
    dirname[1] = '.'; //PARENT DIR
    appendDirentTobuf(buf, namei("")->inum,dirname, 1);
    appendDirentTobuf(buf,num_inodes+1,"ideinfo", 2);
    appendDirentTobuf(buf,num_inodes+2,"filestat",3 );
    appendDirentTobuf(buf,num_inodes+3,"inodeinfo", 4);
    int pids  [NPROC]={0};
    usedPids(pids);

    int i;
    for(i=0; i<NPROC && pids[i]!=-1 ;i++){
        memset(dirname,0,DIRSIZ);
        itoa(dirname,pids[i]);
        appendDirentTobuf(buf,num_inodes + 3000 + (i * 50), dirname, i+5);
    }

    return (i+5) * sizeof(struct dirent);
}
int getInodeDirents(struct inode *ip, char *buf){

    int inodes[num_inodes];
    getInodesInUse(inodes);
    char name[14];
    int i;
    for( i = 0; i<num_inodes  && inodes[i] != -1; i++ ){
        memset(name,0,DIRSIZ);
        itoa(name ,inodes[i]);
        appendDirentTobuf(buf,num_inodes + 10 + (i * 10 ) , name , i);
    }
    return i * sizeof(struct dirent);
}


int getDirents(struct inode *ip, char *buf){

    if(ip->inum <= num_inodes)  // case /proc
       return  getProcDirents(ip, buf);
    if(ip->inum == num_inodes + 3) // inodeinfo dir
       return  getInodeDirents(ip, buf);

    return 0;
}
int
procfsisdir(struct inode *ip) {
    if (ip->type != T_DEV || ip->major != PROCFS)
        return 0;

    return 1;
}

void
procfsiread(struct inode* dp, struct inode *ip) {
    ip->valid = VALID;
    ip->major = PROCFS;
    ip->type = T_DEV;
}

int
procfsread(struct inode *ip, char *dst, int off, int n) {

    // double check
    if(ip->type != T_DEV)
       return 0;

    if (num_inodes == 0){    // if it is the first read (open) then get the number of
        struct superblock sb;
        readsb(ip->dev, &sb);
        num_inodes = sb.ninodes;

    }
    char dierntsBuff [1104]; // max size of dirents ( 64 procs+ 3 infos + 2 - aliases)
    memset(dierntsBuff,0,1104);
    int num_dirents = getDirents(ip, dierntsBuff);
    int  bytesRead;
    if (off < num_dirents) {
        bytesRead = num_dirents - off;
        if (bytesRead < n) {
            memmove(dst, dierntsBuff + off, bytesRead);
            return bytesRead;
        }
        num_dirents = min(num_dirents, n);
        memmove(dst, dierntsBuff + off, num_dirents);
        return num_dirents;
    }
    return 0;
}

int
procfswrite(struct inode *ip, char *buf, int n)
{
  //panic("Trying to write to readonly fs");
    return -1;
}

void
procfsinit(void)
{
  devsw[PROCFS].isdir = procfsisdir;
  devsw[PROCFS].iread = procfsiread;
  devsw[PROCFS].write = procfswrite;
  devsw[PROCFS].read = procfsread;
}
