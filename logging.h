#ifndef logging
#define logging

/* Struct that defines how logging should be carried out
   useFile determines if logs will go to a file or not
   directory specifies the directory that the log files will go 
   
   ONLY EDIT THIS STRUCT WITH THE log_editConfig() METHOD */
struct log_Config {
	int useFile;
	char directory[1024];
};

//edits the log_Config struct, also logs changes it makes
void log_editConfig(int useFule, char* dir);

//cleans up the logging for termination of the program
void log_close();

//Time format: [YYYY-MM-DD HH-MM-SS]
int log_getTime(char str[22]);

//Time format: YYYY-MM-DD
int log_getTimeShort(char str[11]);

//Write to log file formatted
int log_logToFile(char* msg, int type);

//Print to stdin with sterror format
void log_printLogError(char* msg, int type);

//Only print to stdin using the format
void log_printLogFormat(char *msg, int type);

//create string in for [time] - [type of message] - message
// 0 = TRACE
// 1 = DEBUG
// 2 = INFO
// 3 = WARNING
// 4 = ERROR
// 5 = FATAL
// 6 = MESSAGE
void log_createLogFormat(char* buffer, char* msg, int type);

//same as logMessage, but will append strerror at end of the string
int log_logError(char* msg, int type);

//print to stdout and log to file
int log_logMessage(char* msg, int type);

#endif
