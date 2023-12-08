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
int curDiskNum = -1;

/* helper */
int compareFilename(char *buffer, char *name) {
    /* hardcoded for my implementation of filename in file's inode block */
    char *filename = &buffer[7];
    if (strcmp(filename, name) == 0) {
        return 1;
    }
    return 0;
}

int tfs_mkfs(char *filename, int nBytes) {
    
    curDiskNum = openDisk(filename, nBytes);

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
    buffer[2] = 1; /* "pointer" to root dir inode block */
    buffer[4] = nBlocks; /* additional meta data */
    buffer[5] = 2; /* "pointer" to init free block linked list */
    
    int retValue = writeBlock(curDiskNum, 0, buffer);
    if (retValue < 0) {
        return -1;
    }

    /* setup root dir inode block */
    memset(buffer, 0, BLOCKSIZE);
    buffer[0] = 2;
    buffer[1] = VALID_BYTE;
    retValue = writeBlock(curDiskNum, 1, buffer);
    if (retValue < 0) {
        return -1;
    }

    /* setup all middle blocks (init as free blocks) */
    buffer[0] = 4;
    int i;
    for (i = 2; i < nBlocks - 1; i++) {
        buffer[2] = i + 1;

        retValue = writeBlock(curDiskNum, i, buffer);
        if (retValue < 0) {
            return -1;
        }
    }

    /* setup final block; pointer is null */
    buffer[2] = 0;
    retValue = writeBlock(curDiskNum, nBlocks - 1, buffer);
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

    /* read the superblock */
    char *buffer;
    buffer = malloc(BLOCKSIZE * sizeof(char));
    if (buffer == NULL) {
        return -1;
    }
    if (readBlock(curDiskNum, 0, buffer) < 0) {
        return -1;
    }

    /* get nBlocks out of superblock */
    int nBlocks = buffer[4];

    /* verify expected structure of each block */
    int i;
    int offset; 
    for (i = 0; i < nBlocks; i++) {
        if (readBlock(curDiskNum, i, buffer) < 0) {
            return -1;
        }

        if ((unsigned char) buffer[1] != VALID_BYTE) {
            return -1;
        }
    }

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

fileDescriptor tfs_openFile(char *name) {
    if (strlen(name) > 8) {
        printf("error\n");
        return -1; 
    }
    
    /* read the superblock */
    char *superblock;
    superblock = malloc(BLOCKSIZE * sizeof(char));
    if (superblock == NULL) {
        return -1;
    }
    if (readBlock(curDiskNum, 0, superblock) < 0) {
        return -1;
    }

    /* read the root dir inode */
    char *rootdir;
    rootdir = malloc(BLOCKSIZE * sizeof(char));
    if (rootdir == NULL) {
        return -1;
    }
    if (readBlock(curDiskNum, 1, rootdir) < 0) {
        return -1;
    }

    /* make sure dir is not full, and open file list is not full */
    if (rootdir[BLOCKSIZE - 1] != 0 || superblock[BLOCKSIZE - 1] != 0) {
        printf("error\n");
        return -1;
    }

    /* traverse directory for if file already exists */
    char *buffer;
    buffer = malloc(BLOCKSIZE * sizeof(char));
    if (buffer == NULL) {
        return -1;
    }
    int fileExists = 0;
    int fileInodeIdx = -1;
    int inodeListIdx = 4;
    while (rootdir[inodeListIdx] != 0) {
        memset(buffer, 0, BLOCKSIZE);
        if (readBlock(curDiskNum, rootdir[inodeListIdx], buffer) < 0) {
            return -1;
        }

        if (compareFilename(buffer, name)) {
            fileExists = 1;
            fileInodeIdx = rootdir[inodeListIdx];
            break; 
        }

        inodeListIdx++; 
    }

    int fd; 

    if (fileExists) {
        /* check if the file is already open */
        int openListIdx = 6;
        while (superblock[openListIdx] != 0) {
            if (superblock[openListIdx] == fileInodeIdx) {
                return fileInodeIdx;
            }
            openListIdx++; 
        }

        /* if not already open, add to open list */
        superblock[openListIdx] = fileInodeIdx;

        fd = fileInodeIdx;
    } else {
        /* ----- find free block */
        int prevFreeBlockIdx; 
        int freeBlockIdx = superblock[5];
        int penultimate = 0;
        int last = 0;

        while (freeBlockIdx != 0) { 
            memset(buffer, 0, BLOCKSIZE);
            if (readBlock(curDiskNum, freeBlockIdx, buffer) < 0) {
                return -1;
            }

            if (buffer[2] == 0) { 
                last = freeBlockIdx;
                break;
            }

            penultimate = freeBlockIdx;
            freeBlockIdx = buffer[2];
        }

        if (penultimate == 0) {
            prevFreeBlockIdx = last;
        } else {
            prevFreeBlockIdx = penultimate;
        }
        
        /* accordingly edit the free LL */
        memset(buffer, 0, BLOCKSIZE);
        buffer[0] = 4;
        buffer[1] = VALID_BYTE;
        buffer[2] = 0;
        int retValue = writeBlock(curDiskNum, prevFreeBlockIdx, buffer);
        if (retValue < 0) {
            return -1;
        }

        /* ------ turn free block into new file's inode */
        memset(buffer, 0, BLOCKSIZE);
        buffer[0] = 2;
        buffer[1] = VALID_BYTE;
        buffer[2] = 0;
        strcpy(&buffer[7], name);
        retValue = writeBlock(curDiskNum, freeBlockIdx, buffer);
        if (retValue < 0) {
            return -1;
        }

        /* ----- edit meta-data lists */
        /* add new open file to open list */
        int openListIdx = 6;
        while (superblock[openListIdx] != 0) {
            openListIdx++; 
        }
        superblock[openListIdx] = freeBlockIdx;
        retValue = writeBlock(curDiskNum, 0, superblock);
        if (retValue < 0) {
            return -1;
        }

        /* add new open file to directory list */
        int dirListIdx = 4;
        while (rootdir[dirListIdx] != 0) {
            dirListIdx++; 
        }
        rootdir[dirListIdx] = freeBlockIdx;
        retValue = writeBlock(curDiskNum, 1, rootdir);
        if (retValue < 0) {
            return -1;
        }

        fd = freeBlockIdx;
    }

    free(superblock);
    free(rootdir);
    free(buffer);
    superblock = NULL;
    rootdir = NULL;
    buffer = NULL;

    return fd; 
}

int tfs_closeFile(fileDescriptor FD) {
    /* read the superblock */
    char *superblock;
    superblock = malloc(BLOCKSIZE * sizeof(char));
    if (superblock == NULL) {
        return -1;
    }
    if (readBlock(curDiskNum, 0, superblock) < 0) {
        return -1;
    }

    /* check if the file is already open */
    int openListIdx = 6;
    int foundOpen = 0;
    while (superblock[openListIdx] != 0) {
        if (superblock[openListIdx] == FD) {
            foundOpen = 1;
            break; 
        }
        openListIdx++; 
    }

    if (!foundOpen) {
        printf("error\n");
        return -1;
    }

    /* update open file list */
    while (superblock[openListIdx] != 0) {
        superblock[openListIdx] = superblock[openListIdx + 1];
        openListIdx++; 
    }

    /* write the updated superblock back to disk */
    if (writeBlock(curDiskNum, 0, superblock) < 0) {
        printf("error\n");
        return -1;
    }

    free(superblock);
    superblock = NULL; 

    return 0;
}

int tfs_deleteFile(fileDescriptor FD) {
    /* attempt to first close the file if necessary */
    int closeRes = tfs_closeFile(FD); 

    /* read the superblock */
    char *superblock;
    superblock = malloc(BLOCKSIZE * sizeof(char));
    if (superblock == NULL) {
        return -1;
    }
    if (readBlock(curDiskNum, 0, superblock) < 0) {
        return -1;
    }

    /* read the root dir inode */
    char *rootdir;
    rootdir = malloc(BLOCKSIZE * sizeof(char));
    if (rootdir == NULL) {
        return -1;
    }
    if (readBlock(curDiskNum, 1, rootdir) < 0) {
        return -1;
    }

    /* traverse directory for if file already exists */
    int fileExists = 0;
    int fileInodeIdx = -1;
    int inodeListIdx = 4;
    while (rootdir[inodeListIdx] != 0) {
        if (rootdir[inodeListIdx] == FD) {
            fileExists = 1;
            fileInodeIdx = rootdir[inodeListIdx];
            break; 
        }
        inodeListIdx++; 
    }
    if (!fileExists) {
        printf("error\n");
        return -1;
    }

    /* traverse to end of free-block LL */
    char *freeBlock;
    freeBlock = malloc(BLOCKSIZE * sizeof(char));
    if (freeBlock == NULL) {
        return -1;
    }
    int freeBlockIdx = superblock[5];
    int prevBlockIdx = superblock[5];
    if (freeBlockIdx == 0) {
        printf("error\n");
        return - 1;
    }
    while (freeBlockIdx != 0) { 
        memset(freeBlock, 0, BLOCKSIZE);
        if (readBlock(curDiskNum, freeBlockIdx, freeBlock) < 0) {
            return -1;
        }

        prevBlockIdx = freeBlockIdx;
        freeBlockIdx = freeBlock[2];
    }

    /* ----- traverse file inode / extent blocks, and turn into free blocks 
        and append to free LL 
    */
    char *fileBlock;
    fileBlock = malloc(BLOCKSIZE * sizeof(char));
    if (fileBlock == NULL) {
        return -1;
    }
    if (readBlock(curDiskNum, fileInodeIdx, fileBlock) < 0) {
        return -1;
    }
    int curFreeBlockIdx = prevBlockIdx; 
    int curFileBlockIdx = fileInodeIdx;
    int nextFileBlockIdx = fileBlock[2]; 
    while (nextFileBlockIdx != 0) {
        nextFileBlockIdx = fileBlock[2]; 

        freeBlock[2] = curFileBlockIdx;
        if (writeBlock(curDiskNum, curFreeBlockIdx, freeBlock) < 0) {
            printf("error\n");
            return -1;
        }

        /* edit file block to be a free block */
        memset(fileBlock, 0, BLOCKSIZE);
        fileBlock[0] = 4;
        fileBlock[1] = 0x44;
        if (writeBlock(curDiskNum, curFileBlockIdx, fileBlock) < 0) {
            printf("error\n");
            return -1;
        }

        memcpy(freeBlock, fileBlock, BLOCKSIZE);
        if (readBlock(curDiskNum, nextFileBlockIdx, fileBlock) < 0) {
            printf("error\n");
            return -1;
        }
    }

    free(rootdir);
    rootdir = NULL; 
    free(freeBlock);
    freeBlock = NULL; 
    free(fileBlock);
    fileBlock = NULL; 

    return 0; 
}

/* TODO temp for testing */
int main(int argc, char** argv) {

    tfs_mkfs("a.txt", BLOCKSIZE * NUM_BLOCKS);

    printf("got here\n");

    int res = tfs_mount("a.txt");
    printf("res: %d\n", res);
    printf("curDisk = %s\n", curDisk);

    int fd = tfs_openFile("TFS_f1");

    printf("got here fd: %d\n", fd);

    int fd2 = tfs_openFile("TFS_f2");

    fd2 = tfs_openFile("TFS_f2");
    int fd3  = tfs_openFile("TFS_f3");

    fd = tfs_closeFile(fd3);
    fd = tfs_closeFile(fd2);


    return 0;
}
