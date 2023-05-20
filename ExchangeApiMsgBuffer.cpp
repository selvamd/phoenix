#include <string>
#include <map>
#include <vector>
#include <utility>

#include "ExchangeApiMsgBuffer.hpp"


ExchangeApiMsgBuffer::ExchangeApiMsgBuffer(const struct ExchangeApiMsgBufferProperties & prop):
                    properties_(prop)
{
}

ExchangeApiMsgBuffer::~ExchangeApiMsgBuffer()
{
    for (auto it = streams_.begin(); it != streams_.end();  )
    {
        struct StreamData * stream = *it;
        it = streams_.erase(it);
        delete stream;
    }

    for (auto it = sessions_.begin(); it != sessions_.end(); ++it)
    {
        struct SessionData * session = *it;
        for (auto itr = session->requests.begin(); itr != session->requests.end();  )
        {
            MsgBuffer<exchange_api::ExchangeApiUnion> * buffer = *itr;
            itr = session->requests.erase(itr);
            delete buffer;
        }
    }

/*
    for (auto it = sessions_.begin(); it != sessions_.end();  )
    {
        struct SessionData * session = *it;
        it = sessions_.erase(it);
        delete session;
    }
 */
}

int ExchangeApiMsgBuffer::getSessionIndex(const std::string& sessionname)
{
    if ( sessionname.length() == 0 )
        return -1;
    
    struct SessionData * session;
    
    for (auto it = sessions_.begin(); it != sessions_.end(); ++it )
    {
        session = *it;
        if (session->name == sessionname)
            return session->index;
    }
    
    session = new SessionData;
    session->index = (int)sessions_.size();
    session->name = sessionname;
    session->last_transmitted_client_response_sequence = -1;
    session->last_received_client_request_sequence = -1;
    sessions_.push_back(session);

    for (auto it = streams_.begin(); it != streams_.end(); ++it )
    {
        MsgBuffer<exchange_api::ExchangeApiUnion> * buffer = new MsgBuffer<exchange_api::ExchangeApiUnion>();
        buffer->setCapacity(properties_.SESSION_REQUEST_BUFFER_SIZE);
        session->requests.push_back(buffer);
    }
    
    session->response_transmit_sequence.setCapacity(properties_.SESSION_RESPONSE_BUFFER_SIZE);

    return session->index;
}

int ExchangeApiMsgBuffer::getStreamIndex(const std::string& streamname)
{
    if ( streamname.length() == 0 )
        return -1;
    
    struct StreamData * stream;

    for (auto it = streams_.begin(); it != streams_.end(); ++it )
    {
        stream = *it;
        if (stream->name == streamname)
            return stream->index;
    }
    
    stream = new StreamData();
    stream->index = (int)streams_.size();
    stream->name = streamname;
    stream->last_transmitted_request_sequence = -1;
    stream->last_received_response_sequence = -1;
    stream->last_transmitted_response_sequence = -1;
    stream->last_archived_response_sequence = -1;

    streams_.push_back(stream);
    
    for (auto it = sessions_.begin(); it != sessions_.end(); ++it )
    {
        struct SessionData * session = *it;
        MsgBuffer<exchange_api::ExchangeApiUnion> * buffer = new MsgBuffer<exchange_api::ExchangeApiUnion>();
        buffer->setCapacity(properties_.SESSION_REQUEST_BUFFER_SIZE);
        session->requests.push_back(buffer);
    }

    stream->responses.setCapacity(properties_.STREAM_RESPONSE_BUFFER_SIZE);
    stream->request_transmit_sequence.setCapacity(properties_.STREAM_REQUEST_BUFFER_SIZE);

    return stream->index;
}

//Returns MatcherInputTransmitSequence  or -1 for error
int32_t ExchangeApiMsgBuffer::addExchangeRequestMsg(int session, int stream, exchange_api::ExchangeApiUnion * msg, int64_t& matcher_sequence)
{
    matcher_sequence = 0;
    
    if ( stream < 0 || stream >= (int)streams_.size())
        return -1;

    if ( session < 0 || session >= (int)sessions_.size())
        return -1;
    
    if ( sessions_[session]->requests[stream]->size() < sessions_[session]->requests[stream]->capacity() )
    {
        if ( streams_[stream]->request_transmit_sequence.size() < streams_[stream]->request_transmit_sequence.capacity() )
        {
            sessions_[session]->last_received_client_request_sequence = msg->gen_msg.getSeqNum();
            uint64_t clientsequence = sessions_[session]->requests[stream]->insert(msg);
            auto rec = std::make_pair(session,clientsequence);
            matcher_sequence = streams_[stream]->request_transmit_sequence.insert(&rec);
            return 0;
        }
    } 
    
    return -1;
}

//Returns ArchiveSequence  or -1 for error - Flush transmitBuffer and client input buffer
int32_t ExchangeApiMsgBuffer::addExchangeResponseMsg(int session, int stream, exchange_api::ExchangeApiUnion * msg,
                                                     int64_t& archive_seq, int64_t& session_seq)
{
    session_seq = 0;
    archive_seq = 0;

    if ( stream < 0 || stream >= (int)streams_.size())
        return -1;
    
    //clear the first responses if there is no room in the buffer
    if ( streams_[stream]->responses.size() == streams_[stream]->responses.capacity() )
        streams_[stream]->responses.flush(streams_[stream]->responses.firstSequence());

    //Add response message to stream and session
    streams_[stream]->last_received_response_sequence = msg->gen_msg.getSeqNum();
    archive_seq = streams_[stream]->responses.insert(msg);

    if ( session < 0 || session >= (int)sessions_.size())
        return -1;
    
    if ( sessions_[session]->response_transmit_sequence.size() == sessions_[session]->response_transmit_sequence.capacity() )
        sessions_[session]->response_transmit_sequence.flush(sessions_[session]->response_transmit_sequence.firstSequence());
    
    auto rec = std::make_pair(stream, (uint64_t)archive_seq);
    session_seq = sessions_[session]->response_transmit_sequence.insert(&rec);

    if ( !requestAck(msg) ) return 0;
    
    //match request to response
    for (uint64_t seq = streams_[stream]->request_transmit_sequence.firstSequence();
         seq < streams_[stream]->request_transmit_sequence.nextAvailableSequence(); seq++ )
    {
        exchange_api::ExchangeApiUnion * request;
        std::pair<int,uint64_t> * xref;
        
        xref = streams_[stream]->request_transmit_sequence.get(seq);
        if ( xref == nullptr ) return 0;
        
        request = sessions_[xref->first]->requests[stream]->get(xref->second);
        if (request == nullptr) return 0;
        
        if ( matchmsg(request,msg) )
        {
            sessions_[xref->first]->requests[stream]->flush(xref->second);
            streams_[stream]->request_transmit_sequence.flush(seq);
            return 0;
        }
    }
    
    return 0;
}

bool ExchangeApiMsgBuffer::requestAck(exchange_api::ExchangeApiUnion * msg)
{
    if ( msg->gen_msg.getMsgType() == exchange::ExchangeApiMsgType_t::cancel_reject )
        return true;

    if ( msg->gen_msg.getMsgType() == exchange::ExchangeApiMsgType_t::sod_response )
        return true;
    
    if ( msg->gen_msg.getMsgType() == exchange::ExchangeApiMsgType_t::trading_status_update_response )
        return true;

    if ( msg->gen_msg.getMsgType() != exchange::ExchangeApiMsgType_t::exec_report )
        return false;

    
    return ( msg->exec_report_msg.getOrdStatus() ==  exchange::OrdStatus_t::NEW ||
            msg->exec_report_msg.getOrdStatus() ==  exchange::OrdStatus_t::REJECTED ||
            msg->exec_report_msg.getOrdStatus() ==  exchange::OrdStatus_t::CANCELED);
}

bool ExchangeApiMsgBuffer::matchmsg(exchange_api::ExchangeApiUnion * request,exchange_api::ExchangeApiUnion * response)
{

    if ( response->gen_msg.getMsgType() == exchange::ExchangeApiMsgType_t::sod_response )
    {
        if ( request->gen_msg.getMsgType() != exchange::ExchangeApiMsgType_t::sod_request )
            return false;
        
        if ( strncmp( request->cancel_msg.getOnBehalfOfCompId(), response->cancel_reject.getDeliverToCompId(), exchange::COMP_ID_LENGTH) != 0 )
            return false;
        
        return true;
    }

    if ( response->gen_msg.getMsgType() == exchange::ExchangeApiMsgType_t::trading_status_update_response )
    {
        if ( request->gen_msg.getMsgType() != exchange::ExchangeApiMsgType_t::trading_status_update_request )
            return false;
        
        if ( request->trading_status_update_request.getFirmType() != request->trading_status_update_response.getFirmType() )
            return false;

        if ( request->trading_status_update_request.getFirmTradingStatus() != request->trading_status_update_response.getFirmTradingStatus() )
            return false;
        
        if ( request->trading_status_update_request.getCancelType() != request->trading_status_update_response.getCancelType() )
            return false;

        if ( strncmp( request->trading_status_update_request.getOnBehalfOfCompId(), response->trading_status_update_response.getDeliverToCompId(), exchange::COMP_ID_LENGTH) != 0 )
            return false;
        
        return true;
    }
    
    if ( response->gen_msg.getMsgType() == exchange::ExchangeApiMsgType_t::cancel_reject )
    {
         if ( request->gen_msg.getMsgType() != exchange::ExchangeApiMsgType_t::cancel )
             return false;

        if ( strncmp( request->cancel_msg.getOnBehalfOfCompId(), response->cancel_reject.getDeliverToCompId(), exchange::COMP_ID_LENGTH) != 0 )
            return false;

        if ( request->cancel_msg.getClOrdId() != response->cancel_reject.getClOrdId() )
            return false;
        
        return true;
    }
        
    if ( response->exec_report_msg.getOrdStatus() == exchange::OrdStatus_t::CANCELED )
    {
        if ( request->gen_msg.getMsgType() != exchange::ExchangeApiMsgType_t::new_order )
            return false;
        
        if ( strncmp( request->cancel_msg.getOnBehalfOfCompId(), response->exec_report_msg.getDeliverToCompId(), exchange::COMP_ID_LENGTH) != 0 )
            return false;
        
        if ( request->cancel_msg.getClOrdId() != response->exec_report_msg.getClOrdId() )
            return false;
        
        return true;
    }
    
    if ( strncmp( request->new_order_msg.getOnBehalfOfCompId(), response->exec_report_msg.getDeliverToCompId(), exchange::COMP_ID_LENGTH) != 0 )
        return false;
    
    if ( request->new_order_msg.getClOrdId() != response->exec_report_msg.getClOrdId() )
        return false;
    
    return true;
}

//Process ArchiveOk - flush output buffer and OutSequenceMap
bool ExchangeApiMsgBuffer::updateStreamArchiveComplete(int stream, int64_t ArchivedSequence)
{
    if ( stream < 0 || stream >= (int)streams_.size())
        return false;
    
    if ( streams_[stream]->responses.get(ArchivedSequence) == nullptr)
        return false;
    
    if ( streams_[stream]->last_archived_response_sequence >=  ArchivedSequence)
        return false;
    
    streams_[stream]->last_archived_response_sequence = ArchivedSequence;
    return true;
}

//Get last message to transmit
exchange_api::ExchangeApiUnion * ExchangeApiMsgBuffer::getNextRequestMsg(int stream)
{
    exchange_api::ExchangeApiUnion * request;
    std::pair<int,uint64_t> * xref;
    
    if ( stream < 0 || stream >= (int)streams_.size())
        return nullptr;
    
    xref = streams_[stream]->request_transmit_sequence.get(streams_[stream]->last_transmitted_request_sequence+1);
    if (xref == nullptr)
        return nullptr;

    /*
    //throttle messages inflight to matcher based on count in transmit buffer
    int inflightcount = 1 + streams_[stream]->last_transmitted_request_sequence - streams_[stream]->request_transmit_sequence.firstSequence();
    if ( inflightcount >= properties_.STREAM_MSGS_IN_FLIGHT )
    {
        return nullptr;
    }
    */

    request = sessions_[xref->first]->requests[stream]->get(xref->second);
    if ( request != nullptr)
    {
        streams_[stream]->last_transmitted_request_sequence++;
        request->gen_msg.setSeqNum(streams_[stream]->last_transmitted_request_sequence);
    }

    return request;
}

//Get last message to transmit
exchange_api::ExchangeApiUnion * ExchangeApiMsgBuffer::getNextResponseMsg(int stream)
{
    exchange_api::ExchangeApiUnion * response;
    if ( stream < 0 || stream >= (int)streams_.size())
        return nullptr;
    
    response = streams_[stream]->responses.get(streams_[stream]->last_transmitted_response_sequence+1);
    if ( response != nullptr)
    {
        streams_[stream]->last_transmitted_response_sequence++;
        response->gen_msg.setSeqNum(streams_[stream]->last_transmitted_response_sequence);
    }
    
    return response;
}

//Process ClientResponseResend
exchange_api::ExchangeApiUnion * ExchangeApiMsgBuffer::getNextResponseMsgForSession(int session)
{
    exchange_api::ExchangeApiUnion * response;
    std::pair<int,uint64_t> * xref;
    
    if ( session < 0 || session >= (int)sessions_.size())
        return nullptr;
    
    xref = sessions_[session]->response_transmit_sequence.get(sessions_[session]->last_transmitted_client_response_sequence+1);
    if (xref == nullptr)
        return nullptr;
    
    response = streams_[xref->first]->responses.get(xref->second);
    if ( response != nullptr)
    {
        sessions_[session]->last_transmitted_client_response_sequence++;
        response->gen_msg.setSeqNum(sessions_[session]->last_transmitted_client_response_sequence);
    }
    
    return response;
}

//Rewinds transmit sequences causing messages to replay
bool ExchangeApiMsgBuffer::setLastTransmittedRequestSequence(int stream, int64_t sequence)
{
    exchange_api::ExchangeApiUnion * request;
    std::pair<int,uint64_t> * xref;
    
    if ( stream < 0 || stream >= (int)streams_.size())
        return false;
    
    xref = streams_[stream]->request_transmit_sequence.get(sequence+1);
    if (xref == nullptr)
        return false;
    
    request = sessions_[xref->first]->requests[stream]->get(xref->second);
    if (request == nullptr)
        return false;
    
    streams_[stream]->last_transmitted_request_sequence = sequence;
    return true;
}

bool ExchangeApiMsgBuffer::setLastTransmittedResponseSequence(int stream, int64_t sequence)
{
    exchange_api::ExchangeApiUnion * response;
    if ( stream < 0 || stream >= (int)streams_.size())
        return false;
    
    response = streams_[stream]->responses.get(sequence+1);
    if ( response == nullptr)
        return false;
    
    streams_[stream]->last_transmitted_response_sequence = sequence;
    return true;
}

bool ExchangeApiMsgBuffer::setLastTransmittedClientResponseSequence(int session, int64_t sequence)
{
    exchange_api::ExchangeApiUnion * response;
    std::pair<int,uint64_t> * xref;
    
    if ( session < 0 || session >= (int)sessions_.size())
        return false;
    
    xref = sessions_[session]->response_transmit_sequence.get(sequence+1);
    if (xref == nullptr)
        return false;
    
    response = streams_[xref->first]->responses.get(xref->second);
    if (response == nullptr)
        return false;
    
    sessions_[session]->last_transmitted_client_response_sequence = sequence;
    return true;
}

//Get last received sequence numbers
int64_t ExchangeApiMsgBuffer::getLastReceivedClientRequestSequence(int session)
{
    if ( session < 0 || session >= (int)sessions_.size())
        return -1;
    return sessions_[session]->last_received_client_request_sequence;
}

int64_t ExchangeApiMsgBuffer::getLastReceivedResponseSequence(int stream)
{
    if ( stream < 0 || stream >= (int)streams_.size())
        return -1;
    return streams_[stream]->last_received_response_sequence;
}

//matcher doesnot buffer request messages and therefore will just
//call this method to update the sequence number only.
bool ExchangeApiMsgBuffer::setLastReceivedClientRequestSequence(int session, int64_t sequence)
{
    if ( session < 0 || session >= (int)sessions_.size())
        return -1;
    sessions_[session]->last_received_client_request_sequence = sequence;
    return true;
}
