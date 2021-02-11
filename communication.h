#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "logging.h"
#include "config.h"
#ifndef communication_h
#define communication_h

#define ARRAY_SIZE(arr) (int)(sizeof(arr)/sizeof((arr)[0]))

//struct to store data about the socket, and its file descriptor
struct com_SocketInfo {
	int socket;
	struct sockaddr_storage addr;
};

// Convert sockaddr to a string to display the client's IP in string form
int getHost(char ipstr[INET6_ADDRSTRLEN], struct sockaddr_storage addr, int protocol);

//accept and handle all communication with clients
int com_acceptClients(struct com_SocketInfo* sockAddr);

//start server socket based on configuration
int com_startServerSocket(struct fig_ConfigData* data, struct com_SocketInfo* sockAddr, int forceIPv4);

#endif
