#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
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
    sizeToWrite = 250;
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
    printf("Additional Feature b. Directory listing and file renaming");
    tfs_readdir();
    printf("\tRenaming TFS_f1 to newname1\n");
    tfs_rename(fd1, "newname1");
    tfs_readdir();

    return 0;
}
