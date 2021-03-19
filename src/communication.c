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

struct com_ClientList *clientList;

struct com_SocketInfo serverSockAddr;
int com_serverSocket = -1;

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

    //Check that the config data is correct
	if(fig_Configuration.threadsIO < 1){
		fig_Configuration.threadsIO = 1;
		log_logMessage("Must have at least 1 IO thread! Using 1 IO thread", WARNING);
	}	

	if(fig_Configuration.clients <= 0){
		fig_Configuration.clients = 20;
		log_logMessage("Max clients must be at least 1! Using 20 clients", WARNING);
	}
	chat_setMaxUsers(fig_Configuration.clients);

    // Allocate memory based on size from configuration
	clientList = calloc(fig_Configuration.threadsIO, sizeof(struct com_ClientList));
    if (clientList == NULL) {
        log_logError("Error initalizing clientList", ERROR);
        return -1;
    }

    // Initalize queue mutex to prevent locking issues
    for (int i = 0; i < fig_Configuration.threadsIO; i++){
        int ret = pthread_mutex_init(&clientList[i].jobs.queueMutex, NULL);
        if (ret < 0){
            log_logError("Error initalizing pthread_mutex", ERROR);
            return -1;
        }
    }

    //Setup threads for listening
    com_setupIOThreads(&fig_Configuration);

    return 1;
}

void com_close(){
    if(com_serverSocket >= 0){
        close(com_serverSocket);
    }

    free(clientList);
}

int com_insertQueue(struct link_Node *node){
    struct com_ClientList *cliList = NULL;
    struct chat_UserData *user = (struct chat_UserData *) node->data;
    int sockfd = user->socketInfo.socket;

    // Search through each pollfd struct to find the correct queue to insert job
    for(int i = 0; i < fig_Configuration.threadsIO; i++) {
        cliList = &clientList[i];

        pthread_mutex_lock(&cliList->clientListMutex); 

        if(com_hasSocket(sockfd, cliList->clients, cliList->maxClients) >= 0){
            pthread_mutex_lock(&cliList->jobs.queueMutex); 
            struct link_Node *ret = link_add(&cliList->jobs.queue, node);
            if(ret == NULL){
                log_logMessage("Error adding job to queue", WARNING);
            }
            pthread_mutex_unlock(&cliList->jobs.queueMutex); 

            pthread_mutex_unlock(&cliList->clientListMutex); 
            break;
        }

        pthread_mutex_unlock(&cliList->clientListMutex); 
    }

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

// Will determine if the specified socket fd is inside a pollfd struct
// For use inside of a thread; Make sure to lock this externally if needed
int com_hasSocket(int socket, struct pollfd *conns, int size){
    for(int i = 0; i < size; i++){
        if(conns[i].fd == socket){
            return i;
        }
    }

    return -1;
}

// Will return location of job if true, else -1
int com_hasJob(struct com_DataQueue *dataQ, int sockfd){
    struct link_Node *node = NULL;
    struct chat_UserData *user = NULL;
    int loc = 0;

    pthread_mutex_lock(&dataQ->queueMutex); 

    for(node = dataQ->queue.head; node != NULL; node = node->next){
        user = ((struct link_Node *) node->data)->data;

        if(user->socketInfo.socket == sockfd){
            pthread_mutex_unlock(&dataQ->queueMutex); 
            return loc;
        }

        loc++;
    }

    pthread_mutex_unlock(&dataQ->queueMutex); 
    return -1;
}

void *com_communicateWithClients(void *param){
	struct com_ClientList *clientList = param;
	struct timespec delay = {.tv_nsec = 1000000}; // 1ms
	int ret;

    // Initalize the pollfd struct array
	struct pollfd connections[clientList->maxClients];
    clientList->clients = connections;

    // Settings for each pollfd struct
    for(int x = 0; x < clientList->maxClients; x++){
        clientList->clients[x].fd = -1;
        clientList->clients[x].events = POLLIN | POLLHUP | POLLOUT;
    }

	struct chat_UserData *user;
    struct link_Node *node;
	while(1){
		pthread_mutex_lock(&clientList->clientListMutex);

		ret = poll(clientList->clients, clientList->maxClients, 50);
		if(ret != 0){
			for(int i = 0; i < clientList->maxClients; i++){
                int sockfd = clientList->clients[i].fd;
				if(clientList->clients[i].revents & POLLERR){
					log_logMessage("Client error", WARNING);
					close(clientList->clients[i].fd);
					clientList->clients[i].fd = -1;
					clientList->connected--;

				} else if (clientList->clients[i].revents & POLLHUP){
					log_logMessage("Client closed connection", INFO);
					close(clientList->clients[i].fd);
					clientList->clients[i].fd = -1;
					clientList->connected--;

				} else if (clientList->clients[i].revents & POLLIN){
                    // Really think about what to do if socket is not yet a user
                    node = chat_getUserBySocket(sockfd);
                    struct chat_UserData *user = (struct chat_UserData *) node->data;
					int bytes = read(sockfd, user->input, ARRAY_SIZE(user->input));
					user->input[bytes] = '\0';
					if(bytes <= 0){
						if(bytes == 0){
							log_logMessage("Client disconnect", INFO);
						} else if(bytes == -1){
							log_logError("Error reading from client", WARNING);
						}

						close(clientList->clients[i].fd);
						clientList->clients[i].fd = -1;
						clientList->connected--;
					} else {
                        chat_insertQueue(node);
					}
				} else if (clientList->clients[i].revents & POLLOUT){
                    int jobLoc = com_hasJob(&clientList->jobs, clientList->clients[i].fd);

                    // User is in job list
                    if(jobLoc >= 0){
                        pthread_mutex_lock(&clientList->jobs.queueMutex); 
                        node = link_remove(&clientList->jobs.queue, jobLoc);
                        user = node->data;
                        pthread_mutex_unlock(&clientList->jobs.queueMutex); 

                        if(node != NULL && user != NULL){
                            write(user->socketInfo.socket, user->output, ARRAY_SIZE(user->output));
                        }
                    }

                }
			}
		} else if (ret < 0) {
			log_logError("Error with poll()", ERROR);
			exit(EXIT_FAILURE);
		}

		pthread_mutex_unlock(&clientList->clientListMutex);
		nanosleep(&delay, NULL); // Allow other threads time to access mutex
	}
	
	return NULL;
}

//Place a client into one of the poll arrays
int com_insertClient(struct com_SocketInfo addr, struct com_ClientList clientList[], int numThreads){
    //Go thru each clientList and see which one has the least connected clients
	int least = -1, numConnected = INT_MAX;
	for(int i = 0; i < numThreads; i++){
		pthread_mutex_lock(&clientList[i].clientListMutex);
		if(clientList[i].connected < clientList[i].maxClients){
			if(least == -1 || clientList[i].connected < numConnected){
				least = i;
				numConnected = clientList[i].connected;
			}
		}	
		pthread_mutex_unlock(&clientList[i].clientListMutex);
	}

	if(least != -1){
		pthread_mutex_lock(&clientList[least].clientListMutex);
		clientList[least].connected++;
		
		int selectedSpot = 0;
		for(int i = 0; i < clientList[least].maxClients; i++){
			if(clientList[least].clients[i].fd < 0){
				selectedSpot = i;
				break;
			}
		}
		clientList[least].clients[selectedSpot].fd = addr.socket;
		
		pthread_mutex_unlock(&clientList[least].clientListMutex);

        chat_createUser(&addr, "NOOB1");

		return least;
	}

	log_logMessage("Reached max clients!", WARNING);
	return -1;
}

int com_setupIOThreads(struct fig_ConfigData *config){
	char buff[BUFSIZ];
    int numThreads = config->threadsIO;

	// Setup data for the ClientList and then start its thread
	int leftOver = config->clients % numThreads; // Get remaining spots for each thread
    int ret = 0;
	for(int i = 0; i < numThreads; i++){
		clientList[i].maxClients = config->clients / numThreads;
		if(leftOver > 0){
			clientList[i].maxClients++;
			leftOver--;
		}
		clientList[i].threadNum = i;

        // Initialize the mutex to prevent locking issues
        ret = pthread_mutex_init(&clientList[i].clientListMutex, NULL);
        if(ret < 0){
            log_logError("Error initalizing pthread_mutex", ERROR);
            return -1;
        }

		ret = pthread_create(&clientList[i].thread, NULL, com_communicateWithClients, &clientList[i]);
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

int com_acceptClients(){
	char buff[BUFSIZ];

	while(1){
		struct sockaddr_storage cliAddr;
		socklen_t cliAddrSize = sizeof(cliAddr);

		//Accept client's connection and log its IP
		struct com_SocketInfo newCli;
		int client = accept(serverSockAddr.socket, (struct sockaddr *)&cliAddr, &cliAddrSize);
		if(client < 0){
			log_logError("Error accepting client", WARNING);
			continue;
		}

		char ipstr[INET6_ADDRSTRLEN];
		if(!getHost(ipstr, cliAddr, serverSockAddr.addr.ss_family)){
			strncpy(buff, "New client connected from: ", ARRAY_SIZE(buff));
			strncat(buff, ipstr, ARRAY_SIZE(buff)-strlen(buff));
			log_logMessage(buff, INFO);
		}

		// Fill in newCli struct
		newCli.socket = client;
		memcpy(&newCli.addr, &cliAddr, cliAddrSize);

		int ret = com_insertClient(newCli, clientList, fig_Configuration.threadsIO);
		if(ret < 0){
			snprintf(buff, ARRAY_SIZE(buff), "Server is full");
			send(client, buff, strlen(buff), 0);
			close(client);
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
		strncpy(msg, "Error with getaddrinfo: ", ARRAY_SIZE(msg));
		strncat(msg, gai_strerror(ret), ARRAY_SIZE(msg)-strlen(msg));
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
			strncpy(msg, "Binded server to port ", ARRAY_SIZE(msg));
			strncat(msg, port, 6);
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
		strncpy(msg, "Listening to port ", ARRAY_SIZE(msg));
		strncat(msg, port, 6);
		log_logMessage(msg, INFO);
	}

	sockAddr->socket = sock;
	return sock;
}
