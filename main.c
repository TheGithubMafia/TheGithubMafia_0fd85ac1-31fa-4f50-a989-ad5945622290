#include <stdio.h>
#include <stdint.h>
#include "logging.h"
#include "config.h"

int main(){
	struct fig_ConfigData config = {0};
	fig_readConfig("example_config.conf", &config);
	log_editConfig(config.useFile, config.logDirectory);

	log_close();
	return 0;
}
