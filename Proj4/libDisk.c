#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "libDisk.h"
#include "tinyFS.h"
#include "TinyFS_errno.h"

int idCount = 0; 
DiskLL* diskHead = NULL;

/* for myself for testing */
void printLinkedList(DiskLL *head) {
    DiskLL *current = head;
    while (current != NULL) {
        printf("ID: %d, FD: %d, Filename: %s\n", current->id, current->fd, current->filename ? current->filename : "NULL");
        current = current->next;
    }
}

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
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Error opening file");
        return -1;
    }
    char zero = 0;
    int i;
    for (i = 0; i < nBytes; ++i) {
        if (fwrite(&zero, 1, 1, file) != 1) {
            fclose(file);
            return -1;
        }
    }
    int fd = 123;

    /* if nBytes > BLOCKSIZE and there is already a file by the given filename, 
    * that file’s content may be overwritten (cur)
    */
    int foundCase1 = 0;
    DiskLL* cur = diskHead;
    while (cur != NULL) {
        if (nBytes > BLOCKSIZE && strcmp(cur->filename, filename) == 0) {
            foundCase1 = 1;

            /* overwrite */
            /* TODO figure out how to do this; could remove the file and make a new one */

            break;
        }
        cur = cur->next;
    }

    /* if nBytes is 0, an existing disk is opened, and 
    * the content must not be overwritten in this function
    */
    int foundCase2 = 0;
    if (nBytes == 0) {
        cur = diskHead;
        
        while (cur != NULL) {
            printf("entered hereee: %s %s\n", cur->filename, filename);
            printLinkedList(diskHead);
            if (strcmp(cur->filename, filename) == 0) {
                foundCase2 = 1;
                break;
            }
            cur = cur->next;
        }

        /* nBytes is 0 and no disk exists with the given filename; return error */
        if (!foundCase2) {
            /* TODO errno */
            return -1; 
        }
    }

    printf("FOUNDCASE 1 and 2 %d %d\n", foundCase1, foundCase2);

    /* if the disk does not already exist, make a new one */
    if (!foundCase1 && !foundCase2) {

        DiskLL* new = (DiskLL*) malloc(sizeof(DiskLL));
        if (new == NULL) {
            /* TODO errno */
            return -1;
        }
        
        new->id = idCount;
        idCount++; 
        
        new->filename = (char *) malloc(strlen(filename) + 1);
        if (new->filename == NULL) {
            free(new);
            return -1;
        }
        strcpy(new->filename, filename);

        new->fd = fd;
        new->next = NULL; 

        if (diskHead == NULL) {
            diskHead = new;
        } else {
            cur = diskHead;
            while (cur->next != NULL) {
                cur = cur->next;
            }

            cur->next = new;   
        }
            
        cur = new;
    }
    
    int out = cur->id;

    /* attempt to close the file */
    fclose(file);

    
    printf("openDisk() Success!\n\t diskNum: %d \n\t filename: %s \n\t nBytes: %d \n\t foundCase1: %d \n\t foundCase2: %d \n"
    , out, filename, nBytes, foundCase1, foundCase2);
    printLinkedList(diskHead);
    
    
    return out; 
}

int closeDisk(int disk) {
    /*
    find disk
    deallocate disk struct(s)
    free from linked list 
    if not found err
    */

    DiskLL *cur = diskHead;
    DiskLL *prev = NULL;
    while (cur != NULL) {
        if (cur->id == disk) {
            if (prev == NULL) {
                diskHead = cur->next;
            } else {
                prev->next = cur->next;
            }
            
            free(cur);
            return 0;
        }
        
        prev = cur;
        cur = cur->next;
    }

    /* TODO errno */
    return -1;
}

int readBlock(int disk, int bNum, void *block) {
    /* TODO how to handle 
    - the specified disk does not exist / isn't configured 
    - offset into file is out of range (could put in pread error conditional)
    */


    DiskLL *cur = diskHead;
    while (cur != NULL) {
        if (cur->id == disk) {
            break;
        }
        cur = cur->next;
    }

    if (cur == NULL) {
        /* TODO errno */
        return -1;
    }

    /* open the file */
    int fd = open(cur->filename, O_RDONLY);
    if (fd == -1) {
        /* TODO errno */
        printf("entered readBlock error: unable to open file\n");
        return -1;
    }

    off_t offset = (off_t) bNum * BLOCKSIZE;

    ssize_t bytesRead = pread(fd, block, BLOCKSIZE, offset);
    if (bytesRead == -1) {
        fprintf(stderr, "pread failed: %s\n", strerror(errno));


        /* TODO errno */
        if (close(fd) == -1) {
            printf("Error closing file '%s'\n", cur->filename);
            return 1;
        }
        return -1;
    }

    if (close(fd) == -1) {
        printf("Error closing file '%s'\n", cur->filename);
        return 1;
    }

    return 0;
}

int writeBlock(int disk, int bNum, void *block) {
    DiskLL *cur = diskHead;
    while (cur != NULL) {
        if (cur->id == disk) {
            break;
        }
        cur = cur->next;
    }

    if (cur == NULL) {
        /* TODO errno */
        printf("entered writeBlock error 1\n");
        return -1;
    }

    /* open the file */
    int fd = open(cur->filename, O_WRONLY);
    if (fd == -1) {
        /* TODO errno */
        printf("entered writeBlock error: unable to open file\n");
        return -1;
    }

    off_t offset = (off_t) bNum * BLOCKSIZE;

    ssize_t bytesWritten = pwrite(fd, block, BLOCKSIZE, offset);
    if (bytesWritten == -1 || bytesWritten < BLOCKSIZE) {
        /* TODO errno */
        printf("entered writeBlock error 2\n");

        if (close(fd) == -1) {
            printf("entered writeBlock error: unable to close file\n");
            return -1;
        }

        return -1;
    }

    if (close(fd) == -1) {
        /* TODO errno */
        printf("entered writeBlock error: unable to close file\n");
        return -1;
    }

    return 0;
}

// /* TODO: delete; temp for testing */
// int main(int argc, char** argv) {

//     /* ------ self testing of openDisk() */

//     /* 1. if the disk does not already exist, make a new one */
//     int diskNum;
//     diskNum = openDisk("file.txt", 334);

//     /* 2. if nBytes > BLOCKSIZE and there is already a file by the given filename */
//     diskNum = openDisk("file.txt", 700);

//     /* 3.a if nBytes is 0, an existing disk is opened, and the content must not be overwritten in this function */
//     diskNum = openDisk("file.txt", 0);
//     /* 3.b if nBytes is 0, and no existing disk */
//     diskNum = openDisk("gg.txt", 0);
     
//     /* additional testing w/ closeDisk */
//     diskNum = openDisk("a.txt", 800);
//     diskNum = openDisk("b.txt", 300);

//     closeDisk(1);
//     diskNum = openDisk("c.txt", 1110);

//     char *buffer;
//     buffer = malloc(BLOCKSIZE * sizeof(char));
//     memset(buffer,'$',BLOCKSIZE);
//     // retValue = writeBlock(disks[index],ctestBlocks[index2],buffer);
//     int gg = writeBlock(2, 2, buffer);

//     char *readBuff;
//     readBuff = malloc(BLOCKSIZE * sizeof(char));
//     int res = readBlock(2, 2, readBuff);
    

//     return 0;
// }
