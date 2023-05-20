#ifndef _UX_SELECTOR_HH_
#define _UX_SELECTOR_HH_

#include "TcpUtils.hpp"
#include "SocketStreamerBase.hh"
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <netinet/tcp.h>
#include <ctype.h>
#include <poll.h>
#include <cstring>
#include <string>
using namespace std;

struct SocketSession
{
	SocketStreamerBase *Streamer;
	int iReadAttempts;
	short iReadAgain;
	int iMaxMsgSize;
	int iHeaderSize;
	bool SkipNL;
	char * sMsg;
	int iAmountWritten;
	int iWriteMsgLen;
};

class ux_selector
{
	public:
		ux_selector(int PollTimeout=-1); //blocks if -1
		~ux_selector();

        void setPollBlocking(bool stat) { iPollTimeout = (stat)? -1:0; }
    
		int AddServer(int ServerPort);

		//Returns clientID 		
		int AddClient(SocketStreamerBase *Streamer, int iMaxMsgSize, int iHeaderSize, bool SkipNL);

		void RemoveClient(int iClient);

		int PollForSocketEvent();

		//Writes are non-blocking (return WOULD_BLOCK after max attempts and store the unwritten msg)
		//During next request, if we still get blocking errors, it will disconnect. 
		//Writes are buffered to maximize the TCP payload.
		int Write(int iClientID, char * Msg, int MsgLen); 

        int Publish(int iClientID, char * Msg, int MsgLen);
    
		//call flush to push out the buffered data. 
		//Use when there is no additional I/O due to inactivity
		void flush(); 
		
		//Returns Error code shown below  		
		int Read(int &iClientID, char * Msg); 

		//Returns Error code shown below  		
		int Accept(int &iServerPort, int &iClientFD); 

		static const int SUCCESS     	  =  0;		
		static const int FAILURE     	  = -1;		
		static const int DISCONNECT  	  = -2;		
		static const int WOULD_BLOCK 	  = -3;		
		static const int END_OF_SOCK_LIST = -4;		
		
	private:
		ux_selector& operator=(const ux_selector &); 	//assigment op
		ux_selector(const ux_selector &);		//copy constructor

		void ConfigureSocketOptions(int iSessionFD);
		int WriteInternal(int iClientID); 

		//Max values
		static const int  iMaxAccept = 20;
		static const int  iMaxClient = 1000;

		static const int  iInputReadBuffer = -1;
		static const int  iOutputeadBuffer = -1;
		static const bool bNagleOff = true;

		//Maximum transmission unit for Ethernet is 1500. 
		//Set under 1400 after accounting for overheads (IP+UDP headers)
		//to avoid packet fragmentation. Default is no buffering
		static const int  WRITE_BUFF_SIZE = 0;

		//These 2 values cause disconnects as the load on socket grows. 
		static const int  iMaxReadAttempts = 3;
		static const int  iMaxWriteAttepts = 3;

		int iAcceptSocketFD[iMaxAccept];
		int iAcceptPort[iMaxAccept];

		struct pollfd Connections[iMaxClient+iMaxAccept];
		struct SocketSession Sessions[iMaxClient+iMaxAccept];
		int iMaxPollFD;     
        
		int iPollTimeout; 
		
		//Transient
		int iNextAcceptPollIdx;
		int iNextReadPollIdx;
		int iClientsWithDataCount;	
};

#endif

