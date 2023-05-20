#ifndef _RELIABLE_MULTICAST_CHANNEL_HPP_
#define _RELIABLE_MULTICAST_CHANNEL_HPP_

#include <cstdint>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "Containers.hpp"
#include "DataTypes.hpp"
#include "ux_selector.hh"
#include "TcpUtils.hpp"
#include "Logger.hpp"
#include <algorithm>

//genmsg msg types
const char HEARTBEAT    = 1;
const char GAPFILL      = 2;
const char TRANSMITOK   = 3;
const char FILLER       = 4;
const char APPLICATION  = 5;

struct MulticastGenMsg
{
    char    MsgLength[6];
    char    MsgType;
    char    MsgStream;
    char    MsgCount;
    char    MsgSequence[20];
    char    MsgSender[10];
    
    void init(char stream, char type, uint64_t seq = 0)
    {
        memset(this,0,sizeof(*this));
        sprintf(MsgLength,"%d",(int)sizeof(*this));
        sprintf(MsgSequence,"%llu",seq);
        MsgStream = stream;
        MsgType = type;
    }
};

struct MulticastFullMsg
{
    struct MulticastGenMsg  genmsg;
    char    buffer[1450];
};

union MulticastMessage
{
    struct MulticastFullMsg fullmsg;
    struct MulticastGenMsg  genmsg;
};

class RMCSocketStreamer : public SocketStreamerBase
{
    public:
        RMCSocketStreamer(int iSocket, int iBufferSize): SocketStreamerBase(iSocket, iBufferSize) { }
        int iCalculateMsgSize()
        {
            sMsgBuffer[iPos+5] = 0; //make sure last char is a null terminator
            return atoi(sMsgBuffer);
        }
};

struct ClientStreamData
{
    char        streamid;
    int32_t     clientID;
    uint64_t    gap_fill_index;
    uint64_t    gap_fill_count;
    uint64_t    archive_ok_index;
    Timestamp   last_event_time;
};

struct StreamData
{
    char        streamid;
    std::string recoveryhost;
    int32_t     recoveryport;
    int32_t     recoveryClientID;
    Timestamp   last_event_time;
    uint64_t    max_nogap_sequence;
    MsgBuffer<MulticastMessage> * buffer;
    uint64_t    local_read_index;
    std::map<int32_t,ClientStreamData> clients;
    //used for making gap fill requests
    Timestamp   last_gapfill_time;
    uint64_t    last_gapfill_sequence;
};

//Class for reliable multicasting. Can be used to setup as publish node or subscriber node
//Notes:
//  A multicast channel can have multiple streams (identified by streamid)
//  A Publisher node can publish data for 1 stream and configured to provide gapfills for the same
//  Subscriber nodes can read from 1-to-n streams of a single multicast channel.
//  Subscriber can be configured do recovery either from publisher or a relay node (a peer subscriber)
//  Subscriber nodes can provide msgs to local consumers on arrival (unreliable) or after resequencing (reliable)
//  Subscriber nodes set with gapfill can act as relay to provide recovery to other peer subscribers
//  Data outside the buffered range is non-recoverable from this framework (not guaranteed).
class ReliableMulticastChannel
{
    public:
        //Provide streamid for publisher nodes
        ReliableMulticastChannel(char stream = 0) : selector(0), publish_stream_id(stream), gapfillacceptport(0)  {}
        ~ReliableMulticastChannel() { }

        void SetupGapFill(int32_t gapfillacceptport_);
        void SetupMulticast(string local_addr, string mcast_addr, int mcastport );
        void AddRecoveryToStream(char stream, std::string recovery_host, int32_t recovery_port);
        void process();
        bool publishMsg(const char * sMsg, int32_t len);
        void setReadIndex(char stream, uint64_t seq);
        MulticastMessage * getNextMsgByIndex(char stream);
        MulticastMessage * getNextMsgAsap(char stream);
    
    private:
    
        bool processStreamEvent(MulticastMessage &msg, struct StreamData & data, Timestamp &now);
        bool processClientEvent(MulticastMessage &msg, struct StreamData & data, int32_t clientID);
    
        ux_selector selector;
        std::string mcast_address;
        int32_t     mcast_port;
        std::string local_interface;
        int32_t multicast_clientid;
    
        std::map<char,struct StreamData> data_streams;
        const char publish_stream_id;
        int32_t     gapfillacceptport; //used by gapfiller only
};

#endif
