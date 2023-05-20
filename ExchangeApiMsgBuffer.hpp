#ifndef _EXCHNAGE_API_MSG_BUFFER_HPP_
#define _EXCHNAGE_API_MSG_BUFFER_HPP_

#include <string>
#include <map>
#include <vector>

#include "ExchangeApi.hpp"
#include "MsgBuffer.hpp"


struct ExchangeApiMsgBufferProperties
{
    int MAX_STREAMS                     =    10;
    int MAX_SESSIONS                    =   100;
    
    int SESSION_REQUEST_BUFFER_SIZE     =   10;
    int SESSION_RESPONSE_BUFFER_SIZE    =   10;
    
    int STREAM_REQUEST_BUFFER_SIZE      = 1000; //Always set to SESSION_REQUEST_BUFFER_SIZE * MAX_SESSIONS
    int STREAM_RESPONSE_BUFFER_SIZE     = 1000;

    int STREAM_MSGS_IN_FLIGHT           =    5;
    
};

struct SessionData
{
    int     index;
    std::string  name;
    
    int64_t last_received_client_request_sequence;      //updated on addExchangeRequest
    int64_t last_transmitted_client_response_sequence;  //updated on getNextResponseMsgForSession
    
    std::vector<MsgBuffer<exchange_api::ExchangeApiUnion> *> requests;
    MsgBuffer<std::pair<int,uint64_t>> response_transmit_sequence;
};

struct StreamData
{
    int     index;
    std::string  name;

    int64_t last_transmitted_request_sequence;  //updated on getNextRequestMsg
    int64_t last_received_response_sequence;    //updated on addExchangeResponse
    int64_t last_transmitted_response_sequence; //updated on getNextResponseMsg
    int64_t last_archived_response_sequence;    //updated on ArchiveOk
    
    //on crash restart make sure to query archivers and
    //reset the buffer to LastArchiveSequence.
    MsgBuffer<exchange_api::ExchangeApiUnion> responses;
    MsgBuffer<std::pair<int,uint64_t>> request_transmit_sequence;
};

class ExchangeApiMsgBuffer
{
    public:
    
        ExchangeApiMsgBuffer(const struct ExchangeApiMsgBufferProperties & properties);

        ~ExchangeApiMsgBuffer();
    
        int getSessionIndex(const std::string& session);
        int getStreamIndex(const std::string& stream);
        
        int32_t addExchangeRequestMsg(int session, int stream, exchange_api::ExchangeApiUnion * msg,
                                      int64_t& stream_seq);
        
        int32_t addExchangeResponseMsg(int Session, int stream, exchange_api::ExchangeApiUnion * msg,
                                       int64_t& stream_seq, int64_t& session_seq);
        
        //Process ArchiveOk - flush output buffer and OutSequenceMap
        bool updateStreamArchiveComplete(int stream, int64_t ArchivedSequence);
        
        //Get last message to transmit
        exchange_api::ExchangeApiUnion * getNextRequestMsg(int stream);
        
        //Get last message to transmit
        exchange_api::ExchangeApiUnion * getNextResponseMsg(int stream);
        
        //Process ClientResponseResend
        exchange_api::ExchangeApiUnion * getNextResponseMsgForSession(int session);

        //Rewinds transmit sequences causing messages to replay
        bool setLastTransmittedRequestSequence(int stream, int64_t sequence);
        bool setLastTransmittedResponseSequence(int stream, int64_t sequence);
        bool setLastTransmittedClientResponseSequence(int session, int64_t sequence);

        //Get last received sequence numbers
        int64_t getLastReceivedClientRequestSequence(int session);
        int64_t getLastReceivedResponseSequence(int stream);

        //matcher doesnot buffer request messages and therefore will just
        //call this method to update the sequence number only.
        bool setLastReceivedClientRequestSequence(int session, int64_t sequence);
    
    private:
        bool requestAck(exchange_api::ExchangeApiUnion * msg);
        bool matchmsg(exchange_api::ExchangeApiUnion * request,exchange_api::ExchangeApiUnion * response);

        struct ExchangeApiMsgBufferProperties properties_;
        std::vector<SessionData *> sessions_;
        std::vector<StreamData *>  streams_;
};

#endif



