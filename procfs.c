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
#define DRNTSIZE sizeof(struct dirent)
#define PIDINFO 1


char * inodeType[]= {
        [1]     "DIR\0",
        [2]     "FILE\0",
        [3]      "DEV\0",
};



char * procState[]= {
        [0]     "UNUSED\0",
        [1]     "EMBRYO\0",
        [2]      "SLEEPING\0",
        [3]      "RUNNABLE\0",
        [4]      "RUNNING\0",
        [5]      "ZOMBIE\0",
};


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

int atoi(char * str){
    int res = 0; // Initialize result
    // Iterate  all chars of input string and update result

    for (int i = 0; str[i] != '\0'; ++i)
        res = res * 10 + str[i] - '0';

    return res;

}


int appentStrToBuf(char * buf , char * str, uint off){
    int n = strlen(str);
    memmove(buf+off, str, n);
    return n;
}

int appendNumToBuf(char *buf, int num, uint off){
    char numContainer[DIRSIZ];
    memset(numContainer, 0, DIRSIZ);
    itoa(numContainer, num);
    return appentStrToBuf(buf, numContainer , off);
}



int appendDirentTobuf(char * buf, ushort inum, char * name, int direntNum){

    struct dirent drnt;
    drnt.inum = inum;
    memset(&drnt.name,0,DIRSIZ);
    memmove(&drnt.name, name, strlen(name));
    memmove(buf+(direntNum* DRNTSIZE),(void *)&drnt,DRNTSIZE );

    return DRNTSIZE;
}
int brLine(char * buf, int off){
    return appentStrToBuf(buf,"\n", off);
}


int appendDirAliases(struct inode * ip , char * buf){

    appendDirentTobuf(buf, ip->inum,".", 0);
    appendDirentTobuf(buf, namei("")->inum,"..", 1);
    return 2 * DRNTSIZE;
}

int ideInfo(char *ansBuf){
    uint size =0;
    uint num_Write_waiting=0;
    uint num_Read_waiting=0;
    uint num_Waiting=0;
    struct buf **pp;
    struct buf *cur;

    acquire(getidelock());
    for(pp=getidequeue(); *pp; pp=&(*pp)->qnext){
        cur = *pp;
        if(cur->flags & B_DIRTY)
            num_Write_waiting++;
        if(cur->flags & B_VALID)
            num_Read_waiting++;
        if(cur->flags & (B_VALID|B_DIRTY))
            num_Waiting++;
    }

    size+=appentStrToBuf(ansBuf, "Waiting operations: ", size);
    size+=appendNumToBuf(ansBuf, num_Waiting, size);
    size += brLine(ansBuf,size);
    size+=appentStrToBuf(ansBuf,"Read waiting operations: ", size);
    size+=appendNumToBuf(ansBuf, num_Read_waiting, size);
    size += brLine(ansBuf,size);
    size+=appentStrToBuf(ansBuf,"Write waiting operations: ", size);
    size+=appendNumToBuf(ansBuf, num_Write_waiting, size);
    size += brLine(ansBuf,size);
    size+=appentStrToBuf(ansBuf,"Working blocks: ", size);
    for(pp=getidequeue(); *pp; pp=&(*pp)->qnext){
        cur = *pp;
        size+=appentStrToBuf(ansBuf,"(", size);
        size+=appendNumToBuf(ansBuf, cur->dev, size);
        size+=appentStrToBuf(ansBuf,",", size);
        size+=appendNumToBuf(ansBuf, cur->blockno, size);
        size+=appentStrToBuf(ansBuf,");", size);
    }
    release(getidelock());
    size += brLine(ansBuf,size);
    return size;
}

int filestatInfo(char *ansBuf){
    uint size =0;
    updateCounters();
    size+=appentStrToBuf(ansBuf, "Free fds:", size);
    size+=appendNumToBuf(ansBuf, get_num_free_fd(),size);
    size += brLine(ansBuf,size);
    size+=appentStrToBuf(ansBuf,"Unique inode fds:", size);
    size+=appendNumToBuf(ansBuf, get_num_Unique_inode_fds(), size);
    size += brLine(ansBuf,size);
    size+=appentStrToBuf(ansBuf,"Writeable fds:", size);
    size+=appendNumToBuf(ansBuf, get_num_writeable_fds(), size);
    size += brLine(ansBuf,size);
    size+=appentStrToBuf(ansBuf,"Readable fds:", size);
    size+=appendNumToBuf(ansBuf, get_num_readable_fds(), size);
    size += brLine(ansBuf,size);
    size+=appentStrToBuf(ansBuf,"Refs per fds:", size);
    size+=appendNumToBuf(ansBuf, get_refs_per_fds(), size);
    size += brLine(ansBuf,size);
    return size;
}

int getProcDirents(struct inode *ip, char *buf){
    int size = 0;
    char dirname[DIRSIZ];
    memset(dirname,0,DIRSIZ);
    size +=appendDirAliases(ip,buf);
    size +=appendDirentTobuf(buf,num_inodes+1,"ideinfo", 2);
    size +=appendDirentTobuf(buf,num_inodes+2,"filestat",3 );
    size +=appendDirentTobuf(buf,num_inodes+3,"inodeinfo", 4);
    int pids  [NPROC]={0};
    usedPids(pids);

    int i;
    for(i=0; i<NPROC && pids[i]!=-1 ;i++){
        memset(dirname,0,DIRSIZ);
        itoa(dirname,pids[i]);
        size +=appendDirentTobuf(buf,num_inodes + 3000 + (pids[i] * 50), dirname, i+5);
    }

    return size;
}
int getInodeDirents(struct inode *ip, char *buf){

    int size = appendDirAliases(ip,buf);
    int inodes[num_inodes];
    getInodesInUse(inodes);
    char name[14];
    int i;
    for( i = 0; i<num_inodes  && inodes[i] != -1; i++ ){
        memset(name,0,DIRSIZ);
        itoa(name ,inodes[i]);
        size+=appendDirentTobuf(buf,num_inodes + 10 + (i * 10 ) , name , i);
    }
    return size;
}


int countBlksInUse(struct inode * cp){
    int num=0;
    if (cp->type != T_DEV) {
        for (int i=0; i<NDIRECT+1; i++) {
            if(cp->addrs[i] != 0) {
                num++;
                if(i == NDIRECT){
                    int  * addrs = (int *) cp->addrs[NDIRECT];
                    for(i=0; i<NINDIRECT ;i++){
                        if(addrs[i] != 0)
                            num++;
                    }
                }
            }
        }
    }
    return num;
}


int getInodeInfo(struct inode *ip, char *buf){
    int nblksUse=0 , size=0;
    int indx = (ip->inum -num_inodes - 10) / 10;  // extract indx

    struct inode * cp= getInodeByIndx(indx);

    nblksUse = countBlksInUse(cp);

    // fill buf with info
    size += appentStrToBuf(buf, "Device:",size);
    size += appendNumToBuf(buf, cp->dev,size);
    size += brLine(buf,size);
    size += appentStrToBuf(buf, "Inode number:",size);
    size += appendNumToBuf(buf, cp->inum,size);
    size += brLine(buf,size);
    size += appentStrToBuf(buf, "Is valid:",size);
    size += appendNumToBuf(buf, cp->valid,size);
    size += brLine(buf,size);
    size += appentStrToBuf(buf, "Type:",size);
    size += appentStrToBuf(buf, inodeType[cp->type],size);
    size += brLine(buf,size);
    size += appentStrToBuf(buf, "Major minor: (",size);
    size += appendNumToBuf(buf, cp->minor,size);
    size += appentStrToBuf(buf, ",",size);
    size += appendNumToBuf(buf, cp->minor,size);
    size += appentStrToBuf(buf, ")",size);
    size += brLine(buf,size);
    size += appentStrToBuf(buf, "Hard links:",size);
    size += appendNumToBuf(buf, cp->nlink,size);
    size += brLine(buf,size);
    size += appentStrToBuf(buf, "blocks used:", size);
    size += appendNumToBuf(buf, nblksUse, size);
    size += brLine(buf,size);

    return size;
}

int procInfoDirents(struct inode *ip, char *buf){

    appendDirAliases(ip,buf);
    appendDirentTobuf(buf,ip->inum+1,"name", 2);
    appendDirentTobuf(buf,ip->inum+2,"status",3 );
    return 4 * DRNTSIZE;

}

int procName(struct inode *ip, char *buf){

    int pid = (ip->inum - num_inodes - 3000) / 50;
    struct proc * p = getProc(pid);
    int size =0;
    size += appentStrToBuf(buf, p->name , 0);
    size += brLine(buf,size);
    return size;
}

int procStatus(struct inode *ip, char *buf){
    int size =0;
    int pid = (ip->inum - num_inodes - 3000) / 50;
    struct proc * p = getProc(pid);

    size += appentStrToBuf(buf, "Process State:", 0);
    size += appentStrToBuf(buf, procState[p->state], size);
    size += brLine(buf,size);
    size += appentStrToBuf(buf, "Memory usage:", size);
    size += appendNumToBuf(buf, p->sz, size);
    size += brLine(buf,size);
    return size;
}

int getDirents(struct inode *ip, char *buf){

    if(ip->inum <= num_inodes)                              // proc
        return  getProcDirents(ip, buf);
    if(ip->inum == num_inodes + 1)                          // ideinfo
        return  ideInfo(buf);
    if(ip->inum == num_inodes + 2)                          // filestat
        return  filestatInfo(buf);
    if(ip->inum == num_inodes + 3)                          // inodeinfo dir
        return  getInodeDirents(ip, buf);
    if(ip->minor == PIDINFO) {                              // PID files and dirs
        int slot50 = (ip->inum - num_inodes - 3000)%50;
        if(slot50 == 0)                                     //proc num i dir
            return procInfoDirents(ip, buf);
        if(slot50 == 1)                                          // name link
            return procName(ip, buf);
        if(slot50 == 2)                                          // status
            return procStatus(ip , buf);
    }
    if((ip->inum-num_inodes-10)%10 == 0 )                   //inodeinfo file
        return getInodeInfo(ip, buf);



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


    int slot50 = (ip->inum - num_inodes - 3000) % 50;
    if( slot50 == 0 || slot50 == 1 || slot50 == 2) //proc num i dir
        ip->minor = PIDINFO;
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
    int sizeWritten = getDirents(ip, dierntsBuff);
    int  bytesRead;
    if (off <= sizeWritten) {
        bytesRead = sizeWritten - off;
        if (bytesRead < n) {
            memmove(dst, dierntsBuff + off, bytesRead);
            return bytesRead;
        }
        sizeWritten = min(sizeWritten, n);
        memmove(dst, dierntsBuff + off, sizeWritten);
        return sizeWritten;
    }
    return 0;
}

int
procfswrite(struct inode *ip, char *buf, int n)
{

    return 0;
}

void
procfsinit(void)
{
    devsw[PROCFS].isdir = procfsisdir;
    devsw[PROCFS].iread = procfsiread;
    devsw[PROCFS].write = procfswrite;
    devsw[PROCFS].read = procfsread;
}
