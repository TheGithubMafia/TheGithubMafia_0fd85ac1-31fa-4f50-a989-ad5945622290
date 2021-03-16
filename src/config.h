#ifndef config_h
#define config_h

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "logging.h"

#define ARRAY_SIZE(arr) (int)(sizeof(arr)/sizeof((arr)[0]))
#define MAX_STRLEN (128)


//struct that will store all configuration data
struct fig_ConfigData {
	char logDirectory[BUFSIZ];
	int useFile;
	int port;
	int threads;
	int clients;
};	

// Struct to store all config data
extern struct fig_ConfigData fig_Configuration;

int init_config(char *dir);

void fig_lowerString(char *str);

//split a line into its individual words
//return value is the number of words
//will stop splitting if detects a '#' character
int fig_splitWords(char *line, char words[10][MAX_STRLEN]);

void fig_parseLine(char *line);

//read a configuration file into the fig_ConfigData struct
int fig_readConfig(char *path);

#endif
