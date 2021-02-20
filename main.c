#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include "logging.h"
#include "config.h"
#include "communication.h"
#include "linkedlist.h"

int main(){
	atexit(log_close);

	/*
	struct fig_ConfigData config = {0};
	fig_readConfig("example_config.conf", &config);
	log_editConfig(config.useFile, config.logDirectory);

	struct com_SocketInfo sockAddr;
	int sock = com_startServerSocket(&config, &sockAddr, 0);
	if(sock < 0){
		log_logMessage("Retrying with IPv4...", INFO);
		sock = com_startServerSocket(&config, &sockAddr, 1);
		if(sock < 0){
			return -1;
		}
	}
	com_acceptClients(&sockAddr, &config);

	close(sock);
	*/

	int *p;
	int var = 10;
	p = &var;
	struct link_List list = {0};
	link_add(&list, p);
	*p += 10;
	link_add(&list, p);
	*p += 10;
	link_add(&list, p);

	struct link_Node *node = link_getNode(&list, 1);
	if(!node){
		printf("DEAD\n");
		return -1;
	}
	
	printf("M %p\n", list.head);
	printf("%p\n", node);
	printf("%d 1\n", *((int *)node->data));

	return 0;
}
