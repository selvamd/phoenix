#include "ReliableMulticastChannel.hpp"
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

void ReliableMulticastChannel::SetupGapFill(int32_t gapfillacceptport_)
{
    gapfillacceptport = gapfillacceptport_;
    selector.AddServer(gapfillacceptport);
    LOG_DEBUG << "Gap filler added at port " << gapfillacceptport << LOG_END;
}

void ReliableMulticastChannel::SetupMulticast(string local_addr, string mcast_addr, int mcastport )
{
    int32_t iRC, iMulticastSocket;
    local_interface = local_addr;
    mcast_address   = mcast_addr;
    mcast_port      = mcastport;

    if ( publish_stream_id > 0 )
    {
        struct sockaddr_in addr;
        iRC = connectMulticastPublisher(local_interface.c_str(), mcast_address.c_str(),
                                        mcast_port, iMulticastSocket, addr);
        if ( iRC == 0 )
        {
            RMCSocketStreamer * streamer = new RMCSocketStreamer(iMulticastSocket, 2048);
            streamer->SetMultiCastAddress(addr);
            multicast_clientid = selector.AddClient(streamer, sizeof(MulticastMessage), 6, false);
        }
        else
        {
            LOG_ERROR << "Multicast publish error : " << local_interface << ","
                        << mcast_address << ":" << mcast_port
                        << ", Status = " << iRC << LOG_END;
        }
        
        struct StreamData data;
        data.last_event_time.set();
        data.last_gapfill_time.set();
        data.streamid                           = publish_stream_id;
        data.recoveryhost                       = "";
        data.recoveryport                       = 0;
        data.last_gapfill_sequence              = 0;
        data.max_nogap_sequence                 = 0;
        data.local_read_index                   = 0;
        data.buffer                             = new MsgBuffer<MulticastMessage>();
        data.recoveryClientID                   = -1;
        data.buffer->setCapacity(100);
        data_streams[data.streamid]             = data;
    }
    else
    {
        iRC = connectMulticastSubscriber(local_interface.c_str(), mcast_address.c_str(),
                                        mcast_port, iMulticastSocket);
        if ( iRC == 0 )
        {
            RMCSocketStreamer * streamer = new RMCSocketStreamer(iMulticastSocket, 2048);
            multicast_clientid = selector.AddClient(streamer, sizeof(MulticastMessage), 6, false);
        }
        else
        {
            LOG_ERROR << "Multicast listen error : " << local_interface << ","
                        << mcast_address << ":" << mcast_port
                        << ", Status = " << iRC << LOG_END;
        }
    }
}

void ReliableMulticastChannel::AddRecoveryToStream(char stream, std::string recovery_host, int32_t recovery_port)
{
    if (data_streams.find(stream) != data_streams.end()) return;
    
    int32_t iClientFD;
    if ( connectTCP(recovery_host.c_str(), recovery_port, iClientFD) < 0 )
    {
        LOG_ERROR << "Recovery connection error : " << stream << recovery_host << recovery_port << LOG_END;
        return;
    }
    
    struct StreamData data;
    data.last_event_time.set();
    data.last_gapfill_time.set();
    data.streamid                           = stream;
    data.recoveryhost                       = recovery_host;
    data.recoveryport                       = recovery_port;
    data.last_gapfill_sequence              = 0;
    data.max_nogap_sequence                 = 0;
    data.local_read_index                   = 0;
    data.buffer                             = new MsgBuffer<MulticastMessage>();
    RMCSocketStreamer * streamer            = new RMCSocketStreamer(iClientFD, 2048);
    data.recoveryClientID                   = selector.AddClient(streamer, sizeof(MulticastMessage), 6, false);
    data.buffer->setCapacity(100);
    data_streams[data.streamid]             = data;
}

void ReliableMulticastChannel::process()
{
    int iRC, iServerPort, iClientFD, iClientID;
    MulticastMessage msg, *msgptr;
    Timestamp now;
    now.set();
    
    selector.PollForSocketEvent();
    
    while (true)
    {
        iRC = selector.Accept(iServerPort, iClientFD);
        if ( iRC == ux_selector::END_OF_SOCK_LIST) break;
        if ( iRC == ux_selector::WOULD_BLOCK) continue;
        RMCSocketStreamer * streamer = new RMCSocketStreamer(iClientFD, 4096);
        selector.AddClient(streamer, sizeof(MulticastMessage), 6, false);
    }
    
    while ((iRC = selector.Read(iClientID, (char *)&msg)) != ux_selector::END_OF_SOCK_LIST)
    {
        if ( iRC == ux_selector::SUCCESS )
        {
            //Simply ignore for streams not handled
            auto streamItr = data_streams.find(msg.genmsg.MsgStream);
            if (streamItr == data_streams.end())
            {
                if (iClientID == multicast_clientid) continue;
                struct StreamData data;
                data.last_event_time.set();
                data.last_gapfill_time.set();
                data.streamid                           = publish_stream_id;
                data.recoveryhost                       = "";
                data.recoveryport                       = 0;
                data.last_gapfill_sequence              = 0;
                data.max_nogap_sequence                 = 0;
                data.local_read_index                   = 0;
                data.buffer                             = new MsgBuffer<MulticastMessage>();
                data.recoveryClientID                   = -1;
                data.buffer->setCapacity(100);
                data_streams[data.streamid]             = data;
            }
            
            //gapfill requests for downstream.
            if ( msg.genmsg.MsgType == GAPFILL || msg.genmsg.MsgType == TRANSMITOK)
            {
                processClientEvent(msg,streamItr->second,iClientID);
            }
            else //either multicast (iClientID == multicast_clientid) or recovery data from upstream
            {
                if (iClientID == multicast_clientid)
                    streamItr->second.last_event_time.set();
                else
                    LOG_DEBUG << "got gap fill response for seq " << msg.genmsg.MsgSequence << LOG_END;
                processStreamEvent(msg,streamItr->second, now);
            }
        }
    }
    
    //write at most one recovery response per client/stream during each pass
    for (auto &data : data_streams )
    {
        for (auto &client : data.second.clients)
        {
            //Recovery complete. Skip for this client/stream
            if (client.second.gap_fill_count <= 0) continue;
            
            //Recovery stalled due to gaps in the stream buffer. Wait for buffer to finish the gapfills
            if ( client.second.gap_fill_index > data.second.max_nogap_sequence ) continue;
            
            //Data requested outside the buffer range. At least recover msgs not lost yet
            if ( client.second.gap_fill_index < data.second.buffer->firstSequence() )
            {
                uint64_t lastmsg = client.second.gap_fill_index + client.second.gap_fill_count;
                client.second.gap_fill_index = data.second.buffer->firstSequence();
                client.second.gap_fill_count = lastmsg - client.second.gap_fill_index;
            }
            msgptr = data.second.buffer->get(client.second.gap_fill_index);
            msgptr->genmsg.MsgLength[5] = 0;
            LOG_DEBUG << "sent gapfill response for seq " << msgptr->genmsg.MsgSequence << LOG_END;
            selector.Write(client.second.clientID, (char *)msgptr, atoi(msgptr->genmsg.MsgLength));
            client.second.gap_fill_index++;
            client.second.gap_fill_count--;
        }
    }
    
    //sequence/buffer/publish or heartbeat to multicast channel as needed
    if ( publish_stream_id > 0 )
    {
        struct StreamData& publish_stream = data_streams.begin()->second;
        if ( publish_stream.buffer->size() > 0 && publish_stream.local_read_index <= publish_stream.max_nogap_sequence )
        {
            msgptr = publish_stream.buffer->get(publish_stream.local_read_index++);
            msgptr->genmsg.MsgLength[5] = 0;
            //std::cout << "Publish msg " << msgptr->genmsg.MsgSequence << std::endl;
            if ( (selector.Publish(multicast_clientid, (char *)msgptr, atoi(msgptr->genmsg.MsgLength))) < 0)
                std::cout << "Publish failed. Status " << iRC << std::endl;
            publish_stream.last_event_time = now;
        }
        else if ( now - publish_stream.last_event_time > 5000000 ) //5 sec
        {
            msg.genmsg.MsgType = HEARTBEAT;
            msg.genmsg.MsgStream = publish_stream_id;
            sprintf(msg.genmsg.MsgLength,"%lu",sizeof(msg.genmsg));
            uint64_t seq = (publish_stream.buffer->nextAvailableSequence()==0)? 0:
                            publish_stream.buffer->nextAvailableSequence()-1;
            sprintf(msg.genmsg.MsgSequence,"%llu",seq);
            if ( (selector.Publish(multicast_clientid, (char *)&msg, atoi(msg.genmsg.MsgLength))) < 0)
                LOG_ERROR << "Publish failed. Status " << iRC << LOG_END;
            publish_stream.last_event_time = now;
        }
    }
}

bool ReliableMulticastChannel::publishMsg(const char * sMsg, int32_t len)
{
    if ( publish_stream_id <= 0 ) return false;
    
    struct StreamData& publish_stream   = data_streams.begin()->second;
    publish_stream.max_nogap_sequence   = publish_stream.buffer->nextAvailableSequence();
    
    MulticastMessage msg, *msgptr;
    msg.genmsg.init(publish_stream_id, APPLICATION, publish_stream.max_nogap_sequence);
    publish_stream.buffer->insert(msg);
    msgptr = publish_stream.buffer->get(publish_stream.max_nogap_sequence);
    sprintf(msgptr->genmsg.MsgLength,"%lu",len+sizeof(msg.genmsg));
    memcpy(msgptr->fullmsg.buffer,sMsg,len);
    //LOG_DEBUG << "Publish msg request " << msg.genmsg.MsgSequence << LOG_END;
    return true;
}

void ReliableMulticastChannel::setReadIndex(char stream, uint64_t seq)
{
    //no consumption for publish node
    if ( publish_stream_id > 0 ) return;
    
    //no consumption for invalid stream
    auto streamItr = data_streams.find(stream);
    if (streamItr != data_streams.end()) return;
    
    //no consumption beyond the end of the buffer
    if ( seq >= streamItr->second.buffer->nextAvailableSequence()) return;
    
    //Cannot consume flushed messages. Set to first non-flushed message
    streamItr->second.local_read_index = max(seq,streamItr->second.buffer->firstSequence());
}

MulticastMessage * ReliableMulticastChannel::getNextMsgByIndex(char stream)
{
    //no consumption for publish node
    if ( publish_stream_id > 0 ) return nullptr;
    
    //no consumption for invalid stream
    auto streamItr = data_streams.find(stream);
    if (streamItr == data_streams.end()) return nullptr;

    //no consumption beyond the point where stream has gaps
    if ( streamItr->second.buffer->size() == 0 || streamItr->second.local_read_index > streamItr->second.max_nogap_sequence)
        return nullptr;
    
    return streamItr->second.buffer->get(streamItr->second.local_read_index++);
}

MulticastMessage * ReliableMulticastChannel::getNextMsgAsap(char stream)
{
    //no consumption for publish node
    if ( publish_stream_id > 0 ) return nullptr;
    
    //no consumption for invalid stream
    MulticastMessage * msgptr = nullptr;
    auto streamItr = data_streams.find(stream);
    if (streamItr == data_streams.end()) return nullptr;
    
    while (true)
    {
        //no more messages to consume
        if ( streamItr->second.local_read_index >= streamItr->second.buffer->nextAvailableSequence())
            return nullptr;
        
        //continue skipping buffer entries unless it is not a filler
        msgptr = streamItr->second.buffer->get(streamItr->second.local_read_index++);
        if (msgptr->genmsg.MsgType != FILLER) break;
    }
    return msgptr;
}

bool ReliableMulticastChannel::processStreamEvent(MulticastMessage &msg, struct StreamData & data, Timestamp &now)
{
    if (publish_stream_id == data.streamid) return false;
    
    uint64_t msgseq = atol(msg.genmsg.MsgSequence);
    
    //discard old message
    if (msgseq < data.buffer->firstSequence())
        return false;

    MulticastMessage fillermsg, * bufmsg;
    fillermsg.genmsg.MsgType = FILLER;
    fillermsg.genmsg.MsgStream = data.streamid;
    if (msgseq >= data.buffer->nextAvailableSequence())
    {
        //put fillers for any gaps and then insert the actual msg at the right place
        for (uint64_t seq = data.buffer->nextAvailableSequence(); seq < msgseq; seq++)
        {
            sprintf(fillermsg.genmsg.MsgSequence, "%llu", seq);
            //std::cout << "filler inserted " << data.buffer->nextAvailableSequence() << std::endl;
            data.buffer->insert(fillermsg);
        }
        
        if (msg.genmsg.MsgType == HEARTBEAT)
            msg.genmsg.MsgType = FILLER;
        
        sprintf(msg.genmsg.MsgSequence, "%llu", msgseq);
        //std::cout << "msg inserted " << data.buffer->nextAvailableSequence() << std::endl;
        data.buffer->insert(msg);
        
    }
    else if ( msg.genmsg.MsgType == APPLICATION )
    {
        bufmsg = data.buffer->get(msgseq);
        if ( bufmsg->genmsg.MsgType != FILLER ) return false;
        LOG_DEBUG << "processed gapfill response" << LOG_END;
        data.buffer->set(msgseq, &msg);
    }
    
    uint64_t orig_max_nogap_sequence = data.max_nogap_sequence;
    while (true)
    {
        //advance forward as long as stream doenot gap or end
        bufmsg = data.buffer->get(data.max_nogap_sequence);
        if ( data.max_nogap_sequence + 1 >= data.buffer->nextAvailableSequence()) break;
        if ( bufmsg->genmsg.MsgType == FILLER ) break;
        data.max_nogap_sequence++;
    }
    
    if (now - data.last_gapfill_time > 1000000 || data.max_nogap_sequence != data.last_gapfill_sequence)
    {
        if ( data.max_nogap_sequence + 1 < data.buffer->nextAvailableSequence())
        {
            if (data.max_nogap_sequence == data.last_gapfill_sequence)
                LOG_DEBUG << "cannot recover gaps" << LOG_END;
            
            data.last_gapfill_time      = now;
            data.last_gapfill_sequence  = data.max_nogap_sequence;
            fillermsg.genmsg.init(data.streamid, GAPFILL,data.max_nogap_sequence);
            fillermsg.genmsg.MsgCount   = 1;
            selector.Write(data.recoveryClientID, (char * )&fillermsg, sizeof(fillermsg.genmsg));
            LOG_DEBUG << "generated gapfill request for seq " << fillermsg.genmsg.MsgSequence << LOG_END;
        }
    }

    //publish transmitok to notify upstream node every 10 msgs
    if (data.max_nogap_sequence % 10 == 0 ||
        (data.max_nogap_sequence - orig_max_nogap_sequence) > 10)
    {
        fillermsg.genmsg.init(data.streamid, TRANSMITOK, data.max_nogap_sequence);
        //std::cout << "generated transmitok request " << std::endl;
        selector.Write(data.recoveryClientID, (char * )&fillermsg, atoi(fillermsg.genmsg.MsgLength));
    }

    return true;
}

bool ReliableMulticastChannel::processClientEvent(MulticastMessage &msg, struct StreamData & data, int32_t clientID)
{
    auto clientItr = data.clients.find(clientID);
    if (clientItr == data.clients.end())
    {
        struct ClientStreamData client;
        client.streamid         = data.streamid;
        client.clientID         = clientID;
        client.gap_fill_index   = 0;
        client.gap_fill_count   = 0;
        client.archive_ok_index = 0;
        data.clients[clientID]  = client;
    }
    
    clientItr = data.clients.find(clientID);
    clientItr->second.last_event_time.set();
    
    if (msg.genmsg.MsgType == GAPFILL )
    {
        //donot process if another recovery is in progress
        if (clientItr->second.gap_fill_count > 0 ) return false;
        clientItr->second.gap_fill_index = atol(msg.genmsg.MsgSequence);
        clientItr->second.gap_fill_count = msg.genmsg.MsgCount;
    }
    else if (msg.genmsg.MsgType == TRANSMITOK )
    {
        clientItr->second.archive_ok_index  = max(clientItr->second.archive_ok_index, (uint64_t)atol(msg.genmsg.MsgSequence));
        uint64_t min_archive_ok_index       = data.buffer->nextAvailableSequence();
        min_archive_ok_index                = min(min_archive_ok_index,data.local_read_index);
        for (auto & tempItr : data.clients)
            min_archive_ok_index = min(min_archive_ok_index,tempItr.second.archive_ok_index);
        data.buffer->flush(min_archive_ok_index);
        LOG_DEBUG << "Flushing based on TransmitOK " << min_archive_ok_index << LOG_END;
    }
    
    return true;
}

