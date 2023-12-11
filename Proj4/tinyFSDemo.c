#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "tinyFS.h"
#include "TinyFS_errno.h"

int main(int argc, char **argv) {

    printf("Entered tinyFS Demo!\n");


    /* ----- try to mount the disk */
    if (tfs_mount(DEFAULT_DISK_NAME) < 0) {
        tfs_mkfs(DEFAULT_DISK_NAME, DEFAULT_DISK_SIZE);

        int mountRes = tfs_mount(DEFAULT_DISK_NAME);
        if (mountRes < 0) {
            return mountRes;
        }
    }

    /* ----- open files */
    int fd1 = tfs_openFile("TFS_f1");
    int fd2 = tfs_openFile("TFS_f2");
    int fd3 = tfs_openFile("TFS_f3");
    int fd4 = tfs_openFile("TFS_f4");

    /* ----- write to files */
    int sizeToWrite;
    char *toWrite; 
    int i;

    /* write some simple content */
    sizeToWrite = 10;
    toWrite = (char*) malloc(sizeToWrite * sizeof(char));
    if (toWrite == NULL) {
        printf("error allocaitng\n");
        return TINYFS_ERR_MALLOC_FAIL;
    }
    for (i = 0; i < sizeToWrite; i++) {
        toWrite[i] = 'A' + i; 
    }
    tfs_writeFile(fd1, toWrite, sizeToWrite);
    free(toWrite);
    toWrite = NULL; 

    /* write some simple content */
    sizeToWrite = 26;
    toWrite = (char*) malloc(sizeToWrite * sizeof(char));
    if (toWrite == NULL) {
        printf("error allocaitng\n");
        return TINYFS_ERR_MALLOC_FAIL;
    }
    for (i = 0; i < sizeToWrite; i++) {
        toWrite[i] = 'A' + i; 
    }
    tfs_writeFile(fd2, toWrite, sizeToWrite);
    tfs_writeFile(fd3, toWrite, sizeToWrite);
    free(toWrite);
    toWrite = NULL; 

    /* write a lot of random conent */
    sizeToWrite = 1000;
    toWrite = (char*) malloc(sizeToWrite * sizeof(char));
    if (toWrite == NULL) {
        printf("error allocaitng\n");
        return -1;
    }
    srand((unsigned int)time(NULL)); 
    for (i = 0; i < sizeToWrite; i++) {
        toWrite[i] = (char)(rand() % 256);
    }
    tfs_writeFile(fd4, toWrite, sizeToWrite);
    free(toWrite);
    toWrite = NULL; 

    /* ----- delete a file */
    tfs_deleteFile(fd3);
    
    /* ----- readByte, and proof of auto file-pointer increment */
    char *rb = (char*) malloc(2 * sizeof(char));
    if (rb == NULL) {
        printf("error allocating\n");
        return -1;
    }
    printf("Reading and seeking tests:\n");
    for (i = 0; i < 5; i++) {
        tfs_readByte(fd2, rb);
        printf("\tRead byte from file FD %d: %c\n", fd1, rb[0]);
    }
    
    /* ----- seek then read */
    printf("\tSeeking to byte 3\n");
    tfs_seek(fd1, 3);
    tfs_readByte(fd1, rb);
    printf("\tRead byte from file FD %d: %c\n", fd1, rb[0]);

    printf("\tSeeking to byte 9\n");
    tfs_seek(fd1, 9);
    tfs_readByte(fd1, rb);
    printf("\tRead byte from file FD %d: %c\n", fd1, rb[0]);

    /* ----- Additional Feature b. Directory listing and file renaming */
    printf("Additional Feature b. Directory listing and file renaming\n");
    tfs_readdir();
    printf("\tRenaming TFS_f1 to newname1\n");
    tfs_rename(fd1, "newname1");
    tfs_readdir();

    /* ----- Additional Feature d. Read-only and writeByte support */
    printf("Additional Feature d. Read-only and writeByte support\n");
    printf("\tMade newname1 file RO, write and delete should fail:\n");
    tfs_makeRO("newname1");
    if (tfs_deleteFile(fd1) < 0) {
        printf("\ttfs_deleteFile failed as expected\n");
    }
    if (tfs_writeFile(fd1, toWrite, sizeToWrite) < 0) {
        printf("\ttfs_writeFile failed as expected\n");
    }
    tfs_makeRW("newname1");
    tfs_writeByte(fd2, 3);
    printf("\tSet newname1 file back to read-write, and wrote to it with writeByte\n");

    /* ----- Additional Feature e. Timestamps */
    time_t fd2creation = tfs_readFileInfo(fd2);
    wait(1);
    int fd5 = tfs_openFile("TFS_f5");
    time_t fd5creation = tfs_readFileInfo(fd5);
    printf("Additional Feature 3. Timestamps\n");
    printf("\tfd1 creation in seconds: %ld\n", fd2creation);
    printf("\tfd5 creation in seconds: %ld\n", fd5creation);


    free(toWrite);
    toWrite = NULL;
    free(rb);
    rb = NULL;

    return 0;
}
