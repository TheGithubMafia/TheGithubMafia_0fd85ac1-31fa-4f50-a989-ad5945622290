#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "communication.h"
#include "logging.h"
#include "config.h"

int com_startServerSocket(struct fig_ConfigData* data, int forceIPv4){
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
		if(!forceIPv4){
			log_logMessage(msg, ERROR);
			log_logMessage("Retrying with iPv4...", 2);
			return com_startServerSocket(data, 1);
		} else {
			log_logMessage(msg, FATAL);
			exit(EXIT_FAILURE);
		}
	}

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

	freeaddrinfo(res);

	if(rp == NULL){
		if(!forceIPv4){
			log_logError("No addresses sucessfully binded IPv6!", ERROR);
			log_logMessage("Retrying with iPv4...", 2);
			return com_startServerSocket(data, 1);
		} else {
			log_logError("No addresses sucessfully binded IPv4!", FATAL);
			exit(EXIT_FAILURE);
		}
	}

	ret = listen(sock, 128);
	if(ret != 0){
		log_logError("Error listening on socket", FATAL);
		exit(EXIT_FAILURE);
	} else {
		char msg[BUFSIZ];
		strncpy(msg, "Listening to port ", ARRAY_SIZE(msg));
		strncat(msg, port, 6);
		log_logMessage(msg, INFO);
	}

	return sock;
}
