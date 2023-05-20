#include "gx_ipc.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>

#include <string.h>
#include <stdlib.h>


/*
 * Create shared memory
 */
int iCreateMem( unsigned long lKey, long lSize, long lHugePageSize)
{
    long permissions = IPC_CREAT | IPC_EXCL | 0644;
    
    if ( lHugePageSize <= 0 )
        lHugePageSize = 1;
//    else
//        permissions |= SHM_HUGETLB;
    
    lSize = lSize + (lHugePageSize - (lSize % lHugePageSize));

    if (shmget((key_t)lKey, lSize, permissions ) < 0)
    {
        perror( "shmget" );
        return GX_IPC_ERROR_SHMEMGET;
    }

	return 0;
}

/*
 * Delete shared memory
 */
int iDestroyMem( unsigned long lKey)
{
	int iMemID;

	if ((iMemID = shmget((key_t)lKey, (uint)0, 0644 )) < 0)
	{
		perror( "shmget" );
		return GX_IPC_ERROR_SHMEMGET;
	}

	if (shmctl(iMemID, IPC_RMID, 0) < 0)
	{
		perror( "shmctl" );
		return GX_IPC_ERROR_SHMCTL;
	}

	return 0;
}

/*
 * Attach to shared memory - use intimate shared memory if available
 */
int iAttachMem(unsigned long lKey, void ** pMem, int iShmFlag, int iPermission )
{
    int iMemID;

    if ((iMemID = shmget((key_t)lKey, (uint)0, iPermission )) < 0)
    {
        //UTPLog(LOG_ERR, 0310, "Error attaching to memory with key %ld\n", lKey);
        perror( "shmget" );
        return GX_IPC_ERROR_SHMEMGET;
    }

    *pMem = shmat(iMemID, NULL, iShmFlag);
    if (*pMem == (void *)-1)
    {
        perror( "shmat" );
        *pMem = NULL;
        return GX_IPC_ERROR_SHMAT;
    } /* endif */

    return 0;
}

/*
 * Attach to shared memory - use intimate shared memory if available
 */
int iAttachMem(unsigned long lKey, void ** pMem, int iShmFlag)
{
	int iMemID;

	if ((iMemID = shmget((key_t)lKey, (uint)0, 0644 )) < 0)
	{
		//UTPLog(LOG_ERR, 0310, "Error attaching to memory with key %ld\n", lKey);
		perror( "shmget" );
		return GX_IPC_ERROR_SHMEMGET;
	}

	*pMem = shmat(iMemID, NULL, iShmFlag);
	if (*pMem == (void *)-1)
	{
		perror( "shmat" );
		*pMem = NULL;
		return GX_IPC_ERROR_SHMAT;
	} /* endif */

	return 0;
}

/*
 * Attach to shared memory
 */
int iAttachMem(unsigned long lKey, void ** pMem)
{
    return iAttachMem(lKey, pMem, 0);
}

/*
 * Create memmap memory
 */
int iCreateMemMap(const char * sFilename, unsigned long lSize)
{
	/*
	 * Open file and fill
	 */
	int iFileFD;

	if ((iFileFD = open(sFilename, O_RDWR | O_CREAT | O_EXCL | O_TRUNC, 0644 )) < 0)
	{
		perror( "open" );
		return GX_IPC_ERROR_OPEN_FILE;
	}

	if (lseek(iFileFD, lSize - 1, SEEK_SET) < 0)
	{
		perror( "lseek" );
		close(iFileFD);
		return GX_IPC_ERROR_FILE_SEEK;
	}

	if (write(iFileFD, "\0", 1) < 0)
	{
		perror( "write" );
		close(iFileFD);
		return GX_IPC_ERROR_FILE_WRITE;
	}

	return 0;
}

/*
 * Destroy memmap memory
 */
int iDestroyMemMap(const char * sFilename, char * pMem, unsigned long lSize)
{
	if (munmap(pMem, lSize) < 0)
	{
		perror( "munmap" );
		return GX_IPC_ERROR_MUNMAP;
	}

	if (unlink(sFilename) < 0)
	{
		perror( "unlink" );
		return GX_IPC_ERROR_FILE_REMOVE;
	}

	return 0;
}

/*
 * Attach to memmap memory
 */
int iAttachMemMap(const char * sFilename, unsigned long lSize, void ** pMem)
{
	int iFileFD;

	if ((iFileFD = open(sFilename, O_RDWR, 0644)) < 0)
	{
		perror( "open" );
		return GX_IPC_ERROR_OPEN_FILE;
	}

	/*
	 * Create memmap
	 */

	*pMem = mmap(NULL, lSize, PROT_READ|PROT_WRITE, MAP_SHARED, iFileFD, 0);
	if (*pMem == (void *)-1)
	{
		perror( "mmap" );
		close(iFileFD);
		return GX_IPC_ERROR_MEMMAP;
	}


	close(iFileFD);

	return 0;
}


/*
 * Attach Mem Map to Specified File Location with specific permissions
 */
int iAttachMemMap( const char * sFilename, unsigned long MemMapSize, unsigned long lFileOffset,  int iPermissions, void ** pMem )
{

	int iFileFD;
	if ((iFileFD = open(sFilename, O_RDWR, 0644)) < 0)
	{
		perror( "open" );
		return GX_IPC_ERROR_OPEN_FILE;
	}


	*pMem = mmap(NULL, MemMapSize, iPermissions, MAP_SHARED, iFileFD, lFileOffset );
	if (*pMem == (void *)-1)
	{
		perror( "mmap" );
		close( iFileFD );
		return GX_IPC_ERROR_MEMMAP;
	}


	close(iFileFD);


	return 0;


}
/*
 * Detach from memmap memory
 */
int iDetachMemMap(char * pMem, unsigned long lSize)
{
	if ( munmap( pMem, lSize ) == -1 )
	{
		perror( "munmap" );
		return GX_IPC_ERROR_MUNMAP;
	}

	return 0;
}

/*
 * Extend File
 */
int iExtendFile( const char * sFilename, unsigned long lAdditionalFileSize )
{
	int iFileFD;


	if ((iFileFD = open(sFilename, O_RDWR, 0644)) == -1 )
	{
		perror( "open" );
		return GX_IPC_ERROR_OPEN_FILE;
	}

	if ( lseek( iFileFD, lAdditionalFileSize - 1, SEEK_END) < (off_t)-1 )
	{
		perror( "lseek" );
		close(iFileFD);
		return GX_IPC_ERROR_FILE_SEEK;
	}

	if (write( iFileFD, "\0", 1 ) < 0 )
	{
		perror( "write" );
		close(iFileFD);
		return GX_IPC_ERROR_FILE_WRITE;
	}

	close(iFileFD);

	return 0;
}


