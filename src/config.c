#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "config.h"
#include "logging.h"

//Used to convert string option to an integer
const char *options[] = {"port", "log", "enablelogging", "numthreads", "numclients"};

// Struct to store all config data
struct fig_ConfigData fig_Configuration;

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

void fig_parseLine(char *line){
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
	switch (option) {
		case 0:
			//port
			errno = 0;
			fig_Configuration.port = strtol(words[1], NULL, 10);
			if(errno != 0){
				log_logError("Error converting string to int: port", WARNING);
				fig_Configuration.port = 0;
			}
			break;

		case 1:
			//log
			strncpy(fig_Configuration.logDirectory, words[1], ARRAY_SIZE(fig_Configuration.logDirectory));
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
			//num threads
			errno = 0;
			fig_Configuration.threads = strtol(words[1], NULL, 10);
			if(errno != 0){
				log_logError("Error converting string to int: threads", WARNING);
				fig_Configuration.threads = 0;
			}
			break;

		case 4:
			//max clients
			errno = 0;
			fig_Configuration.clients = strtol(words[1], NULL, 10);
			if(errno != 0){
				log_logError("Error converting string to int: clients", WARNING);
				fig_Configuration.clients = 0;
			}
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
	while(fgets(buff, ARRAY_SIZE(buff), file)){
		fig_parseLine(buff);
	}

	fclose(file);
	return 0;
}
