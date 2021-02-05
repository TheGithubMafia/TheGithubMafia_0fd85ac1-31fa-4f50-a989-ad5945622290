#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include "logging.h"

const char* defaultLogDir = "/var/log/irc-server/";

//Time format: [YYYY-MM-DD HH-MM-SS]
int getTime(char str[22]){
	time_t rawtime;
	time(&rawtime);
	struct tm *time = localtime(&rawtime);
	
	snprintf(str, 22, "[%04d-%02d-%02d %02d:%02d:%02d]", time->tm_year+1900, time->tm_mon+1, time->tm_mday, time->tm_hour, time->tm_min, time->tm_sec);

	return 0;
}

//Time format: YYYY-MM-DD
int getTimeShort(char str[11]){
	time_t rawtime;
	time(&rawtime);
	struct tm *time = localtime(&rawtime);
	
	snprintf(str, 11, "%04d-%02d-%02d", time->tm_year+1900, time->tm_mon+1, time->tm_mday);

	return 0;
}

//Will write to log directory formatted 
int logToFile(char* msg, int type, char* dir){
	FILE *log;

	if(!dir){
		dir = (char *) defaultLogDir;
	}

	//get file location for this log
	char fileLoc[1024];
	char endChar = dir[strlen(dir) - 1];
	strcpy(fileLoc, dir);
	if(endChar != '\\' || endChar != '/')
		strcat(fileLoc, "/");

	//YYYY-MM-DD.log filename
	char time[11];
	getTimeShort(time);
	strcat(fileLoc, time);
	strcat(fileLoc, ".log");

	log = fopen(fileLoc, "a+");	
	if(!log){
		logError("Error opening log file", 2, 0);
		return -1;
	}

	char formattedMsg[1024];
	createLogFormat(formattedMsg, msg, type);
	if(fprintf(log, "%s\n", formattedMsg) < 0){
		logError("Error writing to log file", 2, 0);
		return -1;
	}

	return 0;
}

void printLogFormat(char *msg, int type){
	char formattedMsg[1024];

	createLogFormat(formattedMsg, msg, type);
	printf("%s\n", formattedMsg);
}	

//create string in form [time] - [Type of message] - message
// 0 = INFO
// 1 = WARNING
// 2 = ERROR
// 3 = FATAL
void createLogFormat(char* buffer, char* msg, int type){
	char time[22] = {0};
	getTime(time);

	char formattedType[16];
	char* typeStr;
	switch (type) {
		case 0:
			typeStr = "INFO";
			break;

		case 1:
			typeStr = "WARNING";
			break;

		case 2:
			typeStr = "ERROR";
			break;

		case 3:
			typeStr = "FATAL";
			break;
	}
	
	snprintf(formattedType, sizeof(formattedType)/sizeof(char), " - [%s] - ", typeStr);

	strcpy(buffer, time);
	strcat(buffer, formattedType);
	strcat(buffer, msg);
}

//same as logMessage, but will append the strerror at end of string
int logError(char* msg, int type, int useFile){
	char fullMsg[1024];
	strcpy(fullMsg, msg);
	strcat(fullMsg, ": ");
	strcat(fullMsg, strerror(errno));

	return logMessage(fullMsg, type, useFile);
}

//print to stdout and log to file
int logMessage(char* msg, int type, int useFile){
	printLogFormat(msg, type);

	if(useFile){
		if(logToFile(msg, type, NULL) < 0){
			return -1;
		}
	}
		
	return 0;
}
