#ifndef communication_h
#define communication_h

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <limits.h>
#include <time.h>
#include "logging.h"
#include "config.h"
#include "linkedlist.h"

#define ARRAY_SIZE(arr) (int)(sizeof(arr)/sizeof((arr)[0]))
#define MAX_MESSAGE_LENGTH 2048

/* This header defines functions that handle all of the
 * Sending and receiving of data that the server will handle
 */

// Jobs for queues
// TODO - combine str and msg into a union
struct com_QueueJob {
    int type;
    struct usr_UserData *user;
    char str[1024];
    struct chat_Message *msg; 
};

//struct to store data about the socket, and its file descriptor
struct com_SocketInfo {
    int socket;
	int socket2; // Used for filtering epoll writing events
    struct sockaddr_storage addr;
};

extern int com_serverSocket;

// Setup the server's socket
int init_server();

// close server socket
void com_close();

// Will send a string to client inside node, also appends \r\n
int com_sendStr(struct usr_UserData *user, char *msg);

// Remove all user jobs from queue
int com_cleanQueue(struct usr_UserData *user);

// Insert selected job into the queue
int com_insertQueue(struct com_QueueJob *job);

// Convert sockaddr to a string to display the client's IP in string form
int getHost(char ipstr[INET6_ADDRSTRLEN], struct sockaddr_storage addr, int protocol);

// Will read data from the socket and properly send it for processing
int com_readFromSocket(struct epoll_event *userEvent, int epollfd);

// Writes avaliable queue data to socket
int com_writeToSocket(struct epoll_event *userEvent, int epollfd);

// Handle all incoming data from the client
void *com_communicateWithClients(void *param);

// Setup the threads to start listening for incoming communication and send
// outbound data to clients
int com_setupIOThreads(struct fig_ConfigData *config);

//accept communication with clients
int com_acceptClient(struct com_SocketInfo *serverSock, int epoll_sock);

//start server socket based on configuration
int com_startServerSocket(struct fig_ConfigData* data, struct com_SocketInfo* sockAddr, int forceIPv4);

#endif
