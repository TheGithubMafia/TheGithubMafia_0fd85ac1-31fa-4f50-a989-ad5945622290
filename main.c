#include <stdio.h>
#include <stdint.h>
#include "logging.h"

int main(){
	log_editConfig(1, NULL);
	log_logMessage("Starting program", 0);
	log_editConfig(1, "/var/log/irc-server");
	log_logMessage("YOLO", 5);

	return 0;
}
