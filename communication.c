#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "communication.h"
#include "logging.h"
#include "config.h"

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

int com_acceptClients(struct com_SocketInfo* sockAddr){
	int protocol = sockAddr->addr.ss_family, sock = sockAddr->socket;
	struct sockaddr_storage cliAddr;
	socklen_t cliAddrSize = sizeof(cliAddr);

	//Accept client's connection and log its IP
	int client = accept(sock, (struct sockaddr *)&cliAddr, &cliAddrSize);
	if(client < 0){
		log_logError("Error accepting client", WARNING);
		return -1;
	} else {
		char ipstr[INET6_ADDRSTRLEN];
		if(!getHost(ipstr, cliAddr, protocol)){
			char msg[BUFSIZ];
			strncpy(msg, "New client connected from: ", ARRAY_SIZE(msg));
			strncat(msg, ipstr, ARRAY_SIZE(msg)-strlen(msg));
			log_logMessage(msg, MESSAGE);
		}
	}

	char buffer[BUFSIZ];
	read(client, buffer, ARRAY_SIZE(buffer));
	log_logMessage(buffer, MESSAGE);
	send(client, buffer, strlen(buffer), 0);

	close(client);
	protocol++;
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
