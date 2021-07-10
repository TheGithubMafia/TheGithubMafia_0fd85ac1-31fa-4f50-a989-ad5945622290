#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include "logging.h"
#include "config.h"
#include "communication.h"
#include "linkedlist.h"
#include "chat.h"
#include "commands.h"

void cleanUpServer(){
	log_logMessage("Server is now quitting.", INFO);

	log_close();
	com_close();
	chat_close();
}

int main(){
    atexit(cleanUpServer);

    if(init_config("example_config.conf") == -1) /* config.h */
		return -1;
    if(init_logging() == -1) /* logging.h */
		return -1;
    if(init_server() == -1) /* communication.h */
		return -1;
    if(init_commands() == -1) /* commands.h */
		return -1;
    if(init_chat() == -1) /* chat.h */
		return -1;

    com_acceptClients();

    return 0;
}
