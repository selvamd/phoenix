#include "TcpUtils.hpp"
#include "GlobalUtils.hpp"
#include "ux_selector.hh"
#include "SocketStreamerBase.hh"
#include "ReliableMulticastChannel.hpp"
#include "gtest/gtest.h"
#include <string>

namespace 
{
    struct message
    {
        char name[50];
        int  age;
        bool status;
    };

    class MsgSocketStreamer : public SocketStreamerBase
    {
        public:
            MsgSocketStreamer(int iSocket, int iBufferSize): SocketStreamerBase(iSocket, iBufferSize) { }
            int iCalculateMsgSize() { return sizeof(message); }
    };
    
	class SockTest : public ::testing::Test
	{
        public:
            ux_selector *client;
            ux_selector *server;

		protected:
            SockTest()
            {
                server = new ux_selector(0);
                client = new ux_selector(0);
            }

            virtual ~SockTest() { delete client; delete server; }

			virtual void SetUp()
			{
			}
        
			virtual void TearDown() { }
	};

    int ReadServer(ux_selector *server, bool delimitedMsg)
    {
        static int msgcount = 0;
        int iRC, iServerPort, iClientFD, iClientID;
        char msg[4096];
        
        server->PollForSocketEvent();
        
        while (true)
        {
            iRC = server->Accept(iServerPort, iClientFD);
            if ( iRC == ux_selector::END_OF_SOCK_LIST) break;
            if ( iRC == ux_selector::WOULD_BLOCK) continue;
            //cout << "Connected to port "  << iServerPort << endl;
            if (delimitedMsg)
            {
                SocketStreamerBase * streamer = new SocketStreamerBase(iClientFD, 4096);
                iClientID = server->AddClient(streamer, 4096, 0, false);
            }
            else
            {
                MsgSocketStreamer * streamer = new MsgSocketStreamer(iClientFD, 4096);
                iClientID = server->AddClient(streamer, sizeof(message), 4, false);
            }
        }
        
        while ((iRC = server->Read(iClientID,(char *)&msg)) != ux_selector::END_OF_SOCK_LIST)
        {
            if ( iRC == ux_selector::SUCCESS )
            {
                //cout << "Connected to port "  << (char *)&msg << endl;
                return ++msgcount;
            }
        }
        return msgcount;
    }

	TEST_F(SockTest, TestTcpTransport)
	{
        int iRC, iClientFD, iClientID;
        message msg;
        sprintf(msg.name,"%s", "Selvam");
        
        server->AddServer(63500);
        
        iRC = connectTCP("127.0.0.1", 63500, iClientFD);
        ASSERT_EQ (iRC, 0);

        MsgSocketStreamer * streamer = new MsgSocketStreamer(iClientFD, 4096);
        iClientID = client->AddClient(streamer, sizeof(message), 4, false);
        
        while (ReadServer(server,false) < 100)
        {
            client->Write(iClientID, (char *)&msg, sizeof(message));
            client->flush();
        }
        ASSERT_GE (ReadServer(server, false), 100);
	}

	TEST_F(SockTest, TestTcpTransportDelimitedMsg)
	{
        int iRC, iClientFD, iClientID;
        
        char msg[100];
        sprintf(msg,"%s\n", "Hai how are you");
        
        server->AddServer(62600);
        
        iRC = connectTCP("127.0.0.1", 62600, iClientFD);
        ASSERT_EQ (iRC, 0);

        SocketStreamerBase * streamer = new SocketStreamerBase(iClientFD, 4096);
        iClientID = client->AddClient(streamer, 4096, 0, false);
        
        while (ReadServer(server,true) < 200)
        {
            client->Write(iClientID, (char *)&msg, strlen(msg));
            client->flush();
        }
        ASSERT_GE (ReadServer(server, true), 200);
	}

    TEST_F(SockTest, TestReliableMulticastTransport)
    {
        char streamA = 1, streamB = 2;
        
        //setup multicast publisher for single channel, 2 streams each
        //with gapfill capability
        ReliableMulticastChannel publisherA(streamA), publisherB(streamB);
        publisherA.SetupMulticast("127.0.0.1", "239.255.0.1", 60000);
        publisherA.SetupGapFill(50000);
        publisherB.SetupMulticast("127.0.0.1", "239.255.0.1", 60000);
        publisherB.SetupGapFill(55000);
        
        //setup multicast reader for a single channel with recovery
        //for both streams
        ReliableMulticastChannel reader;
        reader.SetupMulticast("127.0.0.1", "239.255.0.1", 60000);
        reader.AddRecoveryToStream(streamA, "127.0.0.1", 50000);
        reader.AddRecoveryToStream(streamB, "127.0.0.1", 55000);

        std::string strA = "this is streamA";
        std::string strB = "this is streamB";
        
        int readcountA = 0, readcountB = 0;
        MulticastMessage * msgptr;
        for (int i=0;i < 1000; i++)
        {
            if (i < 100) publisherA.publishMsg(strA.c_str(),strA.size());
            if (i < 100) publisherB.publishMsg(strB.c_str(),strB.size());
            publisherA.process();
            publisherB.process();
            reader.process();
            if ( (msgptr = reader.getNextMsgByIndex(streamA)) != nullptr)
            {
                //std::cout << msgptr->fullmsg.buffer << std::endl;
                readcountA++;
            }
            if ( (msgptr = reader.getNextMsgByIndex(streamB)) != nullptr)
            {
                //std::cout << msgptr->fullmsg.buffer << std::endl;
                readcountB++;
            }
        }
        
        //Ensure all 100 msgs are transported
        ASSERT_EQ (readcountA, 100);
        ASSERT_EQ (readcountB, 100);
    }
    
}  // namespace
