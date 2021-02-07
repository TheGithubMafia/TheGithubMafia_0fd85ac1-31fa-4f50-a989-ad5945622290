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
			log_printLogFormat("Disabled Logging to a file", INFO);
		} else {
			log_printLogFormat("Enabled Logging to a file", INFO);
		}
	}

	if(dir == NULL)
		return;
	if(strcmp(log_LoggingConfig.directory, dir) != 0){
		strncpy(log_LoggingConfig.directory, dir, ARRAY_SIZE(log_LoggingConfig.directory)-1);
		log_LoggingConfig.directory[ARRAY_SIZE(log_LoggingConfig.directory)-1] = '\0';

		char msg[BUFSIZ];
		strcpy(msg, "Changing logging directory to ");
		strcat(msg, log_LoggingConfig.directory);
		log_printLogFormat(msg, INFO);

		if(log_LogFile)
			fclose(log_LogFile);
	}
}	

void log_close(){
	if(log_LogFile){
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
		char fileLoc[BUFSIZ];
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
			log_printLogError("Error opening log file", ERROR);
			log_editConfig(0, log_LoggingConfig.directory);
			return -1;
		}
	}

	char formattedMsg[BUFSIZ];
	log_createLogFormat(formattedMsg, msg, type);
	if(fprintf(log_LogFile, "%s\n", formattedMsg) < 0){
		log_printLogError("Error writing to log file", ERROR);
		log_editConfig(0, log_LoggingConfig.directory);
		return -1;
	}

	return 0;
}

void log_printLogError(char* msg, int type){
	char fullMsg[BUFSIZ];
	strcpy(fullMsg, msg);
	strcat(fullMsg, ": ");
	strcat(fullMsg, strerror(errno));
	
	log_printLogFormat(fullMsg, type);
}

void log_printLogFormat(char *msg, int type){
	char formattedMsg[BUFSIZ];

	log_createLogFormat(formattedMsg, msg, type);
	printf("%s\n", formattedMsg);
}	

void log_createLogFormat(char* buffer, char* msg, int type){
	char time[22] = {0};
	log_getTime(time);

	char formattedType[16];
	char* typeStr;
	switch (type) {
		case TRACE:
			typeStr = "TRACE";
			break;

		case DEBUG:
			typeStr = "DEBUG";
			break;

		case INFO:
			typeStr = "INFO";
			break;

		case WARNING:
			typeStr = "WARNING";
			break;

		case ERROR:
			typeStr = "ERROR";
			break;

		case FATAL:
			typeStr = "FATAL";
			break;

		case MESSAGE:
			typeStr = "MESSAGE";
			break;

	}
	
	snprintf(formattedType, sizeof(formattedType)/sizeof(char), " - [%s] - ", typeStr);

	strcpy(buffer, time);
	strcat(buffer, formattedType);
	strcat(buffer, msg);
}

int log_logError(char* msg, int type){
	char fullMsg[BUFSIZ];
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
