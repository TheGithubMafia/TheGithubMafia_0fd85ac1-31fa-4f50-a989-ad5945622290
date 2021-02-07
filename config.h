#include <stdio.h>
#ifndef config_h
#define config_h

#define ARRAY_SIZE(arr) (int)(sizeof(arr)/sizeof((arr)[0]))
#define MAX_STRLEN (128)

//struct that will store all configuration data
struct fig_ConfigData {
	char logDirectory[BUFSIZ];
	int useFile;
	int port;
};	

void fig_lowerString(char *str);

//split a line into its individual words
//return value is the number of words
//will stop splitting if detects a '#' character
int fig_splitWords(char *line, char words[10][MAX_STRLEN]);

void fig_parseLine(char *line, struct fig_ConfigData* data);

//read a configuration file into the fig_ConfigData struct
int fig_readConfig(char *path, struct fig_ConfigData* data);

#endif
