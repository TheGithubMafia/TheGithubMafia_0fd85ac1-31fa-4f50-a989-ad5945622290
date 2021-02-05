#include <stdio.h>
#include <stdint.h>
#include "logging.h"

int main(){
	logMessage("Starting program", 0, 1);

	struct configData data;
	readConfig("NO", &data);
	printf(data.ports);

	return 0;
}
