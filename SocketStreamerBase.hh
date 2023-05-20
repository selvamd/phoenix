 /********************************************************************
    SocketStreamerBase.hh - base class for socket message streamers.
    This streamer assumes message stream made up of messages with
    fixed size headers and no message delimiters. The amount to read
    is based on the message header. The virtual iNextMsg method
    provides the mapping between message headers and message lengths.
    Use delimiter to get next message if unknown header is included
    in the message.
 *********************************************************************/
#ifndef _SOCKET_STREAMER_BASE_HH_
#define _SOCKET_STREAMER_BASE_HH_

#include <stdlib.h>
#include <cstdint>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

class SocketStreamerBase
{
    public:
        static const int DEFAULT_BUFFER_SIZE          = 2048;
        static const char DEFAULT_DELIMITER           = '\n';
        static const int DEFAULT_HEADER_SIZE          = 0;
        static const int ERROR_SOCKET_NOT_SET         = -1;
        static const int ERROR_BUFFER_NOT_SET         = -2;
        static const int ERROR_SOCKET_READ            = -3;
        static const int ERROR_SOCKET_WRITE           = -4;
        static const int ERROR_INVALID_MSG_LEN        = -5;
        static const int ERROR_BUFFER_FULL            = -6;
        static const int ERROR_INVALID_BUFFER_LEN     = -7;
        static const int ERROR_TARGET_BUFFER_OVERFLOW = -8;
        static const int ERROR_NOT_CONNECTED          = -9;
        static const int ERROR_UNKNOWN_HEADER         = -10;
        static const int ERROR_DELIMITER_NOT_FOUND    = -11;
        static const int ERROR_SELECT_TIMEOUT         = -12;
        static const int ERROR_SOCKET_SELECT          = -13;
        static const int ERROR_MISSING_DELIMITER      = -14;
        static const int ERROR_EWOULDBLOCK		      = -15;
        static const int ERROR_EAGAIN		          = -16;
        static const int ERROR_HEAD_BUFF_LEN	      = -17;
        const char *ErrStr( const int iErrNo );
    
	public:
		SocketStreamerBase(int iSocket = -1, int iBufferSize = DEFAULT_BUFFER_SIZE, char cDel = DEFAULT_DELIMITER);
		virtual ~SocketStreamerBase();

		int iReadNonBlockingN(char* sMsg, int iMaxLen, int iHeaderSize, bool iSkipDelimiter = 0) ;
        int iWriteNonBlockingN( const char * sMsg, int iLen, int & iAmountWritten );
        int iPublish(const char * sMsg, int iLen);
		bool hasUnreadMsg(int iHeadSize);
    
		virtual int iCalculateMsgSize() { return 0; };

        void SetSocket(int iSock)       { iSockFD = iSock; }
        void SetMultiCastAddress(struct sockaddr_in addr) { socket_addr = addr; }

		int iGetSocket() const          { return iSockFD; }
		void SetDelimiter(char cDel)    { cDelimiter = cDel; }

	protected:
        int iBufferSize;
        char cDelimiter;
		char * sMsgBuffer;
		//char * sWriteBuffer;
		int iPos;
		int iBytes;

	private:
		int iSockFD;
        struct sockaddr_in socket_addr;
		void CreateBuffer();
		SocketStreamerBase& operator=(const SocketStreamerBase &); 	//assigment op
		SocketStreamerBase(const SocketStreamerBase &);				//copy constructor
};

#endif

