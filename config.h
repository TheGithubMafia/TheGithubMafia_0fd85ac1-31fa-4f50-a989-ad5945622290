#ifndef config
#define config

//struct that will store all configuration data
struct configData {
	char logPath[1024];
	char ports[128];
};	

int readConfig(char* path, struct configData* data);

#endif
