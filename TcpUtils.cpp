#include "TcpUtils.hpp"
#include "Logger.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <unistd.h>
#include <iostream>
#include <netinet/tcp.h>

/**
 * \brief initialize socket used for accepting TCP connections
 *
 * \details Initialize socket for acceptint TCP connection. 
 *          Accept socket will be stored in socket which is passed
 *          by reference. It will be set to -1 if there are any errors.
 *
 * \param interface - interface to listen on
 * \param port - port to listen on
 * \param accept_socket - reference to store listening socket 
 *
 * \return 0 if succesful
 * \return < 0 if error
 */
int32_t InitializeAcceptSocketTCP(const char * interface, in_port_t port, int & accept_socket)
{
    if ((accept_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        return SOCKET_CREATE_TCP_UTILS_ERROR;
        
    setSocketReuse(accept_socket);

    struct sockaddr_in address;
    memset(&address, '\0', sizeof(address));
    address.sin_family = AF_INET;
    if (inet_pton(AF_INET, interface, &address.sin_addr) == 0)
    {
        accept_socket = -1;
        return INVALID_ADDRESS_TCP_UTILS_ERROR;
    }

    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl (INADDR_ANY);

    if (bind(accept_socket, (struct sockaddr *) & address, sizeof(address)) < 0)
        return BIND_TCP_UTILS_ERROR;

    if (listen(accept_socket, MAX_QUEUED_ACCEPT_REQUESTS_TCP_UTILS) < 0)
        return LISTEN_TCP_UTILS_ERROR; 

    return 0;
}

/**
 * \brief Accept TCP connection
 *
 * \param accept_socket - socket for listening port
 * \param client_socket - reference to store socket for client connection. 
 * \param client_address - reference to store address of connecting client
 *  
 * \return 0 if succesful
 * \return < 0 if error, client_socket set to value <0
 */
int32_t acceptConnectionTCP(int accept_socket, int & client_socket, sockaddr_in & client_address)
{
    socklen_t addr_length = sizeof(client_address);

    client_socket = accept(accept_socket, (struct sockaddr *) & client_address, & addr_length);
    if (client_socket < 0)
        return ACCEPT_TCP_UTILS_ERROR;

    return 0;
}

/**
 * \brief Initiate TCP connection
 *
 * \param host - host ip address
 * \param port - host port
 * \param sock - reference to store socket file descriptor, set < 0 on error
 *
 * \return 0 on success
 * \return < 0 on eror
 */
int32_t connectTCP(const char * host, in_port_t port, int & sock)
{
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0)
    {
        std::cout << "Error creating socket for host:port => " << host << ":" << port << std::endl;
        return SOCKET_CREATE_TCP_UTILS_ERROR;
    }

    struct sockaddr_in address;
    memset(&address, '\0', sizeof(address));
    address.sin_family = AF_INET;
    int rc = inet_pton(AF_INET, host, &address.sin_addr.s_addr);
    if (rc <= 0)
    {
        std::cout << "Invalid address" << std::endl;
        return INVALID_ADDRESS_TCP_UTILS_ERROR;
    }
    address.sin_port = htons(port);

    if (connect(sock, (struct sockaddr *) & address, sizeof(address)) < 0)
    {
        std::cout << "Error in connect to port:" << port << std::endl;
        return CONNECT_TCP_UTILS_ERROR;
    }

    return 0;
}

/**
 * \brief close socket connection
 *
 * sock will be set to -1 upon completion
 *
 * \param sock - socket to close
 */
void disconnectTCP(int sock)
{
    close(sock);

    sock = -1;
}

/**
 * \brief Make socket non-blocking
 *
 * \param socket
 *
 * \return 0 on success
 * \return -1 on error
 */
int32_t makeSocketNonBlocking(int socket)
{
    //std::cout << "Setting socket " << socket << " to nonblocking" << std::endl;

    int flags = fcntl(socket, F_GETFL, 0);
    if (flags < 0) 
        return -1;

    flags = flags|O_NONBLOCK;

    if (fcntl(socket, F_SETFL, flags | O_NONBLOCK) >= 0)
        return 0;

    return -1;
}

/**
 * \brief Make socket blocking
 *
 * \param socket
 *
 * \return 0 on success
 * \return -1 on error
 */
int32_t makeSocketBlocking(int socket)
{
    //std::cout << "Setting socket " << socket << " to blocking" << std::endl;

    int flags = fcntl(socket, F_GETFL, 0);
    if (flags < 0) 
        return -1;

    if (fcntl(socket, F_SETFL, flags & ~O_NONBLOCK) >= 0)
        return 0;

    return -1;
}

/**
 * \brief Turn off nagle algorithm
 *
 * \param socket
 *
 * \return 0 on success
 * \return -1 on failure
 */
int32_t turnOffNagle(int socket)
{
    int flag = 1;
    int rc = setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));    

    if (rc < 0)
        return -1;

    return 0;
}

int32_t setSocketReuse(int socket)
{
    int flag = 1;
    int rc = setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (char *) &flag, sizeof(int));    

    if (rc < 0)
        return -1;

    return 0;
}
  
/**
 * \brief Initialize socket for multicast publication. Disables local loopback.
 *
 * \param local interface ip address
 * \param multicast ip address
 * \param port
 * \param reference to variable to store socket
 *
 * \return 0 on success and socket structure information is store in address parameter
 * \return -1 on failure
 */
int32_t connectMulticastPublisher(const char * local_interface_ip_address, const char * multicast_ip_address, in_port_t port, int & multicast_socket, struct sockaddr_in &address)
{
    LOG_DEBUG << "local_interface_ip_address: " << local_interface_ip_address << ", multicast_ip_address: " << multicast_ip_address << ", port: " << port << ",multicast_socket: " << multicast_socket << LOG_END;

    if ((multicast_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        return -1;

    //Set up multicast address 
    //struct sockaddr_in address;
    memset(&address, '\0', sizeof(address));
    address.sin_family = AF_INET;
    if (inet_pton(AF_INET, multicast_ip_address, &address.sin_addr) == 0)
    {
        multicast_socket = -1;
        return -1;
    }
    address.sin_port = htons(port);
    
    //Disable loopback
    /*
    char loopback = 0;
    if (setsockopt(multicast_socket, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopback, sizeof(loopback)) < 0)
    {
        close(multicast_socket);
        multicast_socket = -1;
        return -1;
    }
    */
    
    //Enable local interface to publish multicast
    struct in_addr local_interface_address;
    if (inet_pton(AF_INET, local_interface_ip_address, &local_interface_address) == 0)
    {
        close(multicast_socket);
        multicast_socket = -1;
        return -1;
    }
    if(setsockopt(multicast_socket, IPPROTO_IP, IP_MULTICAST_IF, (char *)&local_interface_address, sizeof(local_interface_address)) < 0)
    {
        close(multicast_socket);
        multicast_socket = -1;
        return -1;
    }

    return 0;
}

/**
 * \brief Initialize socket for receiving multicast packets. 
 *        Sets socket options to allow for multiple receivers.
 *
 * \param local interface ip address
 * \param multicast ip address
 * \param port
 * \param reference to variable to store socket
 *
 * \return 0 on success
 * \return -1 on failure
 */
int32_t connectMulticastSubscriber(const char * local_interface_ip_address, const char * multicast_ip_address, in_port_t port, int & multicast_socket)
{
    if ((multicast_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        return -1;

    //Set socket reuse to true to allow for multiple receivers in the same server
    int reuse = 1;
    //if(setsockopt(multicast_socket, SOL_SOCKET, SO_REUSEADDR , (char *)&reuse, sizeof(reuse)) < 0)
    if(setsockopt(multicast_socket, SOL_SOCKET, SO_REUSEPORT, (char *)&reuse, sizeof(reuse)) < 0)
    {
        perror("error setting socket option");
        close(multicast_socket);
        multicast_socket = -1;
        return -1;
    }

    //bind to local interface
    struct sockaddr_in local_interface_address;
    memset((char *) &local_interface_address, 0, sizeof(local_interface_address));
    local_interface_address.sin_family = AF_INET;
    local_interface_address.sin_port = htons(port);
    local_interface_address.sin_addr.s_addr = htonl(INADDR_ANY);
    /*
    if (inet_pton(AF_INET, INADDR_ANY, &local_interface_address.sin_addr) == 0)
    {
        perror("inet_pton for local interface:");
        close(multicast_socket);
        multicast_socket = -1;
        return -1;
    }
    */
    if(bind(multicast_socket, (struct sockaddr*)&local_interface_address, sizeof(local_interface_address)))
    {
        perror("bind for local interface:");
        close(multicast_socket);
        multicast_socket = -1;
        return -1;
    }
        
    //join multicast group
    struct ip_mreq multicast_group;
    if (inet_pton(AF_INET, multicast_ip_address, &multicast_group.imr_multiaddr) == 0)
    {
        perror("inet_pton for multicast group for multicast address:");
        close(multicast_socket);
        multicast_socket = -1;
        return -1;
    }
    if (inet_pton(AF_INET, local_interface_ip_address, &multicast_group.imr_interface) == 0)
    {
        perror("inet_pton for multicast group for local interface address:");
        close(multicast_socket);
        multicast_socket = -1;
        return -1;
    }
    if (setsockopt(multicast_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&multicast_group, sizeof(multicast_group)) < 0)
    {
        close(multicast_socket);
        multicast_socket = -1;
        return -1;
    }

    return 0;
}




    