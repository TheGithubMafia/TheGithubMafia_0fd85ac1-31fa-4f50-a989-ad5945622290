#include "security.h"

// Compares two strings in constant time for security
int sec_constantStrCmp(char *first, char *second, int length){
	int equal = 1;
	int done = 0;
	for(int i = 0; i < length; i++){
		if(first[i] != second[i] && done != 1){
			equal = -1;
		}

		if(first[i] == '\0' || second[i] == '\0'){
			done = 1;
		}
	}

	return equal;
}
