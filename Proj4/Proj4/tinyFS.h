#ifndef TINYFS_H
#define TINYFS_H

/* The default size of the disk and file system block */
#define BLOCKSIZE 256
/* Your program should use a 10240 Byte disk size giving you 40 blocks total. This is a default size. You must be able to support different possible values */
#define DEFAULT_DISK_SIZE 10240
/* use this name for a default emulated disk file name */
#define DEFAULT_DISK_NAME "tinyFSDisk"
/* use as a special type to keep track of files */
typedef int fileDescriptor;


int tfs_mkfs(char *filename, int nBytes);
/* Makes a blank TinyFS file system of size nBytes on the unix file specified by ‘filename’. This function should use the emulated disk library to open the specified unix file, and upon success, format the file to be a mountable disk. This includes initializing all data to 0x00, setting magic numbers, initializing and writing the superblock and inodes, etc. Must return a specified success/error code. */


int tfs_mount(char *diskname);
int tfs_unmount(void);
/* tfs_mount(char *diskname) “mounts” a TinyFS file system located within ‘diskname’. tfs_unmount(void) “unmounts” the currently mounted file system. As part of the mount operation, tfs_mount should verify the file system is the correct type. In tinyFS, only one file system may be mounted at a time. Use tfs_unmount to cleanly unmount the currently mounted file system. Must return a specified success/error code. */


fileDescriptor tfs_openFile(char *name);
/* Creates or Opens a file for reading and writing on the currently mounted file system. Creates a dynamic resource table entry for the file, and returns a file descriptor (integer) that can be used to reference this entry while the filesystem is mounted. */


int tfs_closeFile(fileDescriptor FD);
/* Closes the file, de-allocates all system resources, and removes table entry */


int tfs_writeFile(fileDescriptor FD,char *buffer, int size);
/* Writes buffer ‘buffer’ of size ‘size’, which represents an entire file’s content, to the file system. Previous content (if any) will be
completely lost. Sets the file pointer to 0 (the start of file) when done. Returns success/error codes. */


int tfs_deleteFile(fileDescriptor FD);
/* deletes a file and marks its blocks as free on disk. */


int tfs_readByte(fileDescriptor FD, char *buffer);
/* reads one byte from the file and copies it to buffer, using the current file pointer location and incrementing it by one upon success. If the file pointer is already past the end of the file then tfs_readByte() should return an error and not increment the file pointer. */


int tfs_seek(fileDescriptor FD, int offset);
/* change the file pointer location to offset (absolute). Returns success/error codes.*/


#endif
