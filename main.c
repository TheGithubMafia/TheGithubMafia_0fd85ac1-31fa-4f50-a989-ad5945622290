#include <stdio.h>
#include <stdint.h>
#include "logging.h"

int main(){
	char time[22];
	getTime(time);
	printf("%s\n", time);
}
