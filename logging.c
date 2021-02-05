#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include "logging.h"

const char* LOG_DEFAULT_LOGGING_DIR = "/var/log/irc-server/";

FILE* log_LogFile;
struct log_Config log_LoggingConfig = {0};

void log_editConfig(int useFile, char* dir){
	if(log_LoggingConfig.useFile != useFile){
		log_LoggingConfig.useFile = useFile;

		if(!useFile){
			log_printLogFormat("Disabled Logging to a file", 1);
		} else {
			log_printLogFormat("Enabled Logging to a file", 1);
		}
	}

	if(dir == NULL)
		return;
	if(strcmp(log_LoggingConfig.directory, dir) != 0){
		strcpy(log_LoggingConfig.directory, dir);

		char msg[1100];
		strcpy(msg, "Changing logging directory to ");
		strcat(msg, log_LoggingConfig.directory);
		log_printLogFormat(msg, 1);

		if(log_LogFile)
			fclose(log_LogFile);
	}
}	

int log_getTime(char str[22]){
	time_t rawtime;
	time(&rawtime);
	struct tm *time = localtime(&rawtime);
	
	snprintf(str, 22, "[%04d-%02d-%02d %02d:%02d:%02d]", time->tm_year+1900, time->tm_mon+1, time->tm_mday, time->tm_hour, time->tm_min, time->tm_sec);

	return 0;
}

int log_getTimeShort(char str[11]){
	time_t rawtime;
	time(&rawtime);
	struct tm *time = localtime(&rawtime);
	
	snprintf(str, 11, "%04d-%02d-%02d", time->tm_year+1900, time->tm_mon+1, time->tm_mday);

	return 0;
}

int log_logToFile(char* msg, int type){
	//check to see if log file is already open
	if(!log_LogFile){

		//get file location for this log
		char fileLoc[1024];
		char endChar = log_LoggingConfig.directory[strlen(log_LoggingConfig.directory) - 1];
		strcpy(fileLoc, log_LoggingConfig.directory);
		if(endChar != '\\' || endChar != '/')
			strcat(fileLoc, "/");

		//YYYY-MM-DD.log filename
		char time[11];
		log_getTimeShort(time);
		strcat(fileLoc, time);
		strcat(fileLoc, ".log");

		log_LogFile = fopen(fileLoc, "a+");	
		if(!log_LogFile){
			log_printLogError("Error opening log file", 3);
			log_editConfig(0, log_LoggingConfig.directory);
			return -1;
		}
	}

	char formattedMsg[1024];
	log_createLogFormat(formattedMsg, msg, type);
	if(fprintf(log_LogFile, "%s\n", formattedMsg) < 0){
		log_printLogError("Error writing to log file", 3);
		log_editConfig(0, log_LoggingConfig.directory);
		return -1;
	}

	return 0;
}

void log_printLogError(char* msg, int type){
	char fullMsg[1024];
	strcpy(fullMsg, msg);
	strcat(fullMsg, ": ");
	strcat(fullMsg, strerror(errno));
	
	log_printLogFormat(fullMsg, type);
}

void log_printLogFormat(char *msg, int type){
	char formattedMsg[1024];

	log_createLogFormat(formattedMsg, msg, type);
	printf("%s\n", formattedMsg);
}	

void log_createLogFormat(char* buffer, char* msg, int type){
	char time[22] = {0};
	log_getTime(time);

	char formattedType[16];
	char* typeStr;
	switch (type) {
		case 0:
			typeStr = "TRACE";
			break;

		case 1:
			typeStr = "DEBUG";
			break;

		case 2:
			typeStr = "INFO";
			break;

		case 3:
			typeStr = "WARNING";
			break;

		case 4:
			typeStr = "ERROR";
			break;

		case 5:
			typeStr = "FATAL";
			break;

		case 6:
			typeStr = "MESSAGE";
			break;

	}
	
	snprintf(formattedType, sizeof(formattedType)/sizeof(char), " - [%s] - ", typeStr);

	strcpy(buffer, time);
	strcat(buffer, formattedType);
	strcat(buffer, msg);
}

int log_logError(char* msg, int type){
	char fullMsg[1024];
	strcpy(fullMsg, msg);
	strcat(fullMsg, ": ");
	strcat(fullMsg, strerror(errno));

	return log_logMessage(fullMsg, type);
}

int log_logMessage(char* msg, int type){
	log_printLogFormat(msg, type);

	if(log_LoggingConfig.useFile){
		if(log_logToFile(msg, type) < 0){
			return -1;
		}
	}
		
	return 0;
}
