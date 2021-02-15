#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <poll.h>
#include <limits.h>
#include <time.h>
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

//struct to store data about each thread's pollfd struct
struct com_ClientList {
	int maxClients;
	int connected;
	int threadNum;
	struct pollfd *clients;
};

// Convert sockaddr to a string to display the client's IP in string form
int getHost(char ipstr[INET6_ADDRSTRLEN], struct sockaddr_storage addr, int protocol);

// Handle all communication between server and client
void *com_communicateWithClients(void *param);

// Place a new client into a pollfd struct
int com_insertClient(struct com_SocketInfo addr, struct com_ClientList clientList[], int numThreads);

//accept and handle all communication with clients
int com_acceptClients(struct com_SocketInfo* sockAddr, struct fig_ConfigData* data);

//start server socket based on configuration
int com_startServerSocket(struct fig_ConfigData* data, struct com_SocketInfo* sockAddr, int forceIPv4);

#endif
