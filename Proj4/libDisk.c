#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "libDisk.h"
#include "tinyFS.h"
#include "tinyFS_errno.h"

#define MAX_DISKS 10

typedef struct {
    int fd;
    char filename[FILENAME_MAX];
    int size;
    int isOpen;
} DiskEmulator;

DiskEmulator openDisks[MAX_DISKS];

int openDisk(char *filename, int nBytes) {

    /* check nBytes is greater than BLOCKSIZE */
    if (nBytes > 0 && nBytes < BLOCKSIZE) {
        /* TODO errnos */
        return -1;
    }

    /* check nBytes is exactly a multiple of BLOCKSIZE */
    if (nBytes % BLOCKSIZE != 0) {
        nBytes -= nBytes % BLOCKSIZE;
    }
    
    /* attempt open the file */
    int fd; 
    fd = open(filename, O_RDWR | O_CREAT, 0600);
    if (fd == -1) {
        return -1; 
    }

    /* if nBytes > BLOCKSIZE and there is already a file by the given filename, 
     * that file’s content may be overwritten
     */
    int overwrite = 0;
    int diskNum = -1;
    int i;
    for (i = 0; i < MAX_DISKS; i++) {
        if (nBytes > BLOCKSIZE && strcmp(openDisks[i].filename, filename) == 0) {
            diskNum = i;
            overwrite = 1;
            break;
        }
    }

    /*
    * TODO Question: what if blockSize = 0, but there are no existing disks that match the filename? 
    * if nBytes is 0, an existing disk is opened, and 
    * the content must not be overwritten in this function
    */
    if (nBytes == 0) {
        for (i = 0; i < MAX_DISKS; i++) {
            if (strcmp(openDisks[i].filename, filename) == 0) {
                diskNum = i;
                overwrite = 0;
                break;
            }
        }
    }

    /* if no existing disk with the given fn has been found, find free spot */
    if (diskNum == -1) {
        for (i = 0; i < MAX_DISKS; i++) {
            if (!openDisks[i].isOpen) {
                diskNum = i;
                break;
            }
        }
    } 
    
    /* TODO fill out this error */
    if (diskNum == -1) {
        return -1;
    }
    
    openDisks[diskNum].fd = fd;
    strncpy(openDisks[diskNum].filename, filename, FILENAME_MAX);
    openDisks[diskNum].size = nBytes;
    openDisks[diskNum].isOpen = 1;

    printf("openDisk() success \n\t diskNum: %d \n\t filename: %s \n\t nBytes: %d \n", diskNum, filename, nBytes);

    return diskNum;
}

int closeDisk(int disk) {
    /* find disk */
    int i;
    int found = -1;
    for (i = 0; i < MAX_DISKS; i++) {
        if (disk == openDisk[i].fd) {
            found = i;
            break;
        }
    }

    if (found == -1) {
        /* TODO errno for no disk found */
        return -1;
    }
    
    /* close the file */
    if (close(openDisk[i].fd) == -1) {
        /* TODO errno for close failure */
        return -1;
    } 

    /* "remove" from openDisks list */
    openDisks[i].fd = 0;
    strncpy(openDisks[i].filename, "\0", FILENAME_MAX);
    openDisks[i].size = 0;
    openDisks[i].isOpen = 0;

    return 0;
}


int readBlock(int disk, int bNum, void *block) {

}


int writeBlock(int disk, int bNum, void *block) {

}

/* TODO: delete; temp for testing */
int main(int argc, char** argv) {

    int fd;
    fd = openDisk("file.txt", 334);
    fd = openDisk("file.txt", 334);

    printf("got here\n");
    printf("fd: %d\n", fd);

    return 0;
}

