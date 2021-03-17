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
    atexit(com_close);
    atexit(chat_close);

    init_config("example_config.conf"); /* config.h */
    init_logging(); /* logging.h */
    init_server(); /* communication.h */
    init_chat();

	//com_acceptClients(&sockAddr, &fig_Configuration);

	return 0;
}
