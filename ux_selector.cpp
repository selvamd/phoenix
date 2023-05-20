#include "ux_selector.hh"
#include <unistd.h>

ux_selector::ux_selector(int PollTimeout)
{
    iPollTimeout = PollTimeout;

	iMaxPollFD = iNextAcceptPollIdx = iNextReadPollIdx = iClientsWithDataCount = 0;

	for (int client=0;client<iMaxAccept;client++)
		iAcceptPort[client] = iAcceptSocketFD[client] = 0;

	for (int client=0;client<iMaxAccept+iMaxClient;client++)
	{
		Sessions[client].Streamer = NULL;
		Sessions[client].iReadAttempts = 0;
		Sessions[client].iMaxMsgSize = 0;
		Sessions[client].iHeaderSize = 0;
		Sessions[client].SkipNL = false;
		Sessions[client].iAmountWritten = 0;
		Sessions[client].iWriteMsgLen = 0;
		Sessions[client].iReadAgain = 1;
		Connections[client].fd = -1;
	}
}

ux_selector::~ux_selector()
{
	for (int ii=iMaxAccept;ii<iMaxClient;ii++)
		RemoveClient(ii);
}

int ux_selector::AddServer(int iServerPort)
{
	int iRC = 0, iServerSocket;
	for (int ii = 0; ii < iMaxAccept; ii++)
	{
		if ( iAcceptPort[ii] <= 0 )
		{
			if ((iRC = InitializeAcceptSocketTCP("127.0.0.1", iServerPort, iServerSocket)) < 0)
			{
                //std::cout << iRC << std::endl;
				return iRC;
			}

			ConfigureSocketOptions(iServerSocket);

			iAcceptPort[ii] = iServerPort;
			iAcceptSocketFD[ii] = iServerSocket;
			Connections[ii].fd = iServerSocket;
			Connections[ii].events = POLLRDNORM;
			if (ii > iMaxPollFD)
				iMaxPollFD = ii;
			return iRC;
		}
	}
    return iRC;
}

int ux_selector::AddClient(SocketStreamerBase *Streamer, int iMaxMsgSize, int iHeaderSize, bool SkipNL)
{
	int iSessionFD = Streamer->iGetSocket();
	ConfigureSocketOptions(iSessionFD);

	for (int ii = iMaxAccept; ii < iMaxClient+iMaxAccept; ii++)
	{
		if (Connections[ii].fd == -1)
		{
			Connections[ii].fd 			= iSessionFD;
			Connections[ii].events 		= POLLRDNORM;
			Sessions[ii].Streamer 		= Streamer;
			Sessions[ii].iReadAttempts 	= 0;
			Sessions[ii].iReadAgain 	= 0;
			Sessions[ii].iMaxMsgSize 	= iMaxMsgSize;
			Sessions[ii].iHeaderSize 	= iHeaderSize;
			Sessions[ii].SkipNL 		= SkipNL;
			Sessions[ii].iAmountWritten = 0;
			Sessions[ii].iWriteMsgLen 	= 0;
			Sessions[ii].sMsg 			= (char *) malloc(sizeof(char) * iMaxMsgSize);
			//touch the memory to make sure allocation is complete
			memset(Sessions[ii].sMsg, 0, iMaxMsgSize);
			if (ii > iMaxPollFD)
				iMaxPollFD = ii;
			return ii;
		}
	}
	return -1;
}

void ux_selector::RemoveClient(int client)
{
	if ( client < iMaxAccept || client > iMaxPollFD || Connections[client].fd == -1 ) return;

	disconnectTCP(Connections[client].fd);
	delete Sessions[client].Streamer;
	free (Sessions[client].sMsg);

	Sessions[client].iReadAttempts 	= 0;
	Sessions[client].iMaxMsgSize   	= 0;
	Sessions[client].iHeaderSize 	= 0;
	Sessions[client].SkipNL 		= false;
	Sessions[client].iAmountWritten = 0;
	Sessions[client].iWriteMsgLen 	= 0;
	Sessions[client].iReadAgain 	= 1;
	Sessions[client].SkipNL 		= false;
	Sessions[client].Streamer       = NULL;
	Connections[client].fd 			= -1;
}

int ux_selector::PollForSocketEvent()
{
	int iRC=0;

	while (true)
	{
		if (iClientsWithDataCount == 0)
		{
			if ((iRC = poll(Connections, iMaxPollFD + 1, iPollTimeout)) < 0)
			{
                sleep(1);
				continue;
			}
		}
		else
		{
			if ((iRC = poll(Connections, iMaxPollFD + 1, 0)) < 0)
			{
                sleep(1);
				continue;
			}
		}
		break;
	}
	iNextAcceptPollIdx = 0;
	iClientsWithDataCount = 0;
	iNextReadPollIdx = iMaxAccept;

    return iRC;
}

int ux_selector::Read(int &iClientID, char * Msg)
{
	int iRC;
	iClientID = -1;
	for (int ii = iNextReadPollIdx; ii < iMaxPollFD+1; ii++)
	{
		iNextReadPollIdx++;

		if (Connections[ii].fd == -1) continue;

		if (Connections[ii].revents & (POLLRDNORM | POLLERR) /*|| Sessions[ii].iReadAgain > 0*/)
		{
			iClientID = ii;
			memset(Msg, '\0', Sessions[ii].iMaxMsgSize);
			if ((iRC = Sessions[ii].Streamer->iReadNonBlockingN(Msg, Sessions[ii].iMaxMsgSize, Sessions[ii].iHeaderSize, Sessions[ii].SkipNL)) < 0)
			{
				if ( iRC != SocketStreamerBase::ERROR_EAGAIN || Sessions[ii].iReadAttempts++ >= iMaxReadAttempts )
				{
					cout << "Read error : " << iRC << " Attempts: " << Sessions[ii].iReadAttempts << endl;
					RemoveClient(iClientID);
					return DISCONNECT;
				}

				return FAILURE;
			}
			else
			{
				++iClientsWithDataCount;
				Sessions[ii].iReadAttempts = 0;
				//Sessions[ii].iReadAgain = 1;
				return SUCCESS;
			}
		}
	}

	//Read for more unread msgs in socketstreamer
	for (int ii = iMaxAccept; ii < iMaxPollFD+1; ii++)
	{
		if (Connections[ii].fd == -1) continue;
		if (Connections[ii].revents & (POLLRDNORM | POLLERR) &&
                Sessions[ii].Streamer->hasUnreadMsg(Sessions[ii].iHeaderSize))
		{
			iNextReadPollIdx = ii;
			return Read(iClientID, Msg);
		}
	}

	return END_OF_SOCK_LIST;
}

int ux_selector::Accept(int &iServerPort, int &iClientFD)
{
	int iRC, iNewSocket;
    sockaddr_in client;
	iServerPort = iClientFD = -1;
	for (int i = iNextAcceptPollIdx; i < iMaxAccept; i++)
	{
		iNextAcceptPollIdx++;
		if (iAcceptSocketFD[i] > 0 && Connections[i].revents & POLLRDNORM)
		{
			iServerPort = iAcceptPort[i];
			if ((iRC = acceptConnectionTCP(iAcceptSocketFD[i], iNewSocket, client)) < 0)
			{
				if (iRC == ACCEPT_TCP_UTILS_ERROR)
					return WOULD_BLOCK;
			}
			else
			{
				iClientFD = iNewSocket;
				return SUCCESS;
			}
		}
	}
	return END_OF_SOCK_LIST;
}

void ux_selector::ConfigureSocketOptions( int iSockFD )
{
	int iRC;
	if ( iSockFD < 0 ) return;

	int iBuff = iInputReadBuffer;
	if (iBuff > 0)
	{
		if ((iRC = setsockopt(iSockFD, SOL_SOCKET, SO_RCVBUF, (void *)&iBuff, sizeof(iBuff))) >= 0)
		{
            int sockbufsize = 0;
            int size = sizeof(int);
            getsockopt(iSockFD, SOL_SOCKET, SO_RCVBUF, (char *)&sockbufsize, (socklen_t*)&size);
		}
	}

	iBuff = iOutputeadBuffer;
	if (iBuff > 0)
	{
		if ((iRC = setsockopt(iSockFD, SOL_SOCKET, SO_SNDBUF, (void *)&iBuff, sizeof(iBuff))) >= 0)
		{
            int sockbufsize = 0;
            int size = sizeof(int);
            getsockopt(iSockFD, SOL_SOCKET, SO_SNDBUF, (char *)&sockbufsize, (socklen_t*)&size);
		}
	}

    //Turn nagle off for socket if flag set
	if (bNagleOff)
	{
		int iNagleOpt = 1;
		setsockopt(iSockFD, IPPROTO_TCP, TCP_NODELAY, (void *)&iNagleOpt, sizeof(iNagleOpt));
	}

    //Set nonblocking
	if (makeSocketNonBlocking(iSockFD) < 0)
		cout << "Non blocking error " << iSockFD << endl;
}

void ux_selector::flush()
{
	for (int ii = iMaxAccept; ii < iMaxPollFD+1; ii++)
	{
		if (Connections[ii].fd == -1) continue;
		if ( Sessions[ii].iWriteMsgLen - Sessions[ii].iAmountWritten > 0 )
		{
			WriteInternal(ii);
		}
	}
}

int ux_selector::Publish(int iClientID, char * Msg, int MsgLen)
{
    int iRC = 0;
    if ( iClientID < iMaxAccept || iClientID > iMaxPollFD || Connections[iClientID].fd == -1 ) return  END_OF_SOCK_LIST;
    iRC = Sessions[iClientID].Streamer->iPublish(Msg,MsgLen);
    return iRC;
}

int ux_selector::Write(int iClientID, char * Msg, int MsgLen)
{
	int iRC = 0;
	if ( iClientID < iMaxAccept || iClientID > iMaxPollFD || Connections[iClientID].fd == -1 ) return  END_OF_SOCK_LIST;

	//Flush partial messages
	if ( Sessions[iClientID].iAmountWritten > 0 )
	{
		iRC = WriteInternal(iClientID);
		if (iRC != SUCCESS)
		{
			RemoveClient(iClientID);
			return DISCONNECT;
		}
	}

	//No parial messages. But there may be buffered messages.
	//Flush if buffer size exceeded.
	if ( Sessions[iClientID].iWriteMsgLen + MsgLen > WRITE_BUFF_SIZE )
	{
		if ( Sessions[iClientID].iWriteMsgLen > 0 )
		{
			iRC = WriteInternal(iClientID);
			if (iRC != SUCCESS)
			{
				RemoveClient(iClientID);
				return DISCONNECT;
			}
		}
	}

	memcpy( &Sessions[iClientID].sMsg[Sessions[iClientID].iWriteMsgLen], Msg, MsgLen);
	Sessions[iClientID].iWriteMsgLen += MsgLen;

	//Flush new message by itself if bigger than the buffer.
	if ( Sessions[iClientID].iWriteMsgLen > WRITE_BUFF_SIZE )
	{
		iRC = WriteInternal(iClientID);
		if (iRC != SUCCESS)
		{
			RemoveClient(iClientID);
			return DISCONNECT;
		}
	}

	return SUCCESS;
}

int ux_selector::WriteInternal(int iClientID)
{
	int iWriteAttempts = 0, iAmountWritten = 0, iRC = 0;
	while (1)
	{
		iRC = Sessions[iClientID].Streamer->iWriteNonBlockingN(
			&Sessions[iClientID].sMsg[Sessions[iClientID].iAmountWritten],
			Sessions[iClientID].iWriteMsgLen - Sessions[iClientID].iAmountWritten,
			iAmountWritten);

		if (iRC < 0)
		{
			if (iRC != SocketStreamerBase::ERROR_EWOULDBLOCK && iRC != SocketStreamerBase::ERROR_EAGAIN )
			{
				cout << "iRC :" << iRC << endl;
				RemoveClient(iClientID);
				return DISCONNECT;
			}
			else
			{
				//cout << "iWriteAttempts :" << iWriteAttempts << endl;
				Sessions[iClientID].iAmountWritten += iAmountWritten;
				if ( ++iWriteAttempts >= iMaxWriteAttepts )
					return WOULD_BLOCK;
			}
		}
		else
		{
			Sessions[iClientID].iWriteMsgLen   = 0;
			Sessions[iClientID].iAmountWritten = 0;
			return SUCCESS;
		}
	}
}

