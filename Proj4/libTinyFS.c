#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
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
    buffer[1] = 0x44;
    buffer[2] = 1;

    int retValue = writeBlock(disk, 0, buffer);
    if (retValue < 0) {
        return -1;
    }

    /* setup all middle blocks (init as free blocks) */
    buffer[0] = 4;
    buffer[1] = 0x44;
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



    return 0;
}


/* TODO temp for testing */
int main(int argc, char** argv) {

    tfs_mkfs("a.txt", BLOCKSIZE * NUM_BLOCKS);

    printf("got here\n");

    return 0;
}
