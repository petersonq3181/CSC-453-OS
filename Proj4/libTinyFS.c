#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "libDisk.h"
#include "tinyFS.h"
#include "libTinyFS.h"


/* 
- use byte 2 of each block as pointer to other blocks 
    - "pointer" as in which block number it points to next 
    - these pointers will keep multiple lists:
        - initially all of it will be the free linked list 
        - once files are wrote, there will be additional file-block linked lists 
    - the last ele of any linked list should have a pointer that is NULL 
- idxs start at 0 ofc, even for blocks 
*/

/* TODO temp for testing */
#define NUM_BLOCKS 12

#define VALID_BYTE 0x44
char* curDisk = NULL;

int tfs_mkfs(char *filename, int nBytes) {
    
    int disk = openDisk(filename, nBytes);

    /* note: div is automatically floored */
    int nBlocks = nBytes / BLOCKSIZE;

    /* setup superblock */
    char *buffer;
    buffer = malloc(BLOCKSIZE * sizeof(char));
    if (buffer == NULL) {
        return -1;
    }
    memset(buffer, 0, BLOCKSIZE);
    buffer[0] = 1;
    buffer[1] = VALID_BYTE;
    buffer[2] = 1;
    /* for myself, store nBlocks in superblock */
    buffer[4] = nBlocks;

    int retValue = writeBlock(disk, 0, buffer);
    if (retValue < 0) {
        return -1;
    }

    /* setup all middle blocks (init as free blocks) */
    buffer[0] = 4;
    int i;
    for (i = 1; i < nBlocks - 1; i++) {
        buffer[2] = i + 1;

        retValue = writeBlock(disk, i, buffer);
        if (retValue < 0) {
            return -1;
        }
    }

    /* setup final block; pointer is null */
    buffer[2] = 0;
    retValue = writeBlock(disk, nBlocks - 1, buffer);
    if (retValue < 0) {
        return -1;
    }

    free(buffer);
    buffer = NULL; 

    return 0;
}

int tfs_mount(char *diskname) {

    /* attempt to unmount any currently mounted system */
    int unmounted = tfs_unmount();

    int fd;
    fd = open(diskname, O_RDONLY);
    if (fd == -1) {
        return -1;
    }

    /* read the superblock */
    char *buffer;
    buffer = malloc(BLOCKSIZE * sizeof(char));
    if (buffer == NULL) {
        return -1;
    }
    if (pread(fd, buffer, BLOCKSIZE, 0) != BLOCKSIZE) {
        close(fd);
        return -1;
    }

    /* get nBlocks out of superblock */
    int nBlocks = buffer[4];

    /* verify expected structure of each block */
    int i;
    int offset; 
    for (i = 0; i < nBlocks; i++) {
        offset = i * BLOCKSIZE;
        if (pread(fd, buffer, BLOCKSIZE, offset) != BLOCKSIZE) {
            close(fd);
            return -1;
        }

        if ((unsigned char) buffer[1] != VALID_BYTE) {
            close(fd);
            return -1;
        }
    }

    close(fd);
    free(buffer);
    buffer = NULL; 

    /* set current mounted file system */
    curDisk = (char *) malloc(strlen(diskname) + 1);
    if (curDisk == NULL) {
        return -1;
    }
    strcpy(curDisk, diskname);

    return 0;
}

int tfs_unmount(void) {
    if (curDisk == NULL) {
        return -1;
    }

    curDisk = NULL;
    return 0; 
}

/* TODO temp for testing */
int main(int argc, char** argv) {

    tfs_mkfs("a.txt", BLOCKSIZE * NUM_BLOCKS);

    printf("got here\n");

    int res = tfs_mount("a.txt");
    printf("res: %d\n", res);
    printf("curDisk = %s\n", curDisk);

    return 0;
}
