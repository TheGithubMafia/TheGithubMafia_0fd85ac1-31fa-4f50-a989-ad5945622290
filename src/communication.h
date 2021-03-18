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
#include <poll.h>
#include <limits.h>
#include <time.h>
#include "logging.h"
#include "config.h"
#include "linkedlist.h"

#define ARRAY_SIZE(arr) (int)(sizeof(arr)/sizeof((arr)[0]))

/* This header defines functions that handle all of the
 * Sending and receiving of data that the server will handle
 */

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
	struct pollfd *clients; /* pollfd array for poll() */
    pthread_t thread;
    pthread_mutex_t clientListMutex;
};

// Queue for the communication threads to get write jobs
struct com_DataQueue {
    struct link_List queue;
    pthread_mutex_t queueMutex;
};

extern int com_serverSocket;

// Setup the server's socket
int init_server();

// close server socket
void com_close();

// Insert selected node into the queue for processing
// Mutex is handled by this function internally
int com_insertQueue(struct link_Node *node);

// Convert sockaddr to a string to display the client's IP in string form
int getHost(char ipstr[INET6_ADDRSTRLEN], struct sockaddr_storage addr, int protocol);

// Will determine if the specified socket fd is inside a pollfd struct
// For use inside inside of a thread; Make sure to lock mutex if needed
int com_hasSocket(int socket, struct pollfd *conns, int size);

//Find first avaliable job in the queue that the thread can use
struct link_Node *com_findJob(struct com_DataQueue *dataQ, struct pollfd *conns, int size);

// Handle all incoming data from the client
void *com_communicateWithClients(void *param);

// Place a new client into a pollfd struct
int com_insertClient(struct com_SocketInfo addr, struct com_ClientList clientList[], int numThreads);

// Setup the threads to start listening for incoming communication and send
// outbound data to clients
int com_setupIOThreads(struct fig_ConfigData *config);

//accept and handle all communication with clients
int com_acceptClients();

//start server socket based on configuration
int com_startServerSocket(struct fig_ConfigData* data, struct com_SocketInfo* sockAddr, int forceIPv4);

#endif
