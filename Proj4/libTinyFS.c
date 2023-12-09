#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
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
#define NUM_BLOCKS 8

#define VALID_BYTE 0x44
char *curDisk = NULL;
int curDiskNum = -1;

/* helper */
int compareFilename(char *buffer, char *name)
{
    /* hardcoded for my implementation of filename in file's inode block */
    char *filename = &buffer[7];
    if (strcmp(filename, name) == 0)
    {
        return 1;
    }
    return 0;
}
void printBuffer(char *buffer)
{
    int i;
    for (i = 0; i < BLOCKSIZE; i++)
    {
        printf("%02x ", (unsigned char)buffer[i]);
        if ((i + 1) % 16 == 0)
        {
            printf("\n");
        }
    }
    if (BLOCKSIZE % 16 != 0)
    {
        printf("\n\n");
    }
    fflush(stdout);
}

int tfs_mkfs(char *filename, int nBytes)
{

    curDiskNum = openDisk(filename, nBytes);

    /* note: div is automatically floored */
    int nBlocks = nBytes / BLOCKSIZE;

    /* setup superblock */
    char *buffer;
    buffer = malloc(BLOCKSIZE * sizeof(char));
    if (buffer == NULL)
    {
        return -1;
    }
    memset(buffer, 0, BLOCKSIZE);
    buffer[0] = 1;
    buffer[1] = VALID_BYTE;
    buffer[2] = 1;       /* "pointer" to root dir inode block */
    buffer[4] = nBlocks; /* additional meta data */
    buffer[5] = 2;       /* "pointer" to init free block linked list */

    int retValue = writeBlock(curDiskNum, 0, buffer);
    if (retValue < 0)
    {
        return -1;
    }

    /* setup root dir inode block */
    memset(buffer, 0, BLOCKSIZE);
    buffer[0] = 2;
    buffer[1] = VALID_BYTE;
    retValue = writeBlock(curDiskNum, 1, buffer);
    if (retValue < 0)
    {
        return -1;
    }

    /* setup all middle blocks (init as free blocks) */
    buffer[0] = 4;
    int i;
    for (i = 2; i < nBlocks - 1; i++)
    {
        buffer[2] = i + 1;

        retValue = writeBlock(curDiskNum, i, buffer);
        if (retValue < 0)
        {
            return -1;
        }
    }

    /* setup final block; pointer is null */
    buffer[2] = 0;
    retValue = writeBlock(curDiskNum, nBlocks - 1, buffer);
    if (retValue < 0)
    {
        return -1;
    }

    free(buffer);
    buffer = NULL;

    return 0;
}

int tfs_mount(char *diskname)
{
    /* attempt to unmount any currently mounted system */
    int unmounted = tfs_unmount();

    /* read the superblock */
    char *buffer;
    buffer = malloc(BLOCKSIZE * sizeof(char));
    if (buffer == NULL)
    {
        return -1;
    }
    if (readBlock(curDiskNum, 0, buffer) < 0)
    {
        return -1;
    }

    /* get nBlocks out of superblock */
    int nBlocks = buffer[4];

    /* verify expected structure of each block */
    int i;
    int offset;
    for (i = 0; i < nBlocks; i++)
    {
        if (readBlock(curDiskNum, i, buffer) < 0)
        {
            return -1;
        }

        if ((unsigned char)buffer[1] != VALID_BYTE)
        {
            return -1;
        }
    }

    free(buffer);
    buffer = NULL;

    /* set current mounted file system */
    curDisk = (char *)malloc(strlen(diskname) + 1);
    if (curDisk == NULL)
    {
        return -1;
    }
    strcpy(curDisk, diskname);

    return 0;
}

int tfs_unmount(void)
{
    if (curDisk == NULL)
    {
        return -1;
    }

    curDisk = NULL;
    return 0;
}

fileDescriptor tfs_openFile(char *name)
{
    if (strlen(name) > 8)
    {
        printf("error\n");
        return -1;
    }

    /* read the superblock */
    char *superblock;
    superblock = malloc(BLOCKSIZE * sizeof(char));
    if (superblock == NULL)
    {
        return -1;
    }
    if (readBlock(curDiskNum, 0, superblock) < 0)
    {
        return -1;
    }

    /* read the root dir inode */
    char *rootdir;
    rootdir = malloc(BLOCKSIZE * sizeof(char));
    if (rootdir == NULL)
    {
        return -1;
    }
    if (readBlock(curDiskNum, 1, rootdir) < 0)
    {
        return -1;
    }

    /* make sure dir is not full, and open file list is not full */
    if (rootdir[BLOCKSIZE - 1] != 0 || superblock[BLOCKSIZE - 1] != 0)
    {
        printf("error\n");
        return -1;
    }

    /* traverse directory for if file already exists */
    char *buffer;
    buffer = malloc(BLOCKSIZE * sizeof(char));
    if (buffer == NULL)
    {
        return -1;
    }
    int fileExists = 0;
    int fileInodeIdx = -1;
    int inodeListIdx = 4;
    while (rootdir[inodeListIdx] != 0)
    {
        memset(buffer, 0, BLOCKSIZE);
        if (readBlock(curDiskNum, rootdir[inodeListIdx], buffer) < 0)
        {
            return -1;
        }

        if (compareFilename(buffer, name))
        {
            fileExists = 1;
            fileInodeIdx = rootdir[inodeListIdx];
            break;
        }

        inodeListIdx++;
    }

    int fd;

    if (fileExists)
    {
        /* check if the file is already open */
        int openListIdx = 6;
        while (superblock[openListIdx] != 0)
        {
            if (superblock[openListIdx] == fileInodeIdx)
            {
                return fileInodeIdx;
            }
            openListIdx++;
        }

        /* if not already open, add to open list */
        superblock[openListIdx] = fileInodeIdx;

        fd = fileInodeIdx;
    }
    else
    {
        /* ----- find free block */
        int prevFreeBlockIdx;
        int freeBlockIdx = superblock[5];
        int penultimate = 0;
        int last = 0;

        while (freeBlockIdx != 0)
        {
            memset(buffer, 0, BLOCKSIZE);
            if (readBlock(curDiskNum, freeBlockIdx, buffer) < 0)
            {
                return -1;
            }

            if (buffer[2] == 0)
            {
                last = freeBlockIdx;
                break;
            }

            penultimate = freeBlockIdx;
            freeBlockIdx = buffer[2];
        }

        if (penultimate == 0)
        {
            prevFreeBlockIdx = last;
        }
        else
        {
            prevFreeBlockIdx = penultimate;
        }

        /* accordingly edit the free LL */
        memset(buffer, 0, BLOCKSIZE);
        buffer[0] = 4;
        buffer[1] = VALID_BYTE;
        buffer[2] = 0;
        int retValue = writeBlock(curDiskNum, prevFreeBlockIdx, buffer);
        if (retValue < 0)
        {
            return -1;
        }

        /* ------ turn free block into new file's inode */
        memset(buffer, 0, BLOCKSIZE);
        buffer[0] = 2;
        buffer[1] = VALID_BYTE;
        buffer[2] = 0;
        strcpy(&buffer[7], name);
        retValue = writeBlock(curDiskNum, freeBlockIdx, buffer);
        if (retValue < 0)
        {
            return -1;
        }

        /* ----- edit meta-data lists */
        /* add new open file to open list */
        int openListIdx = 6;
        while (superblock[openListIdx] != 0)
        {
            openListIdx++;
        }
        superblock[openListIdx] = freeBlockIdx;
        retValue = writeBlock(curDiskNum, 0, superblock);
        if (retValue < 0)
        {
            return -1;
        }

        /* add new open file to directory list */
        int dirListIdx = 4;
        while (rootdir[dirListIdx] != 0)
        {
            dirListIdx++;
        }
        rootdir[dirListIdx] = freeBlockIdx;
        retValue = writeBlock(curDiskNum, 1, rootdir);
        if (retValue < 0)
        {
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

int tfs_closeFile(fileDescriptor FD)
{
    /* read the superblock */
    char *superblock;
    superblock = malloc(BLOCKSIZE * sizeof(char));
    if (superblock == NULL)
    {
        return -1;
    }
    if (readBlock(curDiskNum, 0, superblock) < 0)
    {
        return -1;
    }

    /* check if the file is already open */
    int openListIdx = 6;
    int foundOpen = 0;
    while (superblock[openListIdx] != 0)
    {
        if (superblock[openListIdx] == FD)
        {
            foundOpen = 1;
            break;
        }
        openListIdx++;
    }

    if (!foundOpen)
    {
        printf("error\n");
        return -1;
    }

    /* update open file list */
    while (superblock[openListIdx] != 0)
    {
        superblock[openListIdx] = superblock[openListIdx + 1];
        openListIdx++;
    }

    /* write the updated superblock back to disk */
    if (writeBlock(curDiskNum, 0, superblock) < 0)
    {
        printf("error\n");
        return -1;
    }

    free(superblock);
    superblock = NULL;

    return 0;
}

int tfs_writeFile(fileDescriptor FD, char *buffer, int size)
{
    /* read the superblock */
    char *superblock;
    superblock = malloc(BLOCKSIZE * sizeof(char));
    if (superblock == NULL)
    {
        return -1;
    }
    if (readBlock(curDiskNum, 0, superblock) < 0)
    {
        return -1;
    }

    /* check if the file is already open */
    int openListIdx = 6;
    int fileIdx = 0;
    int foundOpen = 0;
    while (superblock[openListIdx] != 0)
    {
        if (superblock[openListIdx] == FD)
        {
            foundOpen = 1;
            fileIdx = superblock[openListIdx];
            break;
        }
        openListIdx++;
    }
    if (!foundOpen)
    {
        printf("error\n");
        return -1;
    }

    /* read the root dir inode */
    char *rootdir;
    rootdir = malloc(BLOCKSIZE * sizeof(char));
    if (rootdir == NULL)
    {
        return -1;
    }
    if (readBlock(curDiskNum, 1, rootdir) < 0)
    {
        return -1;
    }

    /* read the file inode */
    char *inode;
    inode = malloc(BLOCKSIZE * sizeof(char));
    if (inode == NULL)
    {
        return -1;
    }
    if (readBlock(curDiskNum, fileIdx, inode) < 0)
    {
        return -1;
    }

    int n = (size + (BLOCKSIZE - 4) - 1) / (BLOCKSIZE - 4);

    int curFreeIdx;
    int curFileIdx;
    char *curFile;
    char *freeBlock;

    char *curFree;
    curFree = malloc(BLOCKSIZE * sizeof(char));
    if (curFree == NULL)
    {
        return -1;
    }

    /* traverse to end of free-block LL */
    freeBlock = malloc(BLOCKSIZE * sizeof(char));
    if (freeBlock == NULL)
    {
        return -1;
    }
    int freeBlockIdx = superblock[5];
    int prevBlockIdx = superblock[5];
    if (freeBlockIdx == 0)
    {
        printf("error\n");
        return -1;
    }
    int nFree = 0;
    while (freeBlockIdx != 0)
    {
        memset(freeBlock, 0, BLOCKSIZE);
        if (readBlock(curDiskNum, freeBlockIdx, freeBlock) < 0)
        {
            return -1;
        }
        nFree++;
        prevBlockIdx = freeBlockIdx;
        freeBlockIdx = freeBlock[2];
    }

    /* if there's not enough free space, exit */
    if (n > nFree)
    {
        printf("error\n");
        return -1;
    }

    /* clear any current file extents, and free them, (b/c overwrite) */
    if (inode[2] != 0)
    {

        /* ----- traverse file extent blocks, and turn into free blocks
            and append to free LL
        */
        if (readBlock(curDiskNum, prevBlockIdx, curFree) < 0)
        {
            return -1;
        }

        curFile = malloc(BLOCKSIZE * sizeof(char));
        if (curFile == NULL)
        {
            return -1;
        }
        if (readBlock(curDiskNum, inode[2], curFile) < 0)
        {
            return -1;
        }

        curFreeIdx = prevBlockIdx;
        curFileIdx = inode[2];
        int nextFileIdx;
        while (1)
        {
            nextFileIdx = curFile[2];

            /* edit and rewrite free block */
            curFree[2] = curFileIdx;
            if (writeBlock(curDiskNum, curFreeIdx, curFree) < 0)
            {
                printf("error\n");
                return -1;
            }

            /* rewrite file block to be a free block */
            memset(curFile, 0, BLOCKSIZE);
            curFile[0] = 4;
            curFile[1] = 0x44;
            if (writeBlock(curDiskNum, curFileIdx, curFile) < 0)
            {
                printf("error\n");
                return -1;
            }

            if (nextFileIdx == 0)
            {
                break;
            }

            memcpy(curFree, curFile, BLOCKSIZE);
            curFreeIdx = curFileIdx;

            if (readBlock(curDiskNum, nextFileIdx, curFile) < 0)
            {
                return -1;
            }
            curFileIdx = nextFileIdx;
        }

        free(curFile);
        curFile = NULL;
    }

    if (n < 1)
    {
        /* only modify the inode next pointer */
        inode[2] = 0;
    }
    else
    {
        /* init file pointer to first file extent block, byte 0 */
        inode[4] = 1;
        inode[5] = 0;

        curFreeIdx = superblock[5];
        if (readBlock(curDiskNum, curFreeIdx, curFree) < 0)
        {
            return -1;
        }

        /* update inode to point to beginning of file extents */
        inode[2] = curFreeIdx;

        int i;
        int nextFreeIdx;
        char *bufferOffset;
        for (i = 0; i < n; i++)
        {
            nextFreeIdx = curFree[2];

            memset(curFree, 0, BLOCKSIZE);
            curFree[0] = 3;
            curFree[1] = 0x44;

            bufferOffset = buffer + (i * 252);
            if (i == n - 1)
            {
                curFree[2] = 0;
                int rem = size % 252;
                memcpy(curFree + 3, bufferOffset, rem);
            }
            else
            {
                curFree[2] = nextFreeIdx;
                memcpy(curFree + 3, bufferOffset, 252);
            }

            if (writeBlock(curDiskNum, curFreeIdx, curFree) < 0)
            {
                return -1;
            }

            curFreeIdx = nextFreeIdx;
            if (readBlock(curDiskNum, nextFreeIdx, curFree) < 0)
            {
                return -1;
            }
        }

        /* update superblock to point to new beginning of free LL */
        superblock[5] = curFree[2];
        if (writeBlock(curDiskNum, 0, superblock) < 0)
        {
            return -1;
        }
    }

    /* finally rewrite inode block */
    inode[6] = size;

    if (writeBlock(curDiskNum, fileIdx, inode) < 0)
    {
        return -1;
    }

    free(superblock);
    superblock = NULL;
    free(rootdir);
    rootdir = NULL;
    free(inode);
    inode = NULL;
    free(freeBlock);
    freeBlock = NULL;
    free(curFree);
    curFree = NULL;

    return 0;
}

int tfs_deleteFile(fileDescriptor FD)
{
    /* attempt to first close the file if necessary */
    int closeRes = tfs_closeFile(FD);

    /* read the superblock */
    char *superblock;
    superblock = malloc(BLOCKSIZE * sizeof(char));
    if (superblock == NULL)
    {
        return -1;
    }
    if (readBlock(curDiskNum, 0, superblock) < 0)
    {
        return -1;
    }

    /* read the root dir inode */
    char *rootdir;
    rootdir = malloc(BLOCKSIZE * sizeof(char));
    if (rootdir == NULL)
    {
        return -1;
    }
    if (readBlock(curDiskNum, 1, rootdir) < 0)
    {
        return -1;
    }

    /* traverse directory for if file already exists */
    int fileExists = 0;
    int fileInodeIdx = -1;
    int inodeListIdx = 4;
    while (rootdir[inodeListIdx] != 0)
    {
        if (rootdir[inodeListIdx] == FD)
        {
            fileExists = 1;
            fileInodeIdx = rootdir[inodeListIdx];
            break;
        }
        inodeListIdx++;
    }
    if (!fileExists)
    {
        printf("error\n");
        return -1;
    }
    /* delete file pointer from root dir list */
    while (rootdir[inodeListIdx] != 0)
    {
        rootdir[inodeListIdx] = rootdir[inodeListIdx + 1];
        inodeListIdx++;
    }
    if (inodeListIdx > 0 && rootdir[inodeListIdx - 1] != 0)
    {
        rootdir[inodeListIdx - 1] = 0;
    }
    if (writeBlock(curDiskNum, 1, rootdir) < 0)
    {
        printf("error\n");
        return -1;
    }

    /* traverse to end of free-block LL */
    char *freeBlock;
    freeBlock = malloc(BLOCKSIZE * sizeof(char));
    if (freeBlock == NULL)
    {
        return -1;
    }
    int freeBlockIdx = superblock[5];
    int prevBlockIdx = superblock[5];
    if (freeBlockIdx == 0)
    {
        printf("error\n");
        return -1;
    }
    while (freeBlockIdx != 0)
    {
        memset(freeBlock, 0, BLOCKSIZE);
        if (readBlock(curDiskNum, freeBlockIdx, freeBlock) < 0)
        {
            return -1;
        }

        prevBlockIdx = freeBlockIdx;
        freeBlockIdx = freeBlock[2];
    }

    /* ----- traverse file extent blocks, and turn into free blocks
        and append to free LL
    */
    char *curFile;
    curFile = malloc(BLOCKSIZE * sizeof(char));
    if (curFile == NULL)
    {
        return -1;
    }
    if (readBlock(curDiskNum, fileInodeIdx, curFile) < 0)
    {
        return -1;
    }

    char *curFree;
    curFree = malloc(BLOCKSIZE * sizeof(char));
    if (curFree == NULL)
    {
        return -1;
    }
    if (readBlock(curDiskNum, prevBlockIdx, curFree) < 0)
    {
        return -1;
    }

    int curFreeIdx = prevBlockIdx;
    int curFileIdx = fileInodeIdx;
    int nextFileIdx;
    while (1)
    {
        nextFileIdx = curFile[2];

        /* edit and rewrite free block */
        curFree[2] = curFileIdx;
        if (writeBlock(curDiskNum, curFreeIdx, curFree) < 0)
        {
            printf("error\n");
            return -1;
        }

        /* rewrite file block to be a free block */
        memset(curFile, 0, BLOCKSIZE);
        curFile[0] = 4;
        curFile[1] = 0x44;
        if (writeBlock(curDiskNum, curFileIdx, curFile) < 0)
        {
            printf("error\n");
            return -1;
        }

        if (nextFileIdx == 0)
        {
            break;
        }

        memcpy(curFree, curFile, BLOCKSIZE);
        curFreeIdx = curFileIdx;

        if (readBlock(curDiskNum, nextFileIdx, curFile) < 0)
        {
            return -1;
        }
        curFileIdx = nextFileIdx;
    }

    free(superblock);
    superblock = NULL;
    free(rootdir);
    rootdir = NULL;
    free(freeBlock);
    freeBlock = NULL;
    free(curFile);
    curFile = NULL;
    free(curFree);
    curFree = NULL;

    return 0;
}

int tfs_readByte(fileDescriptor FD, char *buffer)
{
    /* read the superblock */
    char *superblock;
    superblock = malloc(BLOCKSIZE * sizeof(char));
    if (superblock == NULL)
    {
        return -1;
    }
    if (readBlock(curDiskNum, 0, superblock) < 0)
    {
        return -1;
    }

    /* check if the file is already open */
    int openListIdx = 6;
    int fileIdx = 0;
    int foundOpen = 0;
    while (superblock[openListIdx] != 0)
    {
        if (superblock[openListIdx] == FD)
        {
            foundOpen = 1;
            fileIdx = superblock[openListIdx];
            break;
        }
        openListIdx++;
    }
    if (!foundOpen)
    {
        printf("error\n");
        return -1;
    }

    /* read the root dir inode */
    char *rootdir;
    rootdir = malloc(BLOCKSIZE * sizeof(char));
    if (rootdir == NULL)
    {
        return -1;
    }
    if (readBlock(curDiskNum, 1, rootdir) < 0)
    {
        return -1;
    }

    /* read the file inode */
    char *inode;
    inode = malloc(BLOCKSIZE * sizeof(char));
    if (inode == NULL)
    {
        return -1;
    }
    if (readBlock(curDiskNum, fileIdx, inode) < 0)
    {
        return -1;
    }

    int fpBi = inode[4];
    int fpBo = inode[5];

    int nExtents = (inode[6] + (BLOCKSIZE - 4) - 1) / (BLOCKSIZE - 4);

    /* ----- check if FP is already past end of file */
    char *block;
    block = malloc(BLOCKSIZE * sizeof(char));
    if (block == NULL)
    {
        return -1;
    }

    int next = inode[2];
    if (next == 0)
    {
        printf("error\n");
        return -1;
    }

    int llIdx = 1;
    int blockFound = 0;
    while (next != 0)
    {
        if (readBlock(curDiskNum, next, block) < 0)
        {
            return -1;
        }

        if (llIdx == fpBi)
        {
            blockFound = 1;
            break;
        }

        llIdx++;
        next = block[2];
    }

    if (llIdx > nExtents)
    {
        printf("fp beyond file\n");
        return -1;
    }

    if (!blockFound)
    {
        printf("error\n");
        return -1;
    }

    /* increment file pointer */
    if (inode[5] == 252)
    {
        inode[4] += 1;
        inode[5] = 0;
    }
    else
    {
        inode[5] += 1;
    }
    if (writeBlock(curDiskNum, fileIdx, inode) < 0)
    {
        printf("error\n");
        return -1;
    }

    buffer[0] = block[3 + fpBo];
    buffer[1] = '\0';

    free(superblock);
    superblock = NULL;
    free(rootdir);
    rootdir = NULL;
    free(inode);
    inode = NULL;
    free(block);
    block = NULL;

    return 0;
}

int tfs_seek(fileDescriptor FD, int offset)
{
    /* change the file pointer location to
    offset (absolute). Returns success/error codes.
    */

    /* read the superblock */
    char *superblock;
    superblock = malloc(BLOCKSIZE * sizeof(char));
    if (superblock == NULL)
    {
        return -1;
    }
    if (readBlock(curDiskNum, 0, superblock) < 0)
    {
        return -1;
    }

    /* check if the file is already open */
    int openListIdx = 6;
    int fileIdx = 0;
    int foundOpen = 0;
    while (superblock[openListIdx] != 0)
    {
        if (superblock[openListIdx] == FD)
        {
            foundOpen = 1;
            fileIdx = superblock[openListIdx];
            break;
        }
        openListIdx++;
    }
    if (!foundOpen)
    {
        printf("error\n");
        return -1;
    }

    /* read the file inode */
    char *inode;
    inode = malloc(BLOCKSIZE * sizeof(char));
    if (inode == NULL)
    {
        return -1;
    }
    if (readBlock(curDiskNum, fileIdx, inode) < 0)
    {
        return -1;
    }

    if (offset > inode[6])
    {
        printf("offset greater than file size\n");
        return -1;
    }

    /* modify and write back */
    int fpBi = (offset / 252) + 1;
    int fpBo = offset % 252;

    inode[4] = fpBi;
    inode[5] = fpBo;

    if (writeBlock(curDiskNum, fileIdx, inode) < 0)
    {
        return -1;
    }

    free(superblock);
    superblock = NULL;
    free(inode);
    inode = NULL;

    return 0;
}

/* TODO temp for testing */
int main(int argc, char **argv)
{

    /* ---- mkfs test */
    tfs_mkfs("a.txt", BLOCKSIZE * NUM_BLOCKS);

    printf("got here\n");

    /* ---- mount test */
    int res = tfs_mount("a.txt");
    printf("res: %d\n", res);
    printf("curDisk = %s\n", curDisk);

    /* ---- open and delete files test */

    int fd = tfs_openFile("TFS_f1");

    printf("got here fd: %d\n", fd);

    int fd2 = tfs_openFile("TFS_f2");

    fd2 = tfs_openFile("TFS_f2");

    /* ---- write tests */
    int sizeToWrite = 10;
    char *toWrite = (char *)malloc(sizeToWrite * sizeof(char));
    if (toWrite == NULL)
    {
        printf("error allocaitng\n");
        return -1;
    }
    int i;
    for (i = 0; i < sizeToWrite; i++)
    {
        toWrite[i] = 'A' + i;
    }

    for (i = 0; i < sizeToWrite; i++)
    {
        printf("%c ", toWrite[i]);
    }
    printf("\n");

    int wres = tfs_writeFile(fd2, toWrite, sizeToWrite);

    // sizeToWrite = 300;
    // char *toWritet = (char*) malloc(sizeToWrite * sizeof(char));
    // if (toWritet == NULL) {
    //     printf("error allocaitng\n");
    //     return -1;
    // }
    // srand((unsigned int)time(NULL));
    // for (i = 0; i < sizeToWrite; i++) {
    //     toWritet[i] = (char)(rand() % 256);
    // }
    // wres = tfs_writeFile(fd, toWritet, sizeToWrite);

    /* ---- readByte and seek tests */
    char *rb = (char *)malloc(2 * sizeof(char));
    if (rb == NULL)
    {
        printf("error allocating\n");
        return -1;
    }
    int rbres = tfs_readByte(fd2, rb);
    printf("Read byte: %c\n", rb[0]);

    rbres = tfs_readByte(fd2, rb);
    printf("Read byte: %c\n", rb[0]);

    rbres = tfs_readByte(fd2, rb);
    printf("Read byte: %c\n", rb[0]);

    int seekres = tfs_seek(fd2, 8);
    rbres = tfs_readByte(fd2, rb);
    printf("Read byte: %c\n", rb[0]);

    printf("got to end of main!\n");

    return 0;
}
