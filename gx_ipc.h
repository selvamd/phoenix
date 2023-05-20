#ifndef _GX_IPC_X_
#define _GX_IPC_X_

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>

/*
 * Error codes
 */
#define GX_IPC_ERROR_SHMEMGET    -1
#define GX_IPC_ERROR_SHMCTL      -2
#define GX_IPC_ERROR_SHMAT       -3
#define GX_IPC_ERROR_OPEN_FILE   -4
#define GX_IPC_ERROR_FILE_SEEK   -5
#define GX_IPC_ERROR_FILE_WRITE  -6
#define GX_IPC_ERROR_FILE_REMOVE -7
#define GX_IPC_ERROR_MEMMAP      -8
#define GX_IPC_ERROR_MUNMAP      -9
#define GX_IPC_ERROR_INVALID_HUGE_PAGE_SIZE -10

/*
 * Create shared memory
 */
int iCreateMem(unsigned long lKey, long lSize, long lHugePageSize = 1); //

/*
 * Delete shared memory
 */
int iDestroyMem(unsigned long lKey);

/*
 * Attach to shared memory
 */
int iAttachMem(unsigned long lKey, void ** pMem);

/*
 * Attach to shared memory
 */
int iAttachMem(unsigned long lKey, void ** pMem, int iShmFlag);

/*
 * Attach to shared memory
 */
int iAttachMem(unsigned long lKey, void ** pMem, int iShmFlag, int iPermission );

/*
 * Create memmap memory
 */
int iCreateMemMap(const char * sFilename, unsigned long lSize);

/*
 * Destroy memmap memory
 */
int iDestroyMemMap(const char * sFilename, char * pMem, unsigned long lSize);

/*
 * Attach to memmap memory
 */
int iAttachMemMap(const char * sFilename, unsigned long lSize, void ** pMem);

/*
 * Attach Mem Map to Specified File Location
 */
int iAttachMemMap(const char * sFilename, unsigned long lMemMapSize, unsigned long lFileOffset,  int iPermissions, void ** pMem );


/*
 * Detach from memmap memory
 */
int iDetachMemMap(char * pMem, unsigned long lSize);

/*
 * Extend File
 */
int iExtendFile(const char * sFilename, unsigned long lAdditionalFileSize);




#endif
