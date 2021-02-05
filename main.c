#include <stdio.h>
#include <stdint.h>
#include "logging.h"

int main(){
	printLogError("Hello");
	printLogFormat("Started system", 0);
	logMessage("Shutting down system", 0, 1);

	return 0;
}
