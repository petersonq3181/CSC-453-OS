#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "libDisk.h"
#include "tinyFS.h"
#include "tinyFS_errno.h"

int idCount = 0; 
DiskLL* diskHead = NULL;

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

    /* if the disk does not already exist, make a new one */
    if (!foundCase1 && !foundCase2) {

        DiskLL* new = (DiskLL*) malloc(sizeof(DiskLL));
        if (new == NULL) {
            /* TODO errno */
            return -1;
        }
        
        new->id = idCount;
        idCount++; 
        new->filename = filename;
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
    
    return cur->id; 
}

int closeDisk(int disk) {

    /* find disk */
    /*
    int i;
    int found = -1;
    for (i = 0; i < MAX_DISKS; i++) {
        if (disk == openDisk[i].fd) {
            found = i;
            break;
        }
    }
    */

    /*
    if (found == -1) {
        */
        /* TODO errno for no disk found */
        /*
        return -1;
    }
    */
    
    
    /* close the file */
    /* 
    if (close(openDisk[i].fd) == -1) {
        return -1;
    } 
    */

    /* "remove" from openDisks list */
    /*
    openDisks[i].fd = 0;
    strncpy(openDisks[i].filename, "\0", FILENAME_MAX);
    openDisks[i].size = 0;
    openDisks[i].isOpen = 0;
    */

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

    printf("got here\n");
    printf("fd: %d\n", fd);

    return 0;
}
