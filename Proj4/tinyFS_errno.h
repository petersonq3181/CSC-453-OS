#ifndef TINYFS_ERRNO_H
#define TINYFS_ERRNO_H


#define TINYFS_ERR_FILE_NOT_FOUND  -1 /* File not found */
#define TINYFS_ERR_DISK_FULL       -2 /* No space left on disk */
#define TINYFS_ERR_MALLOC_FAIL     -3 /* Malloc failed */
#define TINYFS_ERR_WRITE_BLCK      -4 /* BDD write block failed */
#define TINYFS_ERR_READ_BLCK       -5 /* BDD write block failed */
#define TINYFS_ERR_INVALID_FS      -6 /* Invalid fs structure or key number */
#define TINYFS_ERR_NO_FS           -7 /* No fs mounted */
#define TINYFS_ERR_NAME_TOO_LONG   -8 /* Filename too long */
#define TINYFS_ERR_FILE            -9 /* Too many open files */
#define TINYFS_ERR_FILE_NOT_OPEN   -10 /* File not open */
#define TINYFS_ERR_FILE_NO_DATA    -11 /* File has no data */
#define TINYFS_ERR_FILE_FP         -12 /* FP beyond file content */
#define TINYFS_ERR_OFFSET_FP       -13 /* Offset beyond file content */
#define TINYFS_ERR_READ_ONLY       -14 /* Offset beyond file content */


#endif
