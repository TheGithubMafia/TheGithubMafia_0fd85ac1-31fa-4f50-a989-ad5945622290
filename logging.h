#pragma once

//Time format: [YYYY-MM-DD HH-MM-SS]
int getTime(char str[22]);

//Time format: YYYY-MM-DD
int getTimeShort(char str[11]);

//Write to log file formatted
int logToFile(char* msg, int type, char* dir);

//Give message + strerror
int printLogError(char *msg);

int printLogFormat(char *msg, int type);

//create string in for [time] - [type of message] - message
int createLogFormat(char* buffer, char* msg, int type);

//print to stdout and log to file
int logMessage(char* msg, int type, int useFile);
