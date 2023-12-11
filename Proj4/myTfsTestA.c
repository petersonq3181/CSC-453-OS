#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tinyFS.h"
#include "libTinyFS.h"
#include "TinyFS_errno.h"


int main(int argc, char** argv) {

    /* ---- mkfs test */
    tfs_mkfs("a.txt", DEFAULT_DISK_SIZE);

    /* ---- mount test */
    int res = tfs_mount("a.txt");

    /* ---- open and delete files test */

    int fd = tfs_openFile("TFS_f1");

    printf("got here fd: %d\n", fd);

    int fd2 = tfs_openFile("TFS_f2");

    fd2 = tfs_openFile("TFS_f2");

    /* ---- write tests */
    int sizeToWrite = 10;
    char *toWrite = (char*) malloc(sizeToWrite * sizeof(char));
    if (toWrite == NULL) {
        printf("error allocaitng\n");
        return TINYFS_ERR_MALLOC_FAIL;
    }
    int i; 
    for (i = 0; i < sizeToWrite; i++) {
        toWrite[i] = 'A' + i; 
    }

    for (i = 0; i < sizeToWrite; i++) {
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
    char *rb = (char*)malloc(2 * sizeof(char));
    if (rb == NULL) {
        printf("error allocating\n");
        return TINYFS_ERR_MALLOC_FAIL;
    }
    int rbres = tfs_readByte(fd2, rb);
    printf("Read byte: %c\n", rb[0]);

    rbres = tfs_readByte(fd2, rb);
    printf("Read byte: %c\n", rb[0]);

    rbres = tfs_readByte(fd2, rb);
    printf("Read byte: %c\n", rb[0]);

    rbres = tfs_readByte(fd2, rb);
    printf("Read byte: %c\n", rb[0]);

    int seekres = tfs_seek(fd2, 5);

    /* tfs_rename testing */
    int renameres = tfs_rename(fd, "newn");
    printf("rename res: %d\n", renameres);

    /* readdir testing */
    int readres = tfs_readdir();

    /* tfs_makeRO test */
    int makerores = tfs_makeRO("newn");
    makerores = tfs_makeRW("newn");

    /* tfs_writeByte test */
    int wbres = tfs_writeByte(fd2, 7);

    printf("got to end of main!\n");

    return 0;
}
