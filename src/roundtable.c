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
	log_logMessage("Now starting boundless.chat server: V1.1.0.", INFO);
    atexit(cleanUpServer);

    if(init_config("example_config.conf") == -1) /* config.h */
		return -1;
    if(init_logging() == -1) /* logging.h */
		return -1;
    if(init_chat() == -1) /* chat.h */
		return -1;
    if(init_server() == -1) /* communication.h */
		return -1;
    if(init_commands() == -1) /* commands.h */
		return -1;

	while(1);

    return 0;
}
