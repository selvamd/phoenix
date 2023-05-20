 /********************************************************************
 	SocketStreamerBase.cc - base class for socket message streamers.
	This streamer assumes message stream made up of messages with
	fixed size headers and no message delimiters. The amount to read
	is based on the message header. The virtual iNextMsg method
	provides the mapping between message headers and message lengths.
 *********************************************************************/

#include "SocketStreamerBase.hh"

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <iostream>

SocketStreamerBase::SocketStreamerBase(int iSocket, int iBufferSize, char cDel):
                    iBufferSize(iBufferSize), cDelimiter(cDel), iSockFD(iSocket)
{
	sMsgBuffer = NULL;
	//sWriteBuffer = NULL;
	CreateBuffer();
}

SocketStreamerBase::~SocketStreamerBase()
{
	if ( sMsgBuffer != NULL)
	{
		delete [] sMsgBuffer;
		sMsgBuffer = NULL;
	}
	//if (sWriteBuffer != NULL)
	//{
	//	delete [] sWriteBuffer;
	//	sWriteBuffer = NULL;
	//}
}

void SocketStreamerBase::CreateBuffer()
{
	//if (sWriteBuffer != NULL)
	//	delete [] sWriteBuffer;

	//sWriteBuffer = new char[iBufferSize+1];

	if (sMsgBuffer != NULL)
		delete [] sMsgBuffer;

	sMsgBuffer = new char[iBufferSize];
	//touch it to ensure preallocation
	memset(sMsgBuffer,0,iBufferSize);
	iPos = 0;
	iBytes = 0;
}

//If headSize is 0, then there is no header to parse msglength,
//but parsing is based on delimiter
bool SocketStreamerBase::hasUnreadMsg(int iHeadSize)
{
	if ( iBytes == 0 ) return false;
	if (iHeadSize > 0 && iBytes < iHeadSize )
        return (sMsgBuffer[iPos]=='0' || sMsgBuffer[iPos]=='1');
    return (iHeadSize == 0)? true:(iCalculateMsgSize() < iBytes);
}

int SocketStreamerBase::iReadNonBlockingN(char * sMsg, int iMaxLen, int iHeadSize, bool skipNL)
{
	if (iMaxLen <= 0) {
		return ERROR_INVALID_BUFFER_LEN;
	}

	int iPad = 0 ;		// Do we need to
	if (skipNL) {		// skip unnecessary
		++iPad ;		// New Lines
	}

	if (iBytes > 0) {
		if ((iBufferSize - iPos) < iMaxLen + iPad) {
            //Move buffer contents to make space for new message if needed
			memmove(sMsgBuffer, &sMsgBuffer[iPos], iBytes) ;
			iPos = 0;
		}
	} else {
		iPos = 0;
	}

    //This function will be used for delimited (NewLine) messages
    if (iHeadSize == 0)
    {
        if (iPos > 0) memmove(sMsgBuffer, &sMsgBuffer[iPos], iBytes) ;
        int iNL = 0;
        iPos = 0;
        
        do
        {
            for (;iNL<iBytes;iNL++)
                if (sMsgBuffer[iNL] == '\n')
                    break;
            
            if (iNL == iBytes)
            {
                if (iBytes >= iBufferSize)  return ERROR_TARGET_BUFFER_OVERFLOW;
                int count = read(iSockFD, &sMsgBuffer[iBytes], iBufferSize - iBytes);
                if (count <= 0) return (errno == EAGAIN)? ERROR_EAGAIN:ERROR_SOCKET_READ;
                iBytes += count;
            }
            else
            {
                memcpy(sMsg, sMsgBuffer, iNL) ;
                iBytes -= (iNL+1);
                memcpy(sMsgBuffer, &sMsgBuffer[iNL+1], iBytes);
                return iNL;
            }
        } while (true);
        return ERROR_BUFFER_FULL;
    }
    
    //This function will be used for fixed length messages that have type or length in the headers
	int header = 1 ;
	do {
		while(iBytes < header) {
			int count = 0 ;
			if((count = read(iSockFD, &sMsgBuffer[iPos + iBytes], iBufferSize - (iPos + iBytes))) <= 0) {
				if(errno != EAGAIN) {
					perror( "SocketStreamerBase::iReadNonBlocking(N)" );
					return ERROR_SOCKET_READ;
				}
				return ERROR_EAGAIN;
			}
			iBytes += count ;
		}

        if (header == 1 && !(sMsgBuffer[iPos]=='0' || sMsgBuffer[iPos]=='1')) {
			header  = iHeadSize ;
		}
	} while(iBytes < header) ;

     //Get size of message based on next header.
     //If invalid messages length, return error message.
	int iMsgLen = iCalculateMsgSize();
	if(iMsgLen <= 0) {
		return ERROR_INVALID_MSG_LEN;
	}

	 //Return error if buffer not big enough
	if (iMaxLen < iMsgLen ) {
		memcpy(sMsg, &sMsgBuffer[iPos], ((iMaxLen<iBytes)?iMaxLen:iBytes) );
		return ERROR_TARGET_BUFFER_OVERFLOW;
	}

	//If no complete message found, read from socket
	while (iBytes < iMsgLen + iPad && iBytes < iBufferSize) {
		int count = 0 ;
		if((count = read(iSockFD, &sMsgBuffer[iPos + iBytes], iBufferSize - (iPos + iBytes))) <= 0) {
			if(errno != EAGAIN) {
				perror( "SocketStreamerBase::iReadNonBlocking(N)\n" );
	    		return ERROR_SOCKET_READ;
			}
			return ERROR_EAGAIN;
		}
		iBytes += count ;
	}

	if (iBytes >= iMsgLen + iPad) {
		memcpy(sMsg, &sMsgBuffer[iPos], iMsgLen) ;
		iBytes -= iMsgLen + iPad ;
		iPos += iMsgLen + iPad ;
		return iMsgLen ;
	}

	//Buffer must be full, return error
	return ERROR_BUFFER_FULL;
}

/*
 * 	Write NonBlocking 
 *      return < 0 for failure
 *      return ERROR_EWOULDBLOCK( 0 ) for EWOULDBLOCK.
 * Does not write out delimiter, it is assumed that it is already included in the mesage.
 * For systems where socket write can do partial writes on nonblocking sockets.
 */
int SocketStreamerBase::iWriteNonBlockingN( const char * sMsg, int iLen, int & iAmountWritten )
{
  	size_t iLeft;
  	ssize_t iWritten;
  	const char *ptr;

	if( iSockFD < 0 )
	{
		printf( "Streamer iWriteNonBlocking returning ERROR_NOT_CONNECTED %d\n", ERROR_NOT_CONNECTED );
		return ERROR_NOT_CONNECTED;
	}

  	ptr = (char *)sMsg;
  	iLeft = iLen;
  	while( iLeft > 0 )
    {
      	if( ( iWritten = write( iSockFD, ptr, iLeft ) ) <= 0 )
		{
			if( errno != EWOULDBLOCK )
			{
				perror( "SocketStreamerBase::iWriteNonBlocking Message");
	    		return ERROR_SOCKET_WRITE;
			}
			else
			{
				iAmountWritten = iLen - iLeft;
				return ERROR_EWOULDBLOCK;
			}
		}

      	iLeft -= iWritten;
      	ptr += iWritten;
    }

	iAmountWritten = iLen;
  	return iLen;
}

int SocketStreamerBase::iPublish( const char * sMsg, int iLen)
{
    if (sendto(iSockFD, sMsg, iLen, 0, (struct sockaddr*)&socket_addr, sizeof(socket_addr)) < 0)
    {
        //std::cout << " Error multicasting message." << std::endl;
        perror("Error multicasting message: ");
        return -1;
    }
    return 0;
}

const char *SocketStreamerBase::ErrStr( const int iErrNo )
{
    static struct err
    {
        int errnum;
        const char * str;
    }
    table[] =
    {
        { ERROR_SOCKET_NOT_SET,         "ERROR_SOCKET_NOT_SET" },
        { ERROR_BUFFER_NOT_SET,         "ERROR_BUFFER_NOT_SET" },
        { ERROR_SOCKET_READ,            "ERROR_SOCKET_READ" },
        { ERROR_SOCKET_WRITE,           "ERROR_SOCKET_WRITE" },
        { ERROR_INVALID_MSG_LEN,        "ERROR_INVALID_MSG_LEN" },
        { ERROR_BUFFER_FULL,            "ERROR_BUFFER_FULL" },
        { ERROR_INVALID_BUFFER_LEN,     "ERROR_INVALID_BUFFER_LEN" },
        { ERROR_TARGET_BUFFER_OVERFLOW, "ERROR_TARGET_BUFFER_OVERFLOW" },
        { ERROR_NOT_CONNECTED,          "ERROR_NOT_CONNECTED" },
        { ERROR_UNKNOWN_HEADER,         "ERROR_UNKNOWN_HEADER" },
        { ERROR_DELIMITER_NOT_FOUND,    "ERROR_DELIMITER_NOT_FOUND" },
        { ERROR_SELECT_TIMEOUT,         "ERROR_SELECT_TIMEOUT" },
        { ERROR_SOCKET_SELECT,          "ERROR_SOCKET_SELECT" },
        { ERROR_MISSING_DELIMITER,      "ERROR_MISSING_DELIMITER" },
        { ERROR_EWOULDBLOCK,            "ERROR_EWOULDBLOCK" },
        { ERROR_EAGAIN,                 "ERROR_EAGAIN" },
        { ERROR_HEAD_BUFF_LEN,          "ERROR_HEAD_BUFF_LEN" }
    };

    static char unknown[100];

    for(int i = 0; table[i].errnum != 0; i++)
    {
        if(table[i].errnum == iErrNo)
            return table[i].str;
    }
    snprintf(unknown, 100, "unknown error code [%d]", iErrNo);
    return unknown;
}
