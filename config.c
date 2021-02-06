#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "config.h"
#include "logging.h"

//Used to convert string option to an integer
const char *options[] = {"port", "log", "enablelogging"};

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

void fig_parseLine(char *line, struct fig_ConfigData* data){
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
			for(int i = 1; i < numWords && i < ARRAY_SIZE(data->ports); i++){
				data->ports[i-1] = strtol(words[i], NULL, 10);
			}
			break;

		case 1:
			//log
			strncpy(data->logDirectory, words[1], ARRAY_SIZE(data->logDirectory));
			break;

		case 2:
			//enable logging
			fig_lowerString(words[1]);
			if(!strncmp(words[1], "true", MAX_STRLEN)){
				data->useFile = 1;
			} else {
				data->useFile = 0;
			}
			break;

	}
}

int fig_readConfig(char *path, struct fig_ConfigData* data){
	FILE* file;

	file = fopen(path, "r");
	if(!file){
		log_logError("Error opening config file", 4);
		return -1;
	}

	//read through every line
	char buff[BUFSIZ];
	while(fgets(buff, ARRAY_SIZE(buff), file)){
		fig_parseLine(buff, data);
	}

	fclose(file);
	return 0;
}