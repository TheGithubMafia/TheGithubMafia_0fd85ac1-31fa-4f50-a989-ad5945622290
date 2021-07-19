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
#include "communication.h"
#include "logging.h"
#include "config.h"
#include "chat.h"

struct com_SocketInfo serverSockAddr;
pthread_t *threads;
int com_serverSocket = -1, com_epollfd = -1;
int com_numThreads = -1;
struct usr_UserData *serverUser;

int timeOut, messageLimit;

extern struct chat_ServerLists serverLists;

int init_server(){
    // Initalize the server socket
    com_serverSocket = com_startServerSocket(&fig_Configuration, &serverSockAddr, 0);
    if(com_serverSocket < 0){
        log_logMessage("Retrying with IPv4...", INFO);
        com_serverSocket = com_startServerSocket(&fig_Configuration, &serverSockAddr, 1);
        if(com_serverSocket < 0){
                return -1;
        }
    }

	// Create the epoll
	com_epollfd = epoll_create1(0);
	if(com_epollfd == -1){
		log_logError("Error setting up epoll", FATAL);
		return -1;
	}

	// TODO deal with random users sending data to this user
	serverUser = usr_createUser(&serverSockAddr, fig_Configuration.serverName);
	if(!serverUser){
		log_logMessage("Unable to create the SERVER user", ERROR);
		return -1;
	}

	// Setup listening socket in the epoll	
	struct epoll_event ev;
	ev.events = EPOLLIN|EPOLLONESHOT;
	ev.data.ptr = serverUser;
	if(epoll_ctl(com_epollfd, EPOLL_CTL_ADD, com_serverSocket, &ev) == -1){
		log_logError("Error adding listening socket to epoll", FATAL);
		return -1;
	}
    
	timeOut = fig_Configuration.timeOut;
	messageLimit = fig_Configuration.messageLimit;

    //Setup threads for listening
    com_numThreads = com_setupIOThreads(&fig_Configuration);

    return 1;
}

void com_close(){
    if(com_serverSocket >= 0){
        close(com_serverSocket);
    }

    free(threads);
}

// Make a new job and insert it into the queue for sending
int com_sendStr(struct usr_UserData *user, char *msg){
	if(user == NULL || user == &serverLists.users[0])
		return -1;

    struct com_QueueJob *job = calloc(1, sizeof(struct com_QueueJob));
    job->user = user;

    snprintf(job->str, ARRAY_SIZE(job->str), "%s\r\n", msg);
    com_insertQueue(job);

	// Safely get user's socket
	pthread_mutex_lock(&user->userMutex);
	int sock = user->socketInfo.socket2;
	pthread_mutex_unlock(&user->userMutex);

	// Setup to allow for a write
	struct epoll_event ev = {.events = EPOLLOUT|EPOLLONESHOT};
	ev.data.ptr = user;
	if(epoll_ctl(com_epollfd, EPOLL_CTL_MOD, sock, &ev) == -1){
		log_logError("Error rearming write socket", WARNING);
		usr_deleteUser(user);
		return -1;
	}

    return 1;
}

// Will remove all remaining jobs in a user's queue
int com_cleanQueue(struct usr_UserData *user){
	if(user == NULL)
		return -1;

	pthread_mutex_lock(&user->userMutex); 

	while(link_isEmpty(&user->sendQ) == -1){ // Keep removing items until empty
		free(link_remove(&user->sendQ, 0));
	}

	pthread_mutex_unlock(&user->userMutex); 

	return 1;
}

int com_insertQueue(struct com_QueueJob *job){
    struct usr_UserData *user = job->user;

	if(job == NULL){
		log_logMessage("Invalid job.", DEBUG);
		return -1;
	}

    if(!user || user->id == -1){ // User is disconnecting; nothing new to be sent
        log_logMessage("User no longer valid", TRACE);
        return -1; 
    }

    pthread_mutex_lock(&user->userMutex);
	struct link_Node *ret = link_add(&user->sendQ, job);
	if(ret == NULL){
		log_logMessage("Error adding job to queue", WARNING);
	}
    pthread_mutex_unlock(&user->userMutex);

    return 1;
}

int getHost(char ipstr[INET6_ADDRSTRLEN], struct sockaddr_storage addr, int protocol){
	void *addrSrc;

	if(protocol == AF_INET){
		struct sockaddr_in *s = (struct sockaddr_in *)&addr;
		addrSrc = &s->sin_addr;
	} else if (protocol == AF_INET6){
		struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
		addrSrc = &s->sin6_addr;
	} else {
		log_logMessage("Invalid socket family", WARNING);
		return -1;
	}

	if(inet_ntop(protocol, addrSrc, ipstr, INET6_ADDRSTRLEN) == NULL){
		log_logError("Error getting hostname", WARNING);
		return -1;
	}

	return 0;
}

// Will read data from the socket and properly send it for processing
int com_readFromSocket(struct epoll_event *userEvent, int epollfd){
	struct usr_UserData *user = userEvent->data.ptr;
	if(user == NULL){
		log_logMessage("No user associated with socket", ERROR);
		return -1;
	}
	int sockfd = user->socketInfo.socket;
		
	char buff[MAX_MESSAGE_LENGTH+1] = {0};
	int bytes = read(sockfd, buff, ARRAY_SIZE(buff)-1);

	// Rearm the fd because data has already been read
	struct epoll_event ev = {.events = EPOLLIN|EPOLLONESHOT, .data.ptr = user};
	if(epoll_ctl(epollfd, EPOLL_CTL_MOD, sockfd, &ev) == -1){
		close(sockfd); // TODO delete user
		log_logError("Error rearming socket", ERROR);
		return -1;
	}

	switch(bytes){
		case 0:
			log_logMessage("Client disconnect.", INFO);
			goto disconnect_client; // Fallthrough minus the logging part
		case 1:
			log_logError("Error reading from client.", WARNING);
		disconnect_client:
			usr_deleteUser(user);
			break;

		default: ; 
			// Check time inbetween messages (too fast == quit)
			pthread_mutex_lock(&user->userMutex);
			double timeDifference = difftime(time(NULL), user->lastMsg);
			user->lastMsg = time(NULL);
			user->pinged = -1; // Reset ping
			pthread_mutex_unlock(&user->userMutex);

			if(timeDifference < (double) messageLimit){
				log_logMessage("User sending messages too fast.", INFO);
				usr_deleteUser(user);
				return -1;
			}

			// Split up each line into its own job
			int loc = 0;
			while(loc >= 0){
				int oldLoc = loc;
				char line[1024];

				// Split each line into its own job
				loc = chat_findEndLine(buff, ARRAY_SIZE(buff), loc);
				if(loc > -1)
					buff[loc - 1] = '\0';
				strncpy(line, &buff[oldLoc], ARRAY_SIZE(line)-1);
				chat_insertQueue(user, 0, line, NULL);
			}
	}

	return 1;
}

// Writes avaliable queue data to socket
int com_writeToSocket(struct epoll_event *userEvent, int epollfd){
	struct usr_UserData *user = userEvent->data.ptr;
	if(user == NULL){
		log_logMessage("No user associated with socket", ERROR);
		return -1;
	}

	// Remove the first job
	struct com_QueueJob *job = NULL;
	pthread_mutex_lock(&user->userMutex);
	if(link_isEmpty(&user->sendQ) == -1){
		job = link_removeNode(&user->sendQ, user->sendQ.head);
	}
	pthread_mutex_unlock(&user->userMutex);

	if(job == NULL)
		return -1;

	char buff[ARRAY_SIZE(job->str)];
	strncpy(buff, job->str, ARRAY_SIZE(buff));
	free(job);

	pthread_mutex_lock(&user->userMutex);
	int socket = user->socketInfo.socket2;
	if(user->id < 0)
		socket = -1;
	pthread_mutex_unlock(&user->userMutex);

	if(socket < 0)
		return -1;

	log_logMessage(buff, MESSAGE);
	int ret = write(user->socketInfo.socket2, buff, strlen(buff));
	if(ret == -1){
		log_logError("Error writing to client", ERROR);
		usr_deleteUser(user);
		return -1;
	}
	
	// Setup to allow for another write if avaliable
	struct epoll_event ev = {.events = EPOLLOUT|EPOLLONESHOT};
	ev.data.ptr = user;
	if(epoll_ctl(epollfd, EPOLL_CTL_MOD, user->socketInfo.socket2, &ev) == -1){
		log_logError("Error rearming write socket", WARNING);
		usr_deleteUser(user);
		return -1;
	}
	
	return 0;
}

// TODO - clean this mess of a method up
void *com_communicateWithClients(void *param){
    int *epollfd = param;
	struct usr_UserData *user;

	// First is options, second is storage
	struct epoll_event events[10];
    int num;
	
    while(1){
        num = epoll_wait(*epollfd, events, ARRAY_SIZE(events), -1); 
		if(num == -1){
			if(num == EINTR)
				continue; // OK to continue

			log_logError("epoll_wait", ERROR);
			exit(EXIT_FAILURE);
		}

		for(int i = 0; i < num; i++){
			user = events[i].data.ptr;
			if(user == serverUser){
				com_acceptClient(&serverSockAddr, *epollfd);
				continue;
			}

			if(events[i].events & EPOLLIN){
				com_readFromSocket(&events[i], *epollfd);
			} else if (events[i].events & EPOLLOUT){
				com_writeToSocket(&events[i], *epollfd);
			} // Add disconnection/error
		}
    }
    
    return NULL;
}

int com_setupIOThreads(struct fig_ConfigData *config){
    char buff[BUFSIZ];
    int numThreads = config->threadsIO;

	threads = calloc(numThreads, sizeof(pthread_t));
	if(threads == NULL){
		log_logError("Error allocating space for IO threads", FATAL);
		exit(EXIT_FAILURE);
	}

    int ret = 0;
    for(int i = 0; i < numThreads; i++){
        ret = pthread_create(&threads[i], NULL, com_communicateWithClients, &com_epollfd);
        if(ret != 0){
            snprintf(buff, ARRAY_SIZE(buff), "Error with pthread_create: %d", ret);
            log_logMessage(buff, ERROR);
            return -1;
        }
    }
	snprintf(buff, ARRAY_SIZE(buff), "Successfully listening on %d threads", numThreads);
	log_logMessage(buff, INFO);

    return numThreads;
}

int com_acceptClient(struct com_SocketInfo *serverSock, int epoll_sock){
	char buff[BUFSIZ];

	struct sockaddr_storage cliAddr;
	socklen_t cliAddrSize = sizeof(cliAddr);

	//Accept client's connection and log its IP
	struct com_SocketInfo newCli;
	int client = accept(serverSock->socket, (struct sockaddr *)&cliAddr, &cliAddrSize);

	// Reset listening socket
	struct epoll_event ev = {.events = EPOLLIN | EPOLLONESHOT}; 
	ev.data.ptr = serverUser;
	if(epoll_ctl(epoll_sock, EPOLL_CTL_MOD, serverSock->socket, &ev) == -1){
		log_logError("epoll_ctl rearming server", ERROR);
		return -1;
	}

	if(client < 0){
		log_logError("Error accepting client", WARNING);
		return -1;
	}

	char ipstr[INET6_ADDRSTRLEN];
	if(!getHost(ipstr, cliAddr, serverSock->addr.ss_family)){
		snprintf(buff, ARRAY_SIZE(buff), "New connection from: %s", ipstr);
		log_logMessage(buff, INFO);
	}

	// Fill in newCli struct
	newCli.socket = client;
	memcpy(&newCli.addr, &cliAddr, cliAddrSize);
	newCli.socket2 = dup(client);
	if(newCli.socket2 == -1){
		log_logError("Error creating 2nd fd for client", WARNING);
		return -1;
	}

	if(chat_serverIsFull() == 1){
		snprintf(buff, ARRAY_SIZE(buff), "Server is full, try again later.");
		send(client, buff, strlen(buff), 0);
		close(client);
		close(newCli.socket2);
		
		return -1;
	} else {
		struct usr_UserData *user = usr_createUser(&newCli, UNREGISTERED_NAME);
		if(user == NULL){
			close(client);
			close(newCli.socket2);
			return -1;
		} 
		ev.data.ptr = user;

		// Add to epoll
		if(epoll_ctl(epoll_sock, EPOLL_CTL_ADD, client, &ev) == -1){
			log_logError("epoll_ctl accepting client", WARNING);
			return -1;
		}

		// Setup to allow for a write
		ev.events = EPOLLOUT|EPOLLONESHOT;
		if(epoll_ctl(epoll_sock, EPOLL_CTL_ADD, newCli.socket2, &ev) == -1){
			log_logError("epoll_ctl writing to client", WARNING);
			return -1;
		}
	}

	return 0;
}

int com_startServerSocket(struct fig_ConfigData* data, struct com_SocketInfo* sockAddr, int forceIPv4){
	int sock;
	struct addrinfo hints;
	struct addrinfo *res, *rp;

	memset(&hints, 0, sizeof(hints));
	if(forceIPv4){
		hints.ai_family = AF_INET;
	} else {
		hints.ai_family = AF_INET6;
	}
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE | AI_V4MAPPED;

	//convert int PORT to string
	char port[6];
	snprintf(port, ARRAY_SIZE(port), "%d", data->port);

	int ret = getaddrinfo(NULL, port, &hints, &res);
	if(ret != 0){
		char msg[BUFSIZ];
		snprintf(msg, ARRAY_SIZE(msg), "getaddrinfo: %s", gai_strerror(ret));
		log_logMessage(msg, ERROR);
		return -1;
	}

	// Go through all possible options given by getaddrinfo
	for(rp = res; rp != NULL; rp = rp->ai_next){
		sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if(sock < 0){
			continue;
		}

		//successful
		if(!bind(sock, rp->ai_addr, rp->ai_addrlen)){
			char msg[BUFSIZ];
			snprintf(msg, ARRAY_SIZE(msg), "Binded server to port %s", port);
			log_logMessage(msg, 2);
			break; 
		}

		close(sock);
	}	

	// No address succeeded
	if(rp == NULL){
		log_logError("No addresses sucessfully binded", ERROR);
		return -1;
	}

	//Copy sockaddr struct into the sockAddr struct for later use
	if(sockAddr != NULL){
		memcpy(&sockAddr->addr, &rp->ai_addr, sizeof(rp->ai_addr));
		sockAddr->addr.ss_family = rp->ai_family;
	}

	freeaddrinfo(res);

	ret = listen(sock, 128);
	if(ret != 0){
		log_logError("Error listening on socket", ERROR);
		close(sock);
		return -1;
	} else {
		char msg[BUFSIZ];
		snprintf(msg, ARRAY_SIZE(msg), "Listening to port: %s.", port);
		log_logMessage(msg, INFO);
	}

	sockAddr->socket = sock;
	return sock;
}
