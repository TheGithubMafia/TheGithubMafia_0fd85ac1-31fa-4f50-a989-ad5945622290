#include <stdio.h>
#include <string.h>
#include "config.h"

int readConfig(char* path, struct configData* data){
	FILE file;
	
	strcpy(data->logPath, "log/");
	strcpy(data->ports, path);

	return 0;
}
