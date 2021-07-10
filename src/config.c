#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "config.h"
#include "logging.h"

//Used to convert string option to an integer
const char *options[] = {"port", "log", "enablelogging", "numiothreads", "numdatathreads", "numclients", "nicklength", "servername", "channelnamelength", "groupnamelength"};

// Struct to store all config data
struct fig_ConfigData fig_Configuration = {
	.logDirectory = "/var/log/boundless-server",
	.serverName = "example.boundless.chat",
	.useFile = 0,
	.port = 6667,
	.threadsIO = 1,
	.threadsDATA = 1,
	.clients = 20,
	.nickLen = 10,
	.chanNameLength = 200,
	.groupNameLength = 200
};

int init_config(char *dir){
    fig_readConfig(dir);

    return 1;
}

void fig_lowerString(char *str){
	for(int i = 0; i < (int) strlen(str); i++){
		str[i] = tolower(str[i]);
	}
}

int fig_splitWords(char *line, char words[10][MAX_STRLEN]){
	int startWord = 0, word = 0;
	for(size_t i = 0; i < strlen(line); i++){
		if(isspace(line[i])){
			for(size_t x = 0; x < i-startWord && x < MAX_STRLEN; x++){
				words[word][x] = line[x+startWord];

				if(line[x+startWord] == '#'){
					words[word][x] = '\0';
					if(x != 0){
						++word;
					}

					return word;
				}
			}	
			words[word][i-startWord] = '\0';

			//seek to the end of the current whitespace
			startWord = i+1;
			while(isspace(line[startWord]) && startWord < (int) strlen(line)){
				startWord++; 
			}
			i = startWord;
			++word;
		}
	}	

	return word;
}

void fig_parseLine(char *line, int lineNo){
	char words[10][MAX_STRLEN];		
	int numWords = fig_splitWords(line, words);

	if(numWords < 2){
		return;
	}

	int option = -1;
	fig_lowerString(words[0]);
	for(int i = 0; i < ARRAY_SIZE(options); i++){
		if(!strncmp(words[0], options[i], MAX_STRLEN)){
			option = i;
		}
	}

	//Execute based on the string's value (value is equal to index in option)
	errno = 0;
	int *val;
	switch (option) {
		case 1:
			//log
			strncpy(fig_Configuration.logDirectory, words[1], ARRAY_SIZE(fig_Configuration.logDirectory));
			break;

		case 7:
			//serverName
			strncpy(fig_Configuration.serverName, words[1], ARRAY_SIZE(fig_Configuration.serverName));
			break;

		case 2:
			//enable logging
			fig_lowerString(words[1]);
			if(!strncmp(words[1], "true", MAX_STRLEN)){
				fig_Configuration.useFile = 1;
			} else {
				fig_Configuration.useFile = 0;
			}
			break;

		case 3:
			//num io threads
			val = &fig_Configuration.threadsIO;
			goto edit_int;

		case 4:
			//num data threads
			val = &fig_Configuration.threadsDATA;
			goto edit_int;

		case 0:
			//port
			val = &fig_Configuration.port;
			goto edit_int;

		case 5:
			//max clients
			val = &fig_Configuration.clients;
			goto edit_int;

		case 6:
			//nicklen
			val = &fig_Configuration.nickLen;
			goto edit_int;

		case 8:
			//chanNameLength
			val = &fig_Configuration.chanNameLength;
			goto edit_int;

		case 9:
			//groupNameLength
			val = &fig_Configuration.groupNameLength;
			goto edit_int;

		edit_int:
			fig_editConfigInt(val, words[1], lineNo);	
			break;

	}
}

int fig_readConfig(char *path){
	FILE* file;

	file = fopen(path, "r");
	if(!file){
		log_logError("Error opening config file", 4);
		return -1;
	}

	//read through every line
	char buff[BUFSIZ];
	int lineNo = 0;
	while(fgets(buff, ARRAY_SIZE(buff), file)){
		fig_parseLine(buff, lineNo);
		lineNo++;
	}

	fclose(file);
	return 0;
}

// Will edit the given config int value based on the given value and determine if is valid
int fig_editConfigInt(int *orig, char *str, int lineNo){
	errno = 0;
	int val = strtol(str, NULL, 10);
	char buff[100];

	if(errno != 0){
		snprintf(buff, ARRAY_SIZE(buff), "Line %d: Error converting string to int!", lineNo);
		log_logError(buff, WARNING);
		return -1;
	}

	if(val <= 0){
		snprintf(buff, ARRAY_SIZE(buff), "Line %d: %d is invalid, using default %d.", lineNo, val, *orig);
		log_logError(buff, WARNING);
		return -1;
	}

	*orig = val; // Success
	return 1;
}
