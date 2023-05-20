#ifndef _TCP_UTILS_HPP_
#define _TCP_UTILS_HPP_

/**
 * \brief TCP utility functions
 *
 * \details TCP utility functions
 *          InitializeAcceptSocketTCP - initialize socket for accepting applications
 *          acceptTCP - accept TCP conenction
 *          connectTCP - connect a TCP connection
 *          disconnectTCP - disconnect socket
 *          
 * \copyright
 */

#include <cstdint>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

int32_t InitializeAcceptSocketTCP(const char * interface, in_port_t port, int & accept_socket);
int32_t acceptConnectionTCP(int accept_socket, int & client_socket, sockaddr_in & client_address);
int32_t connectTCP(const char * host, in_port_t port, int & sock);
void disconnectTCP(int sock);
int32_t makeSocketNonBlocking(int socket);
int32_t makeSocketBlocking(int socket);
int32_t turnOffNagle(int socket);
int32_t setSocketReuse(int socket);
int32_t connectMulticastPublisher(const char * local_interface_ip_address,
            const char * multicast_ip_address, in_port_t port,
            int & multicast_socket, struct sockaddr_in &address);
int32_t connectMulticastSubscriber(const char * local_interface_ip_address,
            const char * multicast_ip_address, in_port_t port, int & multicast_socket);

//Error codes
static const int32_t SOCKET_CREATE_TCP_UTILS_ERROR   = -1;
static const int32_t INVALID_ADDRESS_TCP_UTILS_ERROR = -2;
static const int32_t BIND_TCP_UTILS_ERROR            = -3;
static const int32_t LISTEN_TCP_UTILS_ERROR          = -4;
static const int32_t ACCEPT_TCP_UTILS_ERROR          = -5;
static const int32_t CONNECT_TCP_UTILS_ERROR         = -6;

//Constants
static const int32_t MAX_QUEUED_ACCEPT_REQUESTS_TCP_UTILS = 10;

#endif
