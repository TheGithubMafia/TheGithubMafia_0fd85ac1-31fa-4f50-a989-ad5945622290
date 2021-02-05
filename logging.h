#ifndef logging
#define logging

//Time format: [YYYY-MM-DD HH-MM-SS]
int getTime(char str[22]);

//Time format: YYYY-MM-DD
int getTimeShort(char str[11]);

//Write to log file formatted
int logToFile(char* msg, int type, char* dir);

void printLogFormat(char *msg, int type);

//create string in for [time] - [type of message] - message
void createLogFormat(char* buffer, char* msg, int type);

//same as logMessage, but will append strerror at end of the string
int logError(char* msg, int type, int useFile);

//print to stdout and log to file
int logMessage(char* msg, int type, int useFile);

#endif
